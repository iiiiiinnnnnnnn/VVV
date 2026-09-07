// SoundSystem.cpp
#include "Audio/SoundSystem.h"
#include "Audio/VSoundFormat.h"

#include "Resource/ResourceManager.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Scene/SceneManager.h"

#include <xaudio2fx.h>
#include <compressapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>
#include <windows.h>

namespace
{
constexpr UINT32 SpatialSampleRate = 48000;

// 現在のカメラを聞き手にする
const Camera* GetListenerCamera()
{
	Scene* scene = SceneManager::Instance().GetCurrentScene();
	Stage* stage = scene ? scene->GetCurrentStage() : nullptr;
	const Camera* camera = stage ? stage->GetActiveCamera() : nullptr;
	return camera && camera->IsActive() ? camera : nullptr;
}

// 不正な3D設定を除外
bool IsValidSpatialOptions(const SoundSystem::SpatialOptions& options)
{
	return std::isfinite(options.volume) && options.volume > 0.0f &&
		std::isfinite(options.pitch) && options.pitch > 0.0f &&
		std::isfinite(options.minDistance) && options.minDistance >= 0.0f &&
		std::isfinite(options.maxDistance) && options.maxDistance > options.minDistance &&
		std::isfinite(options.lowPassHz) && options.lowPassHz > 0.0f &&
		std::isfinite(options.farLowPassHz) && options.farLowPassHz > 0.0f &&
		std::isfinite(options.reverbMix) && options.reverbMix >= 0.0f && options.reverbMix <= 1.0f;
}

// 可聴範囲の境界で滑らかに減衰
float GetDistanceGain(float distance, const SoundSystem::SpatialOptions& options)
{
	if (!std::isfinite(distance) || distance >= options.maxDistance) return 0.0f;
	if (distance <= options.minDistance) return 1.0f;
	const float t = (distance - options.minDistance) / (options.maxDistance - options.minDistance);
	return (1.0f - t) * (1.0f - t) * (1.0f + 2.0f * t);
}

// 座標をX3DAudio形式に変換
X3DAUDIO_VECTOR ToAudioVector(const Vector3& value)
{
	return {value.x, value.y, value.z};
}

// RIFF識別子を比較
bool IsFourCC(const std::array<char, 4>& value, const char (&expected)[5])
{
	return std::memcmp(value.data(), expected, value.size()) == 0;
}

// 音声エラーをデバッガーに出力
void ReportSoundError(const std::string& message)
{
	const std::string output = "[SoundSystem] " + message + "\n";
	OutputDebugStringA(output.c_str());
}

// キャッシュ用にパスを正規化
std::string MakeSoundKey(const std::filesystem::path& path)
{
	std::string key = path.lexically_normal().generic_string();
	std::transform(key.begin(), key.end(), key.begin(),
		[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	return key;
}
} // 無名名前空間

struct SoundSystem::SoundData
{
	WAVEFORMATEXTENSIBLE format{};
	std::vector<uint8_t> samples;
};

// ソースボイスを停止・破棄
SoundSystem::ActiveVoice::~ActiveVoice()
{
	if (!source) return;
	source->Stop(0);
	source->FlushSourceBuffers();
	source->DestroyVoice();
}

// 終了処理漏れに備えて解放
SoundSystem::~SoundSystem()
{
	Finalize();
}

// XAudio2とマスタリングボイスを初期化
bool SoundSystem::Initialize()
{
	if (IsInitialized()) return true;
	Finalize();

	HRESULT result = XAudio2Create(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
	if (FAILED(result))
	{
		xaudio = nullptr;
		ReportSoundError("XAudio2Create failed: " + std::to_string(result));
		return false;
	}

	result = xaudio->CreateMasteringVoice(&masteringVoice);
	if (FAILED(result))
	{
		ReportSoundError("CreateMasteringVoice failed: " + std::to_string(result));
		xaudio->Release();
		xaudio = nullptr;
		masteringVoice = nullptr;
		return false;
	}

	XAUDIO2_VOICE_DETAILS details{};
	masteringVoice->GetVoiceDetails(&details);
	outputChannels = details.InputChannels;
	DWORD channelMask = 0;
	result = masteringVoice->GetChannelMask(&channelMask);
	if (SUCCEEDED(result)) result = X3DAudioInitialize(channelMask, X3DAUDIO_SPEED_OF_SOUND, spatialHandle);
	if (FAILED(result))
	{
		ReportSoundError("X3DAudioInitialize failed: " + std::to_string(result));
		Finalize();
		return false;
	}
	masteringVoice->SetVolume(masterVolume);
	return true;
}

// 音声リソースを解放
void SoundSystem::Finalize()
{
	StopAll();
	sounds.clear();
	if (masteringVoice)
	{
		masteringVoice->DestroyVoice();
		masteringVoice = nullptr;
	}
	if (xaudio)
	{
		xaudio->Release();
		xaudio = nullptr;
	}
	outputChannels = 0;
	nextVoiceId = 1;
}

// 位置・音量の更新と終了音の解放
void SoundSystem::Update()
{
	const Camera* listener = nullptr;
	bool listenerResolved = false;
	for (auto it = voices.begin(); it != voices.end();)
	{
		ActiveVoice& voice = *it->second;
		bool remove = voice.callback->finished.load();
		if (!remove && voice.isSpatial)
		{
			if (!listenerResolved)
			{
				listener = GetListenerCamera();
				listenerResolved = true;
			}
			if (!listener) remove = true;
			else if (voice.usesFixedPosition)
				remove = !ApplySpatial(voice, voice.fixedPosition, *listener);
			else
			{
				const auto emitter = voice.emitter.lock();
				remove = !emitter || !emitter->IsActive() || emitter->IsPendingDestroy();
				if (!remove) remove = !ApplySpatial(voice, emitter->transform.position, *listener);
			}
		}
		if (remove) it = voices.erase(it);
		else ++it;
	}
}

// WAVを再生してボイスIDを返す
SoundSystem::VoiceId SoundSystem::Play(const std::string& path, const PlayOptions& options)
{
	return StartVoice(path, options, nullptr, nullptr, nullptr);
}

// Actorに追従して再生、範囲外なら無効ID
SoundSystem::VoiceId SoundSystem::Play3D(const std::string& path, Actor* emitter, const SpatialOptions& options)
{
	if (!emitter || !emitter->IsActive() || emitter->IsPendingDestroy() || !IsValidSpatialOptions(options))
		return InvalidVoiceId;
	if (emitter->weak_from_this().expired())
	{
		ReportSoundError("Play3D requires a shared_ptr-managed Actor");
		return InvalidVoiceId;
	}
	const Camera* listener = GetListenerCamera();
	if (!listener) return InvalidVoiceId;
	const float distance = Vector3::Distance(emitter->transform.position, listener->GetEye());
	if (GetDistanceGain(distance, options) <= 0.0f) return InvalidVoiceId;
	return StartVoice(path, options, emitter, &options, listener);
}

SoundSystem::VoiceId SoundSystem::PlayTrack(
	int track, int variant, const PlayOptions& options)
{
	auto sound = LoadTrackSound(track, variant);
	return sound ? StartVoiceData("track " + std::to_string(track), std::move(sound),
		options, nullptr, nullptr, nullptr) : InvalidVoiceId;
}

SoundSystem::VoiceId SoundSystem::PlayTrack3D(
	int track, Actor* emitter, int variant, const SpatialOptions& options)
{
	if (!emitter || !emitter->IsActive() || emitter->IsPendingDestroy() ||
		!IsValidSpatialOptions(options)) return InvalidVoiceId;
	if (emitter->weak_from_this().expired())
	{
		ReportSoundError("PlayTrack3D requires a shared_ptr-managed Actor");
		return InvalidVoiceId;
	}
	const Camera* listener = GetListenerCamera();
	if (!listener) return InvalidVoiceId;
	const float distance = Vector3::Distance(emitter->transform.position, listener->GetEye());
	if (GetDistanceGain(distance, options) <= 0.0f) return InvalidVoiceId;
	auto sound = LoadTrackSound(track, variant);
	return sound ? StartVoiceData("track " + std::to_string(track), std::move(sound),
		options, emitter, &options, listener) : InvalidVoiceId;
}

SoundSystem::VoiceId SoundSystem::PlayTrack3DAt(
	int track, const Vector3& position, int variant, const SpatialOptions& options)
{
	if (!IsValidSpatialOptions(options)) return InvalidVoiceId;
	const Camera* listener = GetListenerCamera();
	if (!listener || GetDistanceGain(Vector3::Distance(position, listener->GetEye()), options) <= 0.0f)
		return InvalidVoiceId;
	auto sound = LoadTrackSound(track, variant);
	return sound ? StartVoiceData("track " + std::to_string(track), std::move(sound),
		options, nullptr, &options, listener, &position) : InvalidVoiceId;
}

// ボイスを作成して再生
SoundSystem::VoiceId SoundSystem::StartVoice(const std::string& path, const PlayOptions& options,
	Actor* emitter, const SpatialOptions* spatial, const Camera* listener,
	const Vector3* fixedPosition)
{
	if (!IsInitialized() && !Initialize()) return InvalidVoiceId;
	if (spatial && !InitializeReverb()) return InvalidVoiceId;

	std::shared_ptr<SoundData> sound = LoadSound(path);
	if (!sound) return InvalidVoiceId;
	return StartVoiceData(path, std::move(sound), options, emitter, spatial, listener, fixedPosition);
}

SoundSystem::VoiceId SoundSystem::StartVoiceData(const std::string& name,
	std::shared_ptr<SoundData> sound, const PlayOptions& options, Actor* emitter,
	const SpatialOptions* spatial, const Camera* listener, const Vector3* fixedPosition)
{
	if (!IsInitialized() && !Initialize()) return InvalidVoiceId;
	if (spatial && !InitializeReverb()) return InvalidVoiceId;
	if (!sound) return InvalidVoiceId;

	auto active = std::make_unique<ActiveVoice>();
	active->sound = sound;
	active->callback = std::make_unique<VoiceCallback>();
	active->volume = std::clamp(options.volume, 0.0f, XAUDIO2_MAX_VOLUME_LEVEL);
	active->isSpatial = spatial != nullptr;
	if (spatial)
	{
		active->usesFixedPosition = fixedPosition != nullptr;
		if (fixedPosition) active->fixedPosition = *fixedPosition;
		else active->emitter = emitter->weak_from_this();
		active->spatial = *spatial;
		const uint32_t channels = sound->format.Format.nChannels;
		active->matrix.resize(channels * outputChannels);
		active->monoMatrix.resize(outputChannels);
		active->reverbMatrix.resize(channels, spatial->reverbMix / channels);
	}

	XAUDIO2_SEND_DESCRIPTOR sends[] = {{0, spatial ? static_cast<IXAudio2Voice*>(spatialVoice) : masteringVoice},
		{0, reverbVoice}};
	const XAUDIO2_VOICE_SENDS sendList = {spatial && spatial->reverbMix > 0.0f ? 2u : 1u, sends};
	HRESULT result = xaudio->CreateSourceVoice(&active->source, &sound->format.Format,
		spatial ? XAUDIO2_VOICE_USEFILTER : 0, XAUDIO2_MAX_FREQ_RATIO, active->callback.get(), &sendList);
	if (FAILED(result))
	{
		ReportSoundError("CreateSourceVoice failed: " + name);
		return InvalidVoiceId;
	}

	XAUDIO2_BUFFER buffer{};
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	buffer.AudioBytes = static_cast<UINT32>(sound->samples.size());
	buffer.pAudioData = sound->samples.data();
	buffer.LoopCount = options.loop ? XAUDIO2_LOOP_INFINITE : 0;
	result = active->source->SubmitSourceBuffer(&buffer);
	if (FAILED(result))
	{
		ReportSoundError("SubmitSourceBuffer failed: " + name);
		return InvalidVoiceId;
	}

	active->source->SetFrequencyRatio(
		std::clamp(options.pitch, XAUDIO2_MIN_FREQ_RATIO, XAUDIO2_MAX_FREQ_RATIO));
	if (spatial)
	{
		const Vector3 emitterPosition = fixedPosition ? *fixedPosition : emitter->transform.position;
		if (!ApplySpatial(*active, emitterPosition, *listener)) return InvalidVoiceId;
		if (spatial->reverbMix > 0.0f && FAILED(active->source->SetOutputMatrix(reverbVoice,
			sound->format.Format.nChannels, 1, active->reverbMatrix.data()))) return InvalidVoiceId;
	}
	else
	{
		active->source->SetVolume(active->volume);
		ApplyPan(*active, options.pan);
	}

	result = active->source->Start(0);
	if (FAILED(result))
	{
		ReportSoundError("SourceVoice Start failed: " + name);
		return InvalidVoiceId;
	}

	VoiceId voiceId = nextVoiceId++;
	if (voiceId == InvalidVoiceId) voiceId = nextVoiceId++;
	voices.emplace(voiceId, std::move(active));
	return voiceId;
}

// ボイスを停止・解放
void SoundSystem::Stop(VoiceId voiceId)
{
	voices.erase(voiceId);
}

// 全ボイスを停止・解放
void SoundSystem::StopAll()
{
	voices.clear();
	if (spatialVoice)
	{
		spatialVoice->DestroyVoice();
		spatialVoice = nullptr;
	}
	if (reverbVoice)
	{
		reverbVoice->DestroyVoice();
		reverbVoice = nullptr;
	}
}

// 現在のActorとカメラで更新
bool SoundSystem::UpdateSpatialVoice(ActiveVoice& voice)
{
	const Camera* listener = GetListenerCamera();
	if (!listener) return false;
	if (voice.usesFixedPosition) return ApplySpatial(voice, voice.fixedPosition, *listener);
	const auto emitter = voice.emitter.lock();
	return emitter && emitter->IsActive() && !emitter->IsPendingDestroy() &&
		ApplySpatial(voice, emitter->transform.position, *listener);
}

// 未使用の音声キャッシュを削除
void SoundSystem::ClearCache()
{
	for (auto it = sounds.begin(); it != sounds.end();)
	{
		if (it->second.use_count() == 1) it = sounds.erase(it);
		else ++it;
	}
}

// WAVのパスを解決してキャッシュに読み込む
std::shared_ptr<SoundSystem::SoundData> SoundSystem::LoadSound(const std::string& path)
{
	std::filesystem::path resolved = ResourceManager::Instance().ResolvePath(path);
	if (!std::filesystem::exists(resolved))
	{
		const std::filesystem::path dataPath = std::filesystem::path("Resources") / path;
		if (std::filesystem::exists(dataPath)) resolved = dataPath;
	}
	const std::string key = MakeSoundKey(resolved);
	if (const auto found = sounds.find(key); found != sounds.end()) return found->second;

	try
	{
		std::ifstream input(resolved, std::ios::binary | std::ios::ate);
		if (!input) throw std::runtime_error("file not found");
		const auto size = input.tellg();
		if (size <= 0) throw std::runtime_error("empty file");
		std::vector<uint8_t> bytes(static_cast<size_t>(size));
		input.seekg(0);
		if (!input.read(reinterpret_cast<char*>(bytes.data()), size))
			throw std::runtime_error("file read failed");
		auto sound = DecodeWave(bytes.data(), bytes.size(), resolved.generic_string());
		sounds.emplace(key, sound);
		return sound;
	}
	catch (const std::exception& exception)
	{
		ReportSoundError("WAV load failed: " + resolved.generic_string() + " (" +
			exception.what() + ")");
		return nullptr;
	}
}

std::shared_ptr<SoundSystem::SoundData> SoundSystem::LoadTrackSound(int track, int variant)
{
	if (track < 0 || track > 10000)
	{
		ReportSoundError("Track number must be between 0 and 10000");
		return nullptr;
	}
	const std::string logicalPath =
		"Resources/Sound/Tracks/" + std::to_string(track) + VSound::Extension;
	const std::filesystem::path resolved = ResourceManager::Instance().ResolvePath(logicalPath);
	try
	{
		std::ifstream input(resolved, std::ios::binary | std::ios::ate);
		if (!input) throw std::runtime_error("track is not registered or cached");
		const auto fileSizeValue = input.tellg();
		if (fileSizeValue < 12) throw std::runtime_error("truncated VSND header");
		const uint64_t fileSize = static_cast<uint64_t>(fileSizeValue);
		input.seekg(0);
		uint32_t magic = 0, version = 0, count = 0;
		input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		input.read(reinterpret_cast<char*>(&version), sizeof(version));
		input.read(reinterpret_cast<char*>(&count), sizeof(count));
		if (!input || magic != VSound::Magic || version != VSound::Version || count == 0 || count > 4096)
			throw std::runtime_error("invalid VSND header");
		std::vector<VSound::Entry> entries(count);
		for (auto& entry : entries)
		{
			input.read(reinterpret_cast<char*>(&entry.variant), sizeof(entry.variant));
			input.read(reinterpret_cast<char*>(&entry.uncompressedSize), sizeof(entry.uncompressedSize));
			input.read(reinterpret_cast<char*>(&entry.compressedSize), sizeof(entry.compressedSize));
			input.read(reinterpret_cast<char*>(&entry.offset), sizeof(entry.offset));
			if (!input || entry.uncompressedSize == 0 || entry.compressedSize == 0 ||
				entry.offset > fileSize || entry.compressedSize > fileSize - entry.offset)
				throw std::runtime_error("invalid VSND entry");
		}

		const VSound::Entry* selected = nullptr;
		if (variant >= 0)
		{
			const auto found = std::find_if(entries.begin(), entries.end(),
				[variant](const VSound::Entry& entry) { return entry.variant == variant; });
			if (found == entries.end()) throw std::runtime_error("variant is not registered");
			selected = &*found;
		}
		else
		{
			static thread_local std::mt19937 random(std::random_device{}());
			std::uniform_int_distribution<size_t> distribution(0, entries.size() - 1);
			selected = &entries[distribution(random)];
		}

		const auto updated = std::filesystem::last_write_time(resolved).time_since_epoch().count();
		const std::string key = MakeSoundKey(resolved) + "#" +
			std::to_string(selected->variant) + "#" + std::to_string(updated);
		if (const auto found = sounds.find(key); found != sounds.end()) return found->second;
		std::vector<uint8_t> compressed(selected->compressedSize);
		input.seekg(static_cast<std::streamoff>(selected->offset));
		if (!input.read(reinterpret_cast<char*>(compressed.data()), compressed.size()))
			throw std::runtime_error("truncated VSND data");

		DECOMPRESSOR_HANDLE decompressor = nullptr;
		if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor))
			throw std::runtime_error("decompressor initialization failed");
		struct DecompressorScope
		{
			DECOMPRESSOR_HANDLE value;
			~DecompressorScope() { if (value) CloseDecompressor(value); }
		} scope{decompressor};
		std::vector<uint8_t> wave(selected->uncompressedSize);
		SIZE_T written = 0;
		if (!Decompress(decompressor, compressed.data(), compressed.size(),
			wave.data(), wave.size(), &written) || written != wave.size())
			throw std::runtime_error("VSND decompression failed");
		auto sound = DecodeWave(wave.data(), wave.size(),
			logicalPath + "[" + std::to_string(selected->variant) + "]");
		sounds.emplace(key, sound);
		return sound;
	}
	catch (const std::exception& exception)
	{
		ReportSoundError("VSND load failed: " + resolved.generic_string() + " (" +
			exception.what() + ")");
		return nullptr;
	}
}

std::shared_ptr<SoundSystem::SoundData> SoundSystem::DecodeWave(
	const uint8_t* bytes, size_t size, const std::string& name) const
{
	if (!bytes || size < 12 || std::memcmp(bytes, "RIFF", 4) != 0 ||
		std::memcmp(bytes + 8, "WAVE", 4) != 0)
		throw std::runtime_error("invalid RIFF/WAVE header: " + name);
	auto sound = std::make_shared<SoundData>();
	bool hasFormat = false;
	bool hasSamples = false;
	size_t offset = 12;
	while (offset + 8 <= size && (!hasFormat || !hasSamples))
	{
		const uint8_t* chunkId = bytes + offset;
		uint32_t chunkSize = 0;
		std::memcpy(&chunkSize, bytes + offset + 4, sizeof(chunkSize));
		offset += 8;
		if (chunkSize > size - offset) throw std::runtime_error("truncated WAV chunk: " + name);
		if (std::memcmp(chunkId, "fmt ", 4) == 0)
		{
			if (chunkSize < 16 || chunkSize > sizeof(WAVEFORMATEXTENSIBLE))
				throw std::runtime_error("unsupported WAV format chunk: " + name);
			std::memcpy(&sound->format, bytes + offset, chunkSize);
			if (chunkSize == 16) sound->format.Format.cbSize = 0;
			hasFormat = true;
		}
		else if (std::memcmp(chunkId, "data", 4) == 0)
		{
			if (chunkSize == 0) throw std::runtime_error("invalid WAV sample size: " + name);
			sound->samples.assign(bytes + offset, bytes + offset + chunkSize);
			hasSamples = true;
		}
		offset += chunkSize + (chunkSize & 1u);
	}
	const WAVEFORMATEX& format = sound->format.Format;
	if (!hasFormat || !hasSamples || format.nChannels == 0 || format.nSamplesPerSec == 0 ||
		format.nBlockAlign == 0)
		throw std::runtime_error("WAV is missing required data: " + name);
	if (format.wFormatTag != WAVE_FORMAT_PCM && format.wFormatTag != WAVE_FORMAT_IEEE_FLOAT &&
		format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
		throw std::runtime_error("only PCM/float WAV is supported: " + name);
	return sound;
}

// ステレオ出力行列を適用
bool SoundSystem::ApplyPan(ActiveVoice& voice, float pan)
{
	voice.pan = std::clamp(pan, -1.0f, 1.0f);
	if (!voice.source || !masteringVoice || outputChannels != 2) return voice.pan == 0.0f;

	const uint32_t sourceChannels = voice.sound->format.Format.nChannels;
	if (sourceChannels == 1)
	{
		const float matrix[2] = {
			voice.pan <= 0.0f ? 1.0f : 1.0f - voice.pan,
			voice.pan >= 0.0f ? 1.0f : 1.0f + voice.pan,
		};
		return SUCCEEDED(voice.source->SetOutputMatrix(masteringVoice, 1, 2, matrix));
	}
	if (sourceChannels == 2)
	{
		const float left = voice.pan <= 0.0f ? 1.0f : 1.0f - voice.pan;
		const float right = voice.pan >= 0.0f ? 1.0f : 1.0f + voice.pan;
		const float matrix[4] = {left, 0.0f, 0.0f, right};
		return SUCCEEDED(voice.source->SetOutputMatrix(masteringVoice, 2, 2, matrix));
	}
	return false;
}

// 洞窟の残響バスを作成
bool SoundSystem::InitializeReverb()
{
	if (reverbVoice) return true;
	IUnknown* reverb = nullptr;
	HRESULT result = XAudio2CreateReverb(&reverb);
	if (FAILED(result))
	{
		ReportSoundError("XAudio2CreateReverb failed: " + std::to_string(result));
		return false;
	}

	XAUDIO2_EFFECT_DESCRIPTOR effect = {reverb, TRUE, 1};
	const XAUDIO2_EFFECT_CHAIN chain = {1, &effect};
	// 両経路を48kHzに揃えて残響の入力制限を守る
	result = xaudio->CreateSubmixVoice(&spatialVoice, outputChannels, SpatialSampleRate);
	if (SUCCEEDED(result))
		result = xaudio->CreateSubmixVoice(&reverbVoice, 1, SpatialSampleRate, 0, 0, nullptr, &chain);
	reverb->Release();
	if (SUCCEEDED(result))
	{
		XAUDIO2FX_REVERB_I3DL2_PARAMETERS cave = XAUDIO2FX_I3DL2_PRESET_CAVE;
		cave.DecayHFRatio = 0.65f;
		cave.RoomHF = -1200;
		XAUDIO2FX_REVERB_PARAMETERS parameters{};
		ReverbConvertI3DL2ToNative(&cave, &parameters);
		result = reverbVoice->SetEffectParameters(0, &parameters, sizeof(parameters));
	}
	if (SUCCEEDED(result)) return true;
	ReportSoundError("Cave reverb initialization failed: " + std::to_string(result));
	if (reverbVoice) reverbVoice->DestroyVoice();
	if (spatialVoice) spatialVoice->DestroyVoice();
	reverbVoice = nullptr;
	spatialVoice = nullptr;
	return false;
}

// 距離減衰・定位・こもりを更新
bool SoundSystem::ApplySpatial(
	ActiveVoice& voice, const Vector3& emitterPosition, const Camera& camera)
{
	const float distance = Vector3::Distance(emitterPosition, camera.GetEye());
	const float gain = GetDistanceGain(distance, voice.spatial);
	if (gain <= 0.0f) return false;

	Vector3 front = camera.GetFront();
	Vector3 up = camera.GetUp();
	if (!std::isfinite(front.LengthSquared()) || front.LengthSquared() < 0.000001f ||
		!std::isfinite(up.LengthSquared()) || up.LengthSquared() < 0.000001f) return false;
	front.Normalize();
	up -= front * front.Dot(up);
	if (up.LengthSquared() < 0.000001f) return false;
	up.Normalize();

	X3DAUDIO_LISTENER listener{};
	listener.Position = ToAudioVector(camera.GetEye());
	listener.OrientFront = ToAudioVector(front);
	listener.OrientTop = ToAudioVector(up);

	// 減衰は音量側に集約し、ここでは定位のみ計算
	X3DAUDIO_DISTANCE_CURVE_POINT points[] = {{0.0f, 1.0f}, {1.0f, 1.0f}};
	X3DAUDIO_DISTANCE_CURVE curve = {points, 2};
	X3DAUDIO_EMITTER source{};
	source.Position = ToAudioVector(emitterPosition);
	source.OrientFront = {0.0f, 0.0f, 1.0f};
	source.OrientTop = {0.0f, 1.0f, 0.0f};
	source.ChannelCount = 1;
	source.CurveDistanceScaler = 1.0f;
	source.pVolumeCurve = &curve;
	X3DAUDIO_DSP_SETTINGS dsp{};
	dsp.SrcChannelCount = 1;
	dsp.DstChannelCount = outputChannels;
	dsp.pMatrixCoefficients = voice.monoMatrix.data();
	X3DAudioCalculate(spatialHandle, &listener, &source, X3DAUDIO_CALCULATE_MATRIX, &dsp);

	// 複数チャンネルの素材も一点の音源として配置
	const uint32_t channels = voice.sound->format.Format.nChannels;
	for (uint32_t output = 0; output < outputChannels; ++output)
		for (uint32_t input = 0; input < channels; ++input)
			voice.matrix[output * channels + input] = voice.monoMatrix[output] / channels;
	if (FAILED(voice.source->SetOutputMatrix(spatialVoice, channels, outputChannels, voice.matrix.data())))
		return false;
	if (FAILED(voice.source->SetVolume(voice.volume * gain))) return false;

	const float t = std::clamp((distance - voice.spatial.minDistance) /
		(voice.spatial.maxDistance - voice.spatial.minDistance), 0.0f, 1.0f);
	const float cutoff = std::lerp(voice.spatial.lowPassHz, voice.spatial.farLowPassHz, t);
	// フィルターは変換後の48kHzで動作
	const float frequency = 2.0f * std::sin(DirectX::XM_PI * std::min(cutoff / SpatialSampleRate, 1.0f / 6.0f));
	const XAUDIO2_FILTER_PARAMETERS filter = {LowPassFilter, std::min(frequency, 1.0f), 1.0f};
	return SUCCEEDED(voice.source->SetFilterParameters(&filter));
}
