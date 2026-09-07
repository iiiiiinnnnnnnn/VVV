// SoundSystem.h
#pragma once

#include <xaudio2.h>
#include <x3daudio.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Audio/SoundTracks.generated.h"

class Actor;
class Camera;

// WAVの読み込み・再生・ボイス管理
class SoundSystem
{
  public:
	using VoiceId = uint64_t;
	static constexpr VoiceId InvalidVoiceId = 0;

	// 再生設定
	struct PlayOptions
	{
		float volume = 1.0f;
		float pitch = 1.0f;
		float pan = 0.0f;
		bool loop = false;
	};

	// 3D
	struct SpatialOptions : PlayOptions
	{
		float minDistance = 2.0f; // 減衰開始距離
		float maxDistance = 30.0f; // 再生限界距離
		float lowPassHz = 3500.0f; // 近距離の高音カット
		float farLowPassHz = 1200.0f; // 遠距離の高音カット
		float reverbMix = 0.35f; // 残響量 0～1
	};

	static SoundSystem& Instance()
	{
		static SoundSystem instance;
		return instance;
	}

	// XAudio2とマスタリングボイスを初期化
	bool Initialize();

	void Finalize();

	void Update();

	// WAVを再生してボイスIDを返す
	VoiceId Play(const std::string& path, const PlayOptions& options = {});

	// shared_ptr管理のActorに追従、範囲外なら停止
	VoiceId Play3D(const std::string& path, Actor* emitter, const SpatialOptions& options = {});

	// 登録済みトラックを再生。variant=-1はコンテナ内からランダム選択
	VoiceId PlayTrack(int track, int variant = -1, const PlayOptions& options = {});
	VoiceId PlayTrack(SoundTrack track, int variant = -1, const PlayOptions& options = {})
	{
		return PlayTrack(static_cast<int>(track), variant, options);
	}

	// 登録済みトラックをActorの位置から3D再生
	VoiceId PlayTrack3D(int track, Actor* emitter, int variant = -1,
		const SpatialOptions& options = {});
	VoiceId PlayTrack3D(SoundTrack track, Actor* emitter, int variant = -1,
		const SpatialOptions& options = {})
	{
		return PlayTrack3D(static_cast<int>(track), emitter, variant, options);
	}

	// 登録済みトラックを指定したワールド座標から3D再生
	VoiceId PlayTrack3DAt(int track, const Vector3& position, int variant = -1,
		const SpatialOptions& options = {});
	VoiceId PlayTrack3DAt(SoundTrack track, const Vector3& position, int variant = -1,
		const SpatialOptions& options = {})
	{
		return PlayTrack3DAt(static_cast<int>(track), position, variant, options);
	}

	// ボイスを停止・解放
	void Stop(VoiceId voiceId);

	// 全ボイスを停止・解放
	void StopAll();

	// 再生中か確認
	bool IsPlaying(VoiceId voiceId) const
	{
		const auto found = voices.find(voiceId);
		if (found == voices.end()) return false;
		return !found->second->callback->finished.load();
	}

	// マスター音量を設定
	void SetMasterVolume(float volume)
	{
		masterVolume = std::max(0.0f, volume);
		if (masteringVoice) masteringVoice->SetVolume(masterVolume);
	}

	// マスター音量を取得
	float GetMasterVolume() const { return masterVolume; }

	// ボイスの音量を設定
	bool SetVolume(VoiceId voiceId, float volume)
	{
		if (!std::isfinite(volume)) return false;
		const auto found = voices.find(voiceId);
		if (found == voices.end()) return false;
		ActiveVoice& voice = *found->second;
		voice.volume = std::clamp(volume, 0.0f, XAUDIO2_MAX_VOLUME_LEVEL);
		if (!voice.isSpatial) return SUCCEEDED(voice.source->SetVolume(voice.volume));
		if (UpdateSpatialVoice(voice)) return true;
		voices.erase(found);
		return false;
	}

	// ボイスのピッチ倍率を設定
	bool SetPitch(VoiceId voiceId, float pitch)
	{
		const auto found = voices.find(voiceId);
		if (found == voices.end()) return false;
		return SUCCEEDED(found->second->source->SetFrequencyRatio(
			std::clamp(pitch, XAUDIO2_MIN_FREQ_RATIO, XAUDIO2_MAX_FREQ_RATIO)));
	}

	// 2Dボイスのパンを設定
	bool SetPan(VoiceId voiceId, float pan)
	{
		const auto found = voices.find(voiceId);
		if (found == voices.end() || found->second->isSpatial) return false;
		return ApplyPan(*found->second, pan);
	}

	// 固定座標から再生中の3Dボイス位置を更新
	bool SetPosition(VoiceId voiceId, const Vector3& position)
	{
		const auto found = voices.find(voiceId);
		if (found == voices.end() || !found->second->isSpatial ||
			!found->second->usesFixedPosition) return false;
		found->second->fixedPosition = position;
		return UpdateSpatialVoice(*found->second);
	}

	// 未使用の音声キャッシュを削除
	void ClearCache();

	// 初期化済みか確認
	bool IsInitialized() const { return xaudio != nullptr && masteringVoice != nullptr; }

  private:
	struct SoundData;

	struct VoiceCallback final : IXAudio2VoiceCallback
	{
		std::atomic_bool finished = false;

		// 処理パス終了通知
		void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}

		// 処理パス開始通知
		void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}

		// バッファーの再生完了を記録
		void STDMETHODCALLTYPE OnBufferEnd(void*) override { finished.store(true); }

		// バッファー再生開始通知
		void STDMETHODCALLTYPE OnBufferStart(void*) override {}

		// ループ終了通知
		void STDMETHODCALLTYPE OnLoopEnd(void*) override {}

		// ストリーム終了を記録
		void STDMETHODCALLTYPE OnStreamEnd() override { finished.store(true); }

		// ボイスエラーを再生完了扱いにする
		void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override { finished.store(true); }
	};

	struct ActiveVoice
	{
		IXAudio2SourceVoice* source = nullptr;
		std::shared_ptr<SoundData> sound;
		std::unique_ptr<VoiceCallback> callback;
		std::weak_ptr<Actor> emitter;
		Vector3 fixedPosition = Vector3::Zero;
		SpatialOptions spatial;
		std::vector<float> matrix;
		std::vector<float> monoMatrix;
		std::vector<float> reverbMatrix;
		float volume = 1.0f;
		float pan = 0.0f;
		bool isSpatial = false;
		bool usesFixedPosition = false;

		// ソースボイスを停止・破棄
		~ActiveVoice();
	};

	// 外部からの生成・コピーを禁止
	SoundSystem() = default;
	SoundSystem(const SoundSystem&) = delete;
	SoundSystem& operator=(const SoundSystem&) = delete;

	// 終了処理漏れに備えて解放
	~SoundSystem();

	// WAVのパスを解決してキャッシュに読み込む
	std::shared_ptr<SoundData> LoadSound(const std::string& path);
	std::shared_ptr<SoundData> LoadTrackSound(int track, int variant);
	std::shared_ptr<SoundData> DecodeWave(
		const uint8_t* bytes, size_t size, const std::string& name) const;

	// ステレオ出力行列を適用
	bool ApplyPan(ActiveVoice& voice, float pan);

	// ボイスを作成して再生
	VoiceId StartVoice(const std::string& path, const PlayOptions& options,
		Actor* emitter, const SpatialOptions* spatial, const Camera* listener,
		const Vector3* fixedPosition = nullptr);
	VoiceId StartVoiceData(const std::string& name, std::shared_ptr<SoundData> sound,
		const PlayOptions& options, Actor* emitter, const SpatialOptions* spatial,
		const Camera* listener, const Vector3* fixedPosition = nullptr);

	// 距離減衰・定位・こもりを更新
	bool ApplySpatial(ActiveVoice& voice, const Vector3& emitterPosition, const Camera& listener);

	// 現在のActorとカメラで更新
	bool UpdateSpatialVoice(ActiveVoice& voice);

	// 洞窟の残響バスを作成
	bool InitializeReverb();

	IXAudio2* xaudio = nullptr;
	IXAudio2MasteringVoice* masteringVoice = nullptr;
	IXAudio2SubmixVoice* spatialVoice = nullptr;
	IXAudio2SubmixVoice* reverbVoice = nullptr;
	X3DAUDIO_HANDLE spatialHandle{};
	uint32_t outputChannels = 0;
	VoiceId nextVoiceId = 1;
	float masterVolume = 1.0f;
	std::unordered_map<std::string, std::shared_ptr<SoundData>> sounds;
	std::unordered_map<VoiceId, std::unique_ptr<ActiveVoice>> voices;
};
