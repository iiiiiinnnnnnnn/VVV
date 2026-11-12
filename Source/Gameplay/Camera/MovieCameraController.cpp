#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Gameplay/Camera/MovieCameraController.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "Application/Time/GameTime.h"
#include <InputKit/InputKit.h>
#include "Physics/Collider/CharacterController.h"
#include "Application/Tools/Dialog.h"
#include "Audio/SoundSystem.h"
#include "Core/Foundation/Json.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/Deer.h"
#include "Gameplay/Camera/FreeCameraController.h"
#include "Gameplay/Stage/Stage.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Effect/EffectManager.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"

static float MovieKeyBlend(float time, float begin, float end, int easing)
{
	if (end <= begin) return 0.0f;
	return Easing::Evaluate((time - begin) / (end - begin), static_cast<Easing::Type>(easing));
}

template <class Key> static size_t MovieKeyIndex(const std::vector<Key>& keys, float time)
{
	const auto next = std::upper_bound(keys.begin(), keys.end(), time,
		[](float value, const Key& key) { return value < key.time; });
	return next == keys.begin() ? 0 : static_cast<size_t>(next - keys.begin() - 1);
}

template <class Key> static void StoreMovieKey(std::vector<Key>& keys, const Key& key)
{
	const auto found = std::min_element(keys.begin(), keys.end(), [&](const Key& a, const Key& b) {
		return std::abs(a.time - key.time) < std::abs(b.time - key.time);
	});
	if (found != keys.end() && std::abs(found->time - key.time) < 0.001f) *found = key;
	else keys.push_back(key);
	std::sort(keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.time < b.time; });
}

// 発生イベントは同時刻にも複数登録できる。
static void StoreMovieKey(std::vector<MovieCameraController::ParticleKey>& keys,
	const MovieCameraController::ParticleKey& key)
{
	keys.push_back(key);
	std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

static void StoreMovieKey(std::vector<MovieCameraController::SoundKey>& keys,
	const MovieCameraController::SoundKey& key)
{
	keys.push_back(key);
	std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
}

MovieCameraController::MovieCameraController(Object* owner) : CameraController(owner)
{
	SetActive(false);
}

MovieCameraController::~MovieCameraController() { End(); }

void MovieCameraController::SyncCameraToController(const Camera& value)
{
	eye = value.GetEye();
	focus = value.GetFocus();
	up = value.GetUp();
	const float projectionY = value.GetProjection()._22;
	if (projectionY > 0.0f) fov = DirectX::XMConvertToDegrees(2.0f * std::atan(1.0f / projectionY));
}

void MovieCameraController::SyncControllerToCamera(Camera& value)
{
	Vector3 direction = focus - eye;
	if (direction.LengthSquared() < 0.000001f) return;
	Vector3 cameraUp = Vector3::Up;
	direction.Normalize();
	if (std::abs(direction.Dot(cameraUp)) > 0.999f) cameraUp = Vector3::UnitZ;
	value.SetLookAt(eye, focus, cameraUp);
	const float aspect = std::max(Game::Graphics::ScreenWidth, 1.0f) /
		std::max(Game::Graphics::ScreenHeight, 1.0f);
	value.SetPerspectiveFov(RAD(fov), aspect, 0.1f, 1000.0f);
}

bool MovieCameraController::Begin(Stage& value, Actor& player, Actor* boss, Game::PostProcess& postProcess)
{
	if (editing) return true;
	stage = &value;
	camera = owner->GetComponent<Camera>();
	freeCamera = owner->GetComponent<FreeCameraController>();
	if (!camera || !freeCamera) return false;
	try
	{
		if (!boss)
		{
			temporaryBoss = std::make_shared<Actor>("Scene Boss Preview");
			auto model = ResourceManager::Instance().LoadModel("Aracore", true);
			model->ApplyInitialMorphs();
			auto* renderer = temporaryBoss->AddComponent<VMDLModelComponent>(
				model, ModelShaderId::PBR, PBRRenderParams{}, false);
			renderer->SetModelYawOffset(DirectX::XM_PI);
			renderer->SetAutoUpdateTransform(false);
			stage->GetActorManager().Register(temporaryBoss);
			stage->GetActorManager().FlushPendingActors();
			renderer->PrepareMoviePreview();
			boss = temporaryBoss.get();
		}
		auto capture = [](Actor& actor) {
			ActorState state;
			state.actor = &actor;
			state.transform = actor.transform;
			state.active = actor.IsActive();
			state.renderer = actor.GetComponent<VMDLModelComponent>();
			if (!state.renderer || !state.renderer->GetModel()) throw std::runtime_error("Actor has no VMDL renderer.");
			state.renderer->GetModel()->GetNodePoses(state.pose);
			return state;
		};
		playerState = capture(player);
		bossState = capture(*boss);
		SyncCameraToController(*camera);
		savedEye = eye; savedFocus = focus; savedUp = up; savedFov = fov;
		wasPaused = Game::Time::paused;
		savedTimeScale = Game::Time::scale;
		moviePostProcess = &postProcess;
		savedPostSettings = postProcess.CaptureMovieSettings();
		currentPostSettings = savedPostSettings;
		visibility.clear(); cameraStates.clear(); controllerStates.clear();
		for (Actor* actor : stage->GetActorManager().GetActors())
		{
			if (dynamic_cast<Deer*>(actor)) visibility.push_back({ actor, actor->IsActive() });
			if (Camera* other = actor->GetComponent<Camera>()) cameraStates.push_back({ other, other->GetPriority() });
			for (CameraController* controller : actor->GetComponents<CameraController>())
			{
				controllerStates.push_back({ controller, controller->IsActive() });
			}
		}
		editing = true;
		particlePathsLoaded = false;
		soundPathsLoaded = false;
		for (const auto& state : controllerStates) state.controller->SetActive(false);
		origin = stage->GetTransform()->position;
		for (const auto& entry : visibility) entry.actor->SetActive(false);
		player.SetActive(true); boss->SetActive(true);
		stage->PromoteCamera(camera);
		closeRequested = false;
		Game::Time::paused = true;
		Game::Time::deltaTime = 0.0f;
		SoundSystem::Instance().SetPaused(true);
		if (sequence.cameras.empty())
		{
			SeedSequence();
			auto root = ResourceManager::FindSourceResourceRoot();
			if (root.empty()) root = std::filesystem::absolute("Resources");
			documentPath = root / "Movie" / "BossDefeat.vmovie";
			if (std::filesystem::exists(documentPath)) Load(documentPath);
		}
		PlaceAtStageCenter();
		Seek(0.0f);
		return true;
	}
	catch (const std::exception& error)
	{
		if (editing) End();
		else if (temporaryBoss) { temporaryBoss->SetActive(false); temporaryBoss->Destroy(); temporaryBoss.reset(); }
		return false;
	}
}

bool MovieCameraController::BeginPlayback(Stage& value, Actor& player, Actor& boss, Game::PostProcess& postProcess)
{
	if (editing) return false;
	if (!Begin(value, player, &boss, postProcess)) return false;
	auto root = ResourceManager::FindSourceResourceRoot();
	if (root.empty()) root = std::filesystem::absolute("Resources");
	if (!Load(root / "Movie" / "BossDefeat.vmovie")) { End(); return false; }
	runtimePlayback = true;
	playbackResult = 0;
	playing = true;
	freeView = false;
	Seek(0.0f);
	return true;
}

void MovieCameraController::End(bool keepActorPositions)
{
	StopSounds();
	StopParticles();
	if (!editing) return;
	playing = false;
	for (ActorState* state : { &playerState, &bossState })
	{
		if (!keepActorPositions) state->actor->transform = state->transform;
		else if (auto* controller = state->actor->GetComponent<CharacterController>())
			controller->SetPosition(state->actor->transform.position);
		if (!keepActorPositions) state->renderer->GetModel()->SetNodePoses(state->pose);
		state->renderer->UpdateModelTransform(state->actor->transform.GetMatrix());
	}
	for (const auto& state : visibility) state.actor->SetActive(state.active);
	playerState.actor->SetActive(playerState.active);
	bossState.actor->SetActive(bossState.active);
	camera->SetLookAt(savedEye, savedFocus, savedUp);
	const float aspect = std::max(Game::Graphics::ScreenWidth, 1.0f) / std::max(Game::Graphics::ScreenHeight, 1.0f);
	camera->SetPerspectiveFov(RAD(savedFov), aspect, 0.1f, 1000.0f);
	for (const auto& state : cameraStates) state.camera->SetPriority(state.priority);
	for (const auto& state : controllerStates) state.controller->SetActive(state.active);
	Game::Time::scale = savedTimeScale;
	Game::Time::paused = wasPaused;
	Game::Time::deltaTime = 0.0f;
	SoundSystem::Instance().SetPaused(wasPaused);
	if (moviePostProcess) moviePostProcess->ApplyMovieSettings(savedPostSettings);
	moviePostProcess = nullptr;
	if (temporaryBoss) { temporaryBoss->SetActive(false); temporaryBoss->Destroy(); temporaryBoss.reset(); }
	editing = false; closeRequested = false; freeView = false; runtimePlayback = false;
	stage = nullptr;
}

void MovieCameraController::PlaceAtStageCenter()
{
	if (!stage) return;
	// ボスの開始キーをステージ中央へ合わせ、全トラックへ同じ移動量を適用する。
	const Vector3 center = stage->GetTransform()->position;
	origin = center;
	if (!sequence.boss.empty()) origin -= sequence.boss.front().position;
	Terrain* terrain = stage->GetComponent<Terrain>();
	Transform* terrainTransform = stage->GetTransform();
	if (!terrain)
	{
		terrain = stage->GetTerrain();
		if (!terrain) return;
		for (Actor* actor : stage->GetActorManager().GetActors())
			if (actor->GetComponent<Terrain>() == terrain) { terrainTransform = actor->GetTransform(); break; }
	}
	if (!terrain || !terrainTransform) return;
	const Matrix world = terrainTransform->GetMatrix();
	const Matrix inverse = world.Invert();
	const auto groundHeight = [&](const Vector3& position)
	{
		Vector3 local = Vector3::Transform(position, inverse);
		local.y = terrain->GetSurfaceHeightByUV(local.x / terrain->GetTerrainSize() + 0.5f,
			local.z / terrain->GetTerrainSize() + 0.5f);
		return Vector3::Transform(local, world).y;
	};
	origin.y += groundHeight(center) - center.y;
	// 開始キーに負のYがあっても埋まらないよう、全トラックを共通の高さだけ持ち上げる。
	float lift = 0.0f;
	const auto fit = [&](const auto& keys)
	{
		if (keys.empty()) return;
		const Vector3 position = origin + keys.front().position;
		lift = std::max(lift, groundHeight(position) + 0.02f - position.y);
	};
	fit(sequence.player);
	fit(sequence.boss);
	origin.y += lift;
}

void MovieCameraController::SeedSequence()
{
	sequence = Sequence{};
	sequence.cameras = {
		{ 0, {8,5,-12}, {0,2,-3}, 55 },
		{ 1.5f, {6,5,-8}, {0,4,-2}, 60 },
		{ 2.5f, {2,1.8f,-10}, {0,1,-8}, 48, 0, true },
		{ 4.5f, {8,3,-10}, {0,2,-5}, 55 }
	};
	sequence.player = { {0, {0,0,-8}, 0} };
	sequence.boss = { {0, {0,0,0}, 180}, {1, {0,0,0}, 180, 2},
		{2, {0,5,-3}, 180, 1}, {3, {0,0,-5}, 180}, {5, {0,0,-5}, 180} };
	auto motion = [](ActorState& state, const char* preferred) {
		auto* model = state.renderer->GetModel();
		if (model->GetAnimationIndex(preferred) >= 0) return std::string(preferred);
		std::string query = preferred;
		std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		for (const auto& animation : model->GetAnimations())
		{
			std::string name = animation.name;
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (name == query) return animation.name;
		}
		return model->GetAnimations().empty() ? std::string{} : model->GetAnimations().front().name;
	};
	sequence.playerMotions = { {0, motion(playerState, "SS_Idle"), 0, 1, true} };
	sequence.bossMotions = { {0, motion(bossState, "Idle"), 0, 1, true},
		{1, motion(bossState, "Jump"), 0, 1, false} };
	sequence.effects = { {0.0f, savedPostSettings} };
	sequence.timeScales = { {0.0f, 1.0f} };
	undoStack.clear(); redoStack.clear();
	dirty = false;
}

void MovieCameraController::Seek(float time)
{
	StopSounds();
	StopParticles();
	cursor = std::clamp(time, 0.0f, sequence.duration);
	qteResult = 0;
	pendingQteResult = QteResult::None;
	Evaluate();
}

void MovieCameraController::StopSounds()
{
	auto& sound = SoundSystem::Instance();
	for (const auto& playback : soundPlaybacks)
		if (playback.voice) sound.Stop(playback.voice);
	soundPlaybacks.clear();
	if (previewSound) sound.Stop(previewSound);
	previewSound = SoundSystem::InvalidVoiceId;
}

void MovieCameraController::UpdateSounds(float previousTime)
{
	PROFILE_FUNCTION();
	auto& sound = SoundSystem::Instance();
	if (soundPlaybacks.size() != sequence.sounds.size())
	{
		StopSounds();
		soundPlaybacks.resize(sequence.sounds.size());
	}
	if (!playing)
	{
		for (auto& playback : soundPlaybacks)
		{
			if (!playback.voice || playback.paused) continue;
			sound.SetVoicePaused(playback.voice, true);
			playback.paused = true;
		}
		return;
	}
	if (previewSound) { sound.Stop(previewSound); previewSound = SoundSystem::InvalidVoiceId; }
	for (size_t i = 0; i < sequence.sounds.size(); ++i)
	{
		const auto& key = sequence.sounds[i];
		auto& playback = soundPlaybacks[i];
		if (!playback.started && key.time <= cursor)
		{
			playback.started = true;
			// シークで通り過ぎた音は鳴らさず、再生中に通過したキーだけ発音する。
			if (key.time >= previousTime && cursor < key.time + key.duration && !key.path.empty())
			{
				SoundSystem::PlayOptions options;
				options.volume = key.volume;
				options.pitch = key.pitch;
				options.pan = key.pan;
				options.loop = key.loop;
				playback.voice = sound.Play(key.path, options);
			}
		}
		if (!playback.voice) continue;
		if (cursor >= key.time + key.duration)
		{
			sound.Stop(playback.voice);
			playback.voice = SoundSystem::InvalidVoiceId;
		}
		else if (playback.paused)
		{
			sound.SetVoicePaused(playback.voice, false);
			playback.paused = false;
		}
	}
}

void MovieCameraController::EvaluateActor(ActorState& state, const std::vector<ActorKey>& keys,
	const std::vector<MotionKey>& motions)
{
	if (!state.actor || keys.empty()) return;
	const size_t index = MovieKeyIndex(keys, cursor);
	const auto& a = keys[index];
	const auto& b = keys[std::min(index + 1, keys.size() - 1)];
	const float blend = MovieKeyBlend(cursor, a.time, b.time, a.easing);
	state.actor->transform.position = origin + Vector3::Lerp(a.position, b.position, blend);
	const float yaw = a.yaw + std::remainder(b.yaw - a.yaw, 360.0f) * blend;
	state.actor->transform.rotation = Quaternion::CreateFromYawPitchRoll(RAD(yaw), 0.0f, 0.0f);
	auto* model = state.renderer->GetModel();
	auto pose = state.pose;
	if (!motions.empty() && cursor >= motions.front().time)
	{
		const auto& key = motions[MovieKeyIndex(motions, cursor)];
		const int animation = model->GetAnimationIndex(key.animation.c_str());
		if (animation >= 0)
		{
			const float duration = model->GetAnimations()[animation].secondsLength;
			float time = key.offset + (cursor - key.time) * key.speed;
			if (key.loop && duration > 0.0f) time = std::fmod(time, duration);
			else time = std::clamp(time, 0.0f, duration);
			const size_t count = std::min(pose.size(), model->GetAnimations()[animation].nodeAnims.size());
			for (size_t i = 0; i < count; ++i) model->ComputeAnimation(animation, static_cast<int>(i), time, pose[i]);
		}
	}
	model->SetNodePoses(pose);
	state.renderer->UpdateModelTransform(state.actor->transform.GetMatrix());
}

void MovieCameraController::StopParticles()
{
	auto manager = EffectManager::Instance().GetEffekseerManager();
	for (auto& playback : particlePlaybacks)
		if (playback.effect && manager && playback.handle >= 0 && manager->Exists(playback.handle))
			playback.effect->Stop(playback.handle);
	particlePlaybacks.clear();
	particleCursor = -1.0f;
}

Matrix MovieCameraController::GetParticleTransform(const ParticleKey& key) const
{
	Matrix anchor = Matrix::CreateTranslation(origin);
	if (key.target != 0)
	{
		const auto& keys = key.target == 1 ? sequence.player : sequence.boss;
		const ActorState& state = key.target == 1 ? playerState : bossState;
		if (!keys.empty())
		{
			const float time = key.follow ? cursor : key.time;
			const size_t index = MovieKeyIndex(keys, time);
			const auto& a = keys[index];
			const auto& b = keys[std::min(index + 1, keys.size() - 1)];
			const float blend = MovieKeyBlend(time, a.time, b.time, a.easing);
			const float yaw = a.yaw + std::remainder(b.yaw - a.yaw, 360.0f) * blend;
			anchor = Matrix::CreateScale(state.transform.scale) * Matrix::CreateRotationY(RAD(yaw)) *
				Matrix::CreateTranslation(origin + Vector3::Lerp(a.position, b.position, blend));
		}
	}
	return Matrix::CreateScale(key.scale) *
		Matrix::CreateFromYawPitchRoll(RAD(key.rotation.y), RAD(key.rotation.x), RAD(key.rotation.z)) *
		Matrix::CreateTranslation(key.position) * anchor;
}

void MovieCameraController::EvaluateParticles()
{
	PROFILE_FUNCTION();
	if (!editing) return;
	auto manager = EffectManager::Instance().GetEffekseerManager();
	if (!manager) return;
	if (cursor < particleCursor || particlePlaybacks.size() != sequence.particles.size()) StopParticles();
	particlePlaybacks.resize(sequence.particles.size());
	particleCursor = cursor;
	for (size_t i = 0; i < sequence.particles.size(); ++i)
	{
		const auto& key = sequence.particles[i];
		auto& playback = particlePlaybacks[i];
		if (key.path.empty() || cursor < key.time || cursor >= key.time + key.duration)
		{
			if (playback.effect && playback.handle >= 0 && manager->Exists(playback.handle))
				playback.effect->Stop(playback.handle);
			playback.handle = -1;
			playback.started = false;
			playback.frame = -1.0f;
			continue;
		}
		if (!playback.started)
		{
			playback.started = true;
			try
			{
				if (!playback.effect) playback.effect = EffectManager::Instance().LoadEffect(key.path);
				playback.effect->SetBillboard(key.billboard);
				playback.handle = playback.effect->Play(Vector3::Zero);
				if (playback.handle >= 0) manager->SetRandomSeed(playback.handle, key.seed);
			}
			catch (const std::exception& error) {  }
		}
		if (!playback.effect || playback.handle < 0 || !manager->Exists(playback.handle)) continue;
		Matrix transform = GetParticleTransform(key);
		if (key.billboard) transform = EffectManager::CreateBillboardTransform(transform, camera->GetView());
		const float frame = std::floor(std::max(cursor - key.time, 0.0f) * key.speed * 60.0f + 0.0001f);
		if (playback.frame == frame && playback.transform == transform) continue;
		playback.transform = transform;
		playback.effect->SetTransform(playback.handle, transform);

		// ムービーの時刻だけで進める。ゲーム時間や他のエフェクトは進めない。
		manager->SetPaused(playback.handle, false);
		manager->BeginUpdate();
		if (playback.frame < 0.0f)
		{
			manager->UpdateHandle(playback.handle, 0.0f);
			playback.frame = 0.0f;
		}
		for (; playback.frame < frame; playback.frame += 1.0f)
		{
			manager->UpdateHandle(playback.handle, 1.0f);
			if (manager->GetInstanceCount(playback.handle) == 0) { playback.frame = frame; break; }
		}
		manager->UpdateHandle(playback.handle, 0.0f);
		manager->EndUpdate();
		manager->SetPaused(playback.handle, true);
	}
}

void MovieCameraController::Evaluate()
{
	PROFILE_FUNCTION();
	if (!editing || sequence.cameras.empty()) return;
	Game::Time::scale = GetMovieTimeScale(cursor);
	EvaluateActor(playerState, sequence.player, sequence.playerMotions);
	EvaluateActor(bossState, sequence.boss, sequence.bossMotions);
	currentPostSettings = savedPostSettings;
	if (!sequence.effects.empty())
	{
		const size_t index = MovieKeyIndex(sequence.effects, cursor);
		const auto& a = sequence.effects[index];
		const auto& b = sequence.effects[std::min(index + 1, sequence.effects.size() - 1)];
		const float blend = MovieKeyBlend(cursor, a.time, b.time, a.easing);
		const auto mix = [&](const auto& self, const json& x, const json& y, const json& schema) -> json {
			if (schema.is_object())
			{
				json result = json::object();
				for (const auto& [name, value] : schema.items()) result[name] = self(self, x.at(name), y.at(name), value);
				return result;
			}
			if (schema.is_array())
			{
				json result = json::array();
				for (size_t i = 0; i < schema.size(); ++i) result.push_back(self(self, x[i], y[i], schema[i]));
				return result;
			}
			if (schema.is_boolean() || schema.is_number_integer()) return x;
			return std::lerp(x.get<float>(), y.get<float>(), blend);
		};
		currentPostSettings = mix(mix, a.settings, b.settings, savedPostSettings);
	}
	if (freeView) { EvaluateParticles(); return; }
	const size_t index = MovieKeyIndex(sequence.cameras, cursor);
	const auto& a = sequence.cameras[index];
	const auto& b = sequence.cameras[std::min(index + 1, sequence.cameras.size() - 1)];
	const float blend = b.cut ? 0.0f : MovieKeyBlend(cursor, a.time, b.time, a.easing);
	eye = origin + Vector3::Lerp(a.eye, b.eye, blend);
	focus = origin + Vector3::Lerp(a.focus, b.focus, blend);
	fov = std::lerp(a.fov, b.fov, blend);
	SyncControllerToCamera(*camera);
	EvaluateParticles();
}

float MovieCameraController::GetMovieTimeScale(float time) const
{
	const auto& keys = sequence.timeScales;
	if (keys.empty() || time < keys.front().time) return 1.0f;
	const size_t index = MovieKeyIndex(keys, time);
	const auto& a = keys[index];
	const auto& b = keys[std::min(index + 1, keys.size() - 1)];
	const float blend = a.interpolate ? MovieKeyBlend(time, a.time, b.time, a.easing) : 0.0f;
	return std::clamp(std::lerp(a.scale, b.scale, blend), 0.01f, 10.0f);
}

void MovieCameraController::AdvanceTime(float deltaTime)
{
	// スケール切替点を跨ぐ長いフレームでも、区間ごとの速度を反映する。
	double time = cursor;
	double remaining = std::max(deltaTime, 0.0f);
	while (remaining > 0.0 && time < sequence.duration)
	{
		const auto next = std::upper_bound(sequence.timeScales.begin(), sequence.timeScales.end(), time,
			[](double value, const TimeScaleKey& key) { return value < key.time; });
		const double boundary = next == sequence.timeScales.end() ? sequence.duration : next->time;
		const double step = std::min(remaining, 1.0 / 120.0);
		const double scale = GetMovieTimeScale(static_cast<float>(time));
		const double midpoint = std::min(time + scale * step * 0.5, (time + boundary) * 0.5);
		const double rate = GetMovieTimeScale(static_cast<float>(midpoint));
		const double advance = rate * step;
		if (advance >= boundary - time)
		{
			remaining = std::max(remaining - (boundary - time) / rate, 0.0);
			time = boundary;
		}
		else
		{
			time += advance;
			remaining = std::max(remaining - step, 0.0);
		}
	}
	cursor = std::min(static_cast<float>(time), sequence.duration);
}

MovieCameraController::QteResult MovieCameraController::ConsumeQteResult()
{
	const auto result = pendingQteResult;
	pendingQteResult = QteResult::None;
	return result;
}

void MovieCameraController::ResolveQte(QteResult result)
{
	if (qteResult || result == QteResult::None) return;
	qteResult = result == QteResult::Success ? 1 : -1;
	pendingQteResult = result;
}

float MovieCameraController::GetQteRemaining() const
{
	if (!editing || !sequence.qteEnabled || qteResult || cursor < sequence.qteStart) return 0.0f;
	const float end = runtimePlayback ? std::min(sequence.qteEnd, 2.250f) : sequence.qteEnd;
	return std::max(end - cursor, 0.0f);
}

float MovieCameraController::GetQteDuration() const
{
	const float end = runtimePlayback ? std::min(sequence.qteEnd, 2.250f) : sequence.qteEnd;
	return std::max(end - sequence.qteStart, 0.0f);
}

void MovieCameraController::Tick(bool qtePressed)
{
	PROFILE_FUNCTION();
	if (!editing) return;
	SoundSystem::Instance().SetPaused(!playing && !SoundSystem::Instance().IsPlaying(previewSound));
	Game::Time::deltaTime = 0.0f;
	Game::Time::scale = GetMovieTimeScale(cursor);
	if (runtimePlayback)
	{
		const float previous = cursor;
		const float failureTime = std::min(2.250f, sequence.duration);
		const float deadline = std::min(sequence.qteEnd, failureTime);
		AdvanceTime(Game::Time::unscaledDeltaTime);
		if (sequence.qteEnabled && !qteResult)
		{
			if (qtePressed && previous <= deadline && cursor >= sequence.qteStart) ResolveQte(QteResult::Success);
			else if (cursor > deadline || cursor >= failureTime) ResolveQte(QteResult::Failed);
		}
		if (sequence.qteEnabled && qteResult <= 0 && cursor >= failureTime)
		{
			cursor = failureTime;
			Evaluate();
			playbackResult = -1;
			End(true);
			InputKit::InputSystem::Instance().SuppressGameplayInput();
			return;
		}
		Evaluate();
		UpdateSounds(previous);
		if ((!sequence.qteEnabled || qteResult > 0) && cursor >= sequence.duration)
		{
			playbackResult = 1;
			End(true);
			InputKit::InputSystem::Instance().SuppressGameplayInput();
		}
		return;
	}
	if (closeRequested) { End(); return; }
	if (!ImGui::IsAnyItemActive()) editUndoSaved = false;
	if (!ImGui::GetIO().WantTextInput)
	{
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
		{ if (ImGui::GetIO().KeyShift) Redo(); else Undo(); }
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) Redo();
		if (!ImGui::IsAnyItemActive() && ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteKey();
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) Save(documentPath);
		if (!ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) RegisterKey();
	}
	if (freeView && !playing)
	{
		freeCamera->UpdateCamera();
		freeCamera->SyncControllerToCamera(*camera);
		SyncCameraToController(*camera);
		SyncControllerToCamera(*camera);
	}
	if (!playing) { UpdateSounds(cursor); EvaluateParticles(); return; }
	const float previous = cursor;
	AdvanceTime(Game::Time::unscaledDeltaTime);
	UpdateSounds(previous);
	if (sequence.qteEnabled && !qteResult)
	{
		if (qtePressed && previous <= sequence.qteEnd && cursor >= sequence.qteStart) ResolveQte(QteResult::Success);
		else if (cursor > sequence.qteEnd) ResolveQte(QteResult::Failed);
	}
	if (cursor >= sequence.duration)
	{
		if (loop) { StopSounds(); StopParticles(); cursor = std::fmod(cursor, sequence.duration); qteResult = 0; UpdateSounds(0.0f); }
		else { cursor = sequence.duration; playing = false; StopSounds(); }
	}
	Evaluate();
}

static json MovieVectorJson(const Vector3& value) { return json::array({ value.x, value.y, value.z }); }
static Vector3 ReadMovieVector(const json& value)
{
	if (!value.is_array() || value.size() != 3) throw std::runtime_error("Invalid movie vector.");
	return { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
}
static bool MovieFinite(const Vector3& value)
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

static void ValidateMovieSequence(const MovieCameraController::Sequence& movie)
{
	if (!std::isfinite(movie.duration) || movie.duration < 0.05f || movie.duration > 3600.0f ||
		!std::isfinite(movie.qteStart) || !std::isfinite(movie.qteEnd) || movie.qteStart < 0.0f ||
		movie.qteEnd <= movie.qteStart || movie.qteEnd > movie.duration)
		throw std::runtime_error("Invalid movie duration or QTE interval.");
	const auto validate = [&](const auto& keys, bool required) {
		if ((required && keys.empty()) || keys.size() > 10000) throw std::runtime_error("Invalid movie key count.");
		float previous = -1.0f;
		for (const auto& key : keys)
		{
			using Key = std::decay_t<decltype(key)>;
			if (!std::isfinite(key.time) || key.time < 0.0f || key.time > movie.duration || key.time < previous ||
				(key.time == previous && !std::is_same_v<Key, MovieCameraController::ParticleKey> &&
					!std::is_same_v<Key, MovieCameraController::SoundKey>))
				throw std::runtime_error("Movie key times must be ordered, unique and within the duration.");
			previous = key.time;
			if constexpr (std::is_same_v<Key, MovieCameraController::CameraKey>)
			{
				if (!MovieFinite(key.eye) || !MovieFinite(key.focus) ||
					(key.focus - key.eye).LengthSquared() < 0.000001f ||
					!std::isfinite(key.fov) || key.fov < 10.0f || key.fov > 120.0f)
					throw std::runtime_error("Invalid movie camera key.");
			}
			else if constexpr (std::is_same_v<Key, MovieCameraController::ActorKey>)
			{
				if (!MovieFinite(key.position) || !std::isfinite(key.yaw)) throw std::runtime_error("Invalid movie actor key.");
			}
			else if constexpr (std::is_same_v<Key, MovieCameraController::TimeScaleKey>)
			{
				if (!std::isfinite(key.scale) || key.scale < 0.01f || key.scale > 10.0f)
					throw std::runtime_error("Invalid movie time scale.");
			}
			else if constexpr (std::is_same_v<Key, MovieCameraController::ParticleKey>)
			{
				if (key.path.size() > 1024 || !std::isfinite(key.duration) || key.duration <= 0.0f || key.duration > 3600.0f ||
					!MovieFinite(key.position) || !MovieFinite(key.rotation) || !MovieFinite(key.scale) ||
					key.scale.x <= 0.0f || key.scale.y <= 0.0f || key.scale.z <= 0.0f ||
					!std::isfinite(key.speed) || key.speed < 0.0f || key.speed > 10.0f || key.target < 0 || key.target > 2)
					throw std::runtime_error("Invalid movie particle key.");
			}
			else if constexpr (std::is_same_v<Key, MovieCameraController::SoundKey>)
			{
				if (key.path.size() > 1024 || !std::isfinite(key.duration) || key.duration <= 0.0f || key.duration > 3600.0f ||
					!std::isfinite(key.volume) || key.volume < 0.0f || key.volume > 4.0f ||
					!std::isfinite(key.pitch) || key.pitch < 0.01f || key.pitch > 2.0f ||
					!std::isfinite(key.pan) || key.pan < -1.0f || key.pan > 1.0f)
					throw std::runtime_error("Invalid movie sound key.");
			}
			else if constexpr (std::is_same_v<Key, MovieCameraController::EffectKey>)
			{
				Game::PostProcess::ValidateMovieSettings(key.settings);
			}
			else
			{
				if (!std::isfinite(key.offset) || key.offset < 0.0f ||
					!std::isfinite(key.speed) || key.speed < 0.0f || key.animation.size() > 512)
					throw std::runtime_error("Invalid movie motion key.");
			}
			if constexpr (!std::is_same_v<Key, MovieCameraController::MotionKey> &&
				!std::is_same_v<Key, MovieCameraController::ParticleKey> &&
				!std::is_same_v<Key, MovieCameraController::SoundKey>)
				if (key.easing < 0 || key.easing >= static_cast<int>(Easing::Type::Count))
					throw std::runtime_error("Invalid movie easing.");
		}
	};
	validate(movie.cameras, true); validate(movie.player, true); validate(movie.boss, true);
	validate(movie.playerMotions, false); validate(movie.bossMotions, false);
	validate(movie.effects, true);
	validate(movie.particles, false);
	validate(movie.timeScales, false);
	validate(movie.sounds, false);
}

bool MovieCameraController::Save(const std::filesystem::path& path)
{
	std::filesystem::path temporary;
	try
	{
		ValidateMovieSequence(sequence);
		json root = {{"format", "VEER.Movie"}, {"version", 5}, {"duration", sequence.duration},
			{"qte", {{"enabled", sequence.qteEnabled}, {"start", sequence.qteStart}, {"end", sequence.qteEnd}}}};
		root["camera"] = json::array();
		for (const auto& key : sequence.cameras)
			root["camera"].push_back({{"time", key.time}, {"eye", MovieVectorJson(key.eye)},
				{"focus", MovieVectorJson(key.focus)}, {"fov", key.fov}, {"easing", key.easing}, {"cut", key.cut}});
		const auto writeActor = [&](const char* name, const auto& keys, const auto& motions) {
			json actor = {{"keys", json::array()}, {"motions", json::array()}};
			for (const auto& key : keys) actor["keys"].push_back({{"time", key.time},
				{"position", MovieVectorJson(key.position)}, {"yaw", key.yaw}, {"easing", key.easing}});
			for (const auto& key : motions) actor["motions"].push_back({{"time", key.time},
				{"animation", key.animation}, {"offset", key.offset}, {"speed", key.speed}, {"loop", key.loop}});
			root[name] = std::move(actor);
		};
		writeActor("player", sequence.player, sequence.playerMotions);
		writeActor("boss", sequence.boss, sequence.bossMotions);
		root["effects"] = json::array();
		for (const auto& key : sequence.effects) root["effects"].push_back({
			{"time", key.time}, {"settings", key.settings}, {"easing", key.easing}});
		root["particles"] = json::array();
		for (const auto& key : sequence.particles) root["particles"].push_back({
			{"time", key.time}, {"path", key.path}, {"duration", key.duration},
			{"position", MovieVectorJson(key.position)}, {"rotation", MovieVectorJson(key.rotation)},
			{"scale", MovieVectorJson(key.scale)}, {"speed", key.speed}, {"target", key.target},
			{"follow", key.follow}, {"billboard", key.billboard}, {"seed", key.seed}});
		root["timeScales"] = json::array();
		for (const auto& key : sequence.timeScales) root["timeScales"].push_back({
			{"time", key.time}, {"scale", key.scale}, {"easing", key.easing}, {"interpolate", key.interpolate}});
		root["sounds"] = json::array();
		for (const auto& key : sequence.sounds) root["sounds"].push_back({
			{"time", key.time}, {"path", key.path}, {"duration", key.duration},
			{"volume", key.volume}, {"pitch", key.pitch}, {"pan", key.pan}, {"loop", key.loop}});
		auto destination = path; destination.replace_extension(".vmovie");
		if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
		temporary = destination; temporary += L".saving.tmp";
		std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
		stream << root.dump(2);
		stream.close();
		if (!stream || !MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Failed to save movie timeline.");
		documentPath = destination; dirty = false;
		return true;
	}
	catch (const std::exception& error)
	{
		if (!temporary.empty()) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
		return false;
	}
}

bool MovieCameraController::Load(const std::filesystem::path& path)
{
	try
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream) throw std::runtime_error("Movie timeline not found.");
		json root; stream >> root;
		if (root.at("format") != "VEER.Movie" || root.at("version") != 5)
			throw std::runtime_error("Unsupported movie timeline format/version.");
		Sequence candidate;
		candidate.duration = root.at("duration").get<float>();
		candidate.qteEnabled = root.at("qte").at("enabled").get<bool>();
		candidate.qteStart = root.at("qte").at("start").get<float>();
		candidate.qteEnd = root.at("qte").at("end").get<float>();
		for (const auto& item : root.at("camera")) candidate.cameras.push_back({item.at("time").get<float>(),
			ReadMovieVector(item.at("eye")), ReadMovieVector(item.at("focus")), item.at("fov").get<float>(),
			item.at("easing").get<int>(), item.at("cut").get<bool>()});
		const auto readActor = [&](const char* name, auto& keys, auto& motions) {
			for (const auto& item : root.at(name).at("keys")) keys.push_back({item.at("time").get<float>(),
				ReadMovieVector(item.at("position")), item.at("yaw").get<float>(), item.at("easing").get<int>()});
			for (const auto& item : root.at(name).at("motions")) motions.push_back({item.at("time").get<float>(),
				item.at("animation").get<std::string>(), item.at("offset").get<float>(),
				item.at("speed").get<float>(), item.at("loop").get<bool>()});
		};
		readActor("player", candidate.player, candidate.playerMotions);
		readActor("boss", candidate.boss, candidate.bossMotions);
		for (const auto& item : root.at("effects")) candidate.effects.push_back({
			item.at("time").get<float>(), item.at("settings"), item.at("easing").get<int>()});
		for (const auto& item : root.at("particles"))
		{
			ParticleKey key;
			key.time = item.at("time").get<float>();
			key.path = item.at("path").get<std::string>();
			key.duration = item.at("duration").get<float>();
			key.position = ReadMovieVector(item.at("position"));
			key.rotation = ReadMovieVector(item.at("rotation"));
			key.scale = ReadMovieVector(item.at("scale"));
			key.speed = item.at("speed").get<float>();
			key.target = item.at("target").get<int>();
			key.follow = item.at("follow").get<bool>();
			key.billboard = item.at("billboard").get<bool>();
			key.seed = item.at("seed").get<int>();
			candidate.particles.push_back(std::move(key));
		}
		for (const auto& item : root.at("timeScales")) candidate.timeScales.push_back({
			item.at("time").get<float>(), item.at("scale").get<float>(),
			item.at("easing").get<int>(), item.at("interpolate").get<bool>()});
		for (const auto& item : root.at("sounds")) candidate.sounds.push_back({
			item.at("time").get<float>(), item.at("path").get<std::string>(),
			item.at("duration").get<float>(), item.at("volume").get<float>(),
			item.at("pitch").get<float>(), item.at("pan").get<float>(), item.at("loop").get<bool>()});
		ValidateMovieSequence(candidate);
		Game::PostProcess effectSettings;
		for (auto& key : candidate.effects)
		{
			effectSettings.ApplyMovieSettings(key.settings);
			key.settings = effectSettings.CaptureMovieSettings();
		}
		sequence = std::move(candidate);
		documentPath = path; documentPath.replace_extension(".vmovie");
		undoStack.clear(); redoStack.clear();
		dirty = false; playing = false; freeView = false;
		selectedTrack = 0; selectedKey = -1;
		PlaceAtStageCenter();
		Seek(0.0f);
		return true;
	}
	catch (const std::exception& error) { return false; }
}


void MovieCameraController::ApplyPostEffects(Game::PostProcess& postProcess) const
{
	if (editing && !currentPostSettings.empty()) postProcess.ApplyMovieSettings(currentPostSettings);
}

void MovieCameraController::PushUndo()
{
	undoStack.push_back(sequence);
	if (undoStack.size() > 64) undoStack.erase(undoStack.begin());
	redoStack.clear();
}

void MovieCameraController::Undo()
{
	if (undoStack.empty()) return;
	redoStack.push_back(sequence); sequence = std::move(undoStack.back()); undoStack.pop_back();
	dirty = true; Scrub(cursor);
}

void MovieCameraController::Redo()
{
	if (redoStack.empty()) return;
	undoStack.push_back(sequence); sequence = std::move(redoStack.back()); redoStack.pop_back();
	dirty = true; Scrub(cursor);
}

void MovieCameraController::Scrub(float time)
{
	selectedKey = -1; playing = false; freeView = false;
	Seek(time);
}

void MovieCameraController::SelectKey(int track, int index)
{
	selectedTrack = track; selectedKey = index; playing = false; freeView = false;
	const auto select = [&](const auto& keys) {
		if (index >= 0 && index < static_cast<int>(keys.size())) Seek(keys[index].time);
		else selectedKey = -1;
	};
	switch (track)
	{
	case 0: select(sequence.cameras); break;
	case 1: select(sequence.player); break;
	case 2: select(sequence.boss); break;
	case 3: select(sequence.playerMotions); break;
	case 4: select(sequence.bossMotions); break;
	case 5: select(sequence.effects); break;
	case 6: select(sequence.particles); break;
	case 7: select(sequence.timeScales); break;
	case 8: select(sequence.sounds); break;
	}
}

void MovieCameraController::RegisterKey()
{
	PushUndo();
	const auto store = [&](auto& keys, auto key) {
		key.time = cursor; StoreMovieKey(keys, key);
		selectedKey = static_cast<int>(MovieKeyIndex(keys, cursor));
	};
	if (selectedTrack == 0)
	{
		SyncCameraToController(*camera);
		CameraKey key = sequence.cameras[MovieKeyIndex(sequence.cameras, cursor)];
		key.eye = eye - origin; key.focus = focus - origin; key.fov = fov;
		store(sequence.cameras, key);
	}
	else if (selectedTrack == 1 || selectedTrack == 2)
	{
		auto& state = selectedTrack == 1 ? playerState : bossState;
		auto& keys = selectedTrack == 1 ? sequence.player : sequence.boss;
		ActorKey key = keys[MovieKeyIndex(keys, cursor)];
		const Vector3 forward = state.actor->transform.GetForward();
		key.position = state.actor->transform.position - origin;
		key.yaw = DirectX::XMConvertToDegrees(std::atan2(forward.x, forward.z));
		store(keys, key);
	}
	else if (selectedTrack == 3 || selectedTrack == 4)
	{
		auto& keys = selectedTrack == 3 ? sequence.playerMotions : sequence.bossMotions;
		MotionKey key;
		if (!keys.empty())
		{
			key = keys[MovieKeyIndex(keys, cursor)];
			key.offset += std::max(cursor - key.time, 0.0f) * key.speed;
		}
		store(keys, key);
	}
	else if (selectedTrack == 5)
	{
		EffectKey key{cursor, currentPostSettings};
		if (!sequence.effects.empty()) key.easing = sequence.effects[MovieKeyIndex(sequence.effects, cursor)].easing;
		store(sequence.effects, key);
	}
	else if (selectedTrack == 6)
	{
		ParticleKey key = particleDraft;
		if (selectedKey >= 0 && selectedKey < static_cast<int>(sequence.particles.size())) key = sequence.particles[selectedKey];
		store(sequence.particles, key);
	}
	else if (selectedTrack == 7)
	{
		TimeScaleKey key{cursor, GetMovieTimeScale(cursor)};
		if (!sequence.timeScales.empty()) key = sequence.timeScales[MovieKeyIndex(sequence.timeScales, cursor)];
		key.scale = std::clamp(Game::Time::scale, 0.01f, 10.0f);
		store(sequence.timeScales, key);
	}
	else
	{
		SoundKey key = soundDraft;
		if (selectedKey >= 0 && selectedKey < static_cast<int>(sequence.sounds.size())) key = sequence.sounds[selectedKey];
		store(sequence.sounds, key);
	}
	dirty = true; playing = false;
	Seek(cursor);
}

void MovieCameraController::DeleteKey()
{
	bool erased = false;
	const auto erase = [&](auto& keys, bool required) {
		if (selectedKey < 0 || selectedKey >= static_cast<int>(keys.size())) return;
		if (required && keys.size() == 1) { return; }
		PushUndo(); keys.erase(keys.begin() + selectedKey); erased = true;
	};
	switch (selectedTrack)
	{
	case 0: erase(sequence.cameras, true); break;
	case 1: erase(sequence.player, true); break;
	case 2: erase(sequence.boss, true); break;
	case 3: erase(sequence.playerMotions, false); break;
	case 4: erase(sequence.bossMotions, false); break;
	case 5: erase(sequence.effects, true); break;
	case 6: erase(sequence.particles, false); break;
	case 7: erase(sequence.timeScales, false); break;
	case 8: erase(sequence.sounds, false); break;
	}
	if (erased) { selectedKey = -1; dirty = true; Seek(cursor); }
}

void MovieCameraController::DrawTimeline()
{
	constexpr float labelWidth = 155.0f;
	constexpr float rowHeight = 32.0f;
	constexpr float rulerHeight = 25.0f;
	constexpr float padding = 13.0f;
	const float width = std::max(ImGui::GetContentRegionAvail().x, labelWidth + 100.0f);
	const float timeWidth = width - labelWidth - padding * 2;
	const ImVec2 begin = ImGui::GetCursorScreenPos();
	const float left = begin.x + labelWidth + padding;
	const auto timeX = [&](float time) { return left + time / sequence.duration * timeWidth; };
	ImGui::InvisibleButton("MovieTimelineCanvas", {width, rulerHeight + rowHeight * 10},
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(begin, {begin.x + width, begin.y + rulerHeight + rowHeight * 10}, IM_COL32(23,26,33,255));
	for (int tick = 0; tick <= 10; ++tick)
	{
		char text[24]; std::snprintf(text, sizeof(text), "%.2f", sequence.duration * tick / 10.0f);
		const float x = left + timeWidth * tick / 10.0f;
		draw->AddText({x - ImGui::CalcTextSize(text).x * 0.5f,begin.y + 3}, IM_COL32(180,190,205,255), text);
		draw->AddLine({x,begin.y + rulerHeight}, {x,begin.y + rulerHeight + rowHeight * 10}, IM_COL32(50,55,65,255));
	}
	const char* labels[] = { "Camera", "Player / Position", "Boss / Position", "Player / Motion", "Boss / Motion", "Post Effects", "Particles", "Time Scale", "Sounds" };
	for (int track = 0; track < 9; ++track)
	{
		const float y = begin.y + rulerHeight + rowHeight * track;
		if (selectedTrack == track) draw->AddRectFilled({begin.x,y}, {begin.x + width,y + rowHeight}, IM_COL32(40,58,78,150));
		draw->AddText({begin.x + 6,y + 8}, IM_COL32(225,230,240,255), labels[track]);
		draw->AddLine({begin.x,y + rowHeight}, {begin.x + width,y + rowHeight}, IM_COL32(60,65,75,255));
	}
	const auto keys = [&](const auto& values, int track) {
		using Key = typename std::decay_t<decltype(values)>::value_type;
		const float y = begin.y + rulerHeight + rowHeight * (track + 0.5f);
		for (size_t i = 0; i < values.size(); ++i)
		{
			const bool selected = selectedTrack == track && selectedKey == static_cast<int>(i);
			const float x = timeX(values[i].time);
			if constexpr (std::is_same_v<Key, ParticleKey> || std::is_same_v<Key, SoundKey>)
				draw->AddRectFilled({x, y - 7}, {timeX(std::min(values[i].time + values[i].duration, sequence.duration)), y + 7},
					IM_COL32(100, 180, 140, selected ? 180 : 90), 3.0f);
			draw->AddCircleFilled({x,y}, selected ? 7.0f : 5.5f,
				selected ? IM_COL32(255,190,60,255) : IM_COL32(90,180,255,255));
			if (selected) draw->AddCircle({x,y}, 10.0f, IM_COL32(255,220,130,255));
			if (hovered && std::abs(ImGui::GetIO().MousePos.x - x) <= 12 && std::abs(ImGui::GetIO().MousePos.y - y) <= 14)
				ImGui::SetTooltip("%s / Key %zu / %.3f s", labels[track], i + 1, values[i].time);
		}
	};
	keys(sequence.cameras, 0); keys(sequence.player, 1); keys(sequence.boss, 2);
	keys(sequence.playerMotions, 3); keys(sequence.bossMotions, 4); keys(sequence.effects, 5); keys(sequence.particles, 6); keys(sequence.timeScales, 7); keys(sequence.sounds, 8);
	const float qteY = begin.y + rulerHeight + rowHeight * 9;
	draw->AddText({begin.x + 6,qteY + 8}, IM_COL32(225,230,240,255), "QTE / Attack");
	if (sequence.qteEnabled)
		draw->AddRectFilled({timeX(sequence.qteStart),qteY + 4}, {timeX(sequence.qteEnd),qteY + rowHeight - 4}, IM_COL32(155,110,30,200));
	draw->AddLine({timeX(cursor),begin.y}, {timeX(cursor),qteY + rowHeight}, IM_COL32(255,100,100,255), 2.0f);
	if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
	{
		dragUndoSaved = false;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("MovieKeyMenu");
		const auto& mouse = ImGui::GetIO().MousePos;
		const int track = static_cast<int>(std::floor((mouse.y - begin.y - rulerHeight) / rowHeight));
		if (track >= 0 && track < 9)
		{
			selectedTrack = track;
			const auto pick = [&](const auto& values) {
				int index = -1; float distance = 13;
				for (size_t i = 0; i < values.size(); ++i)
				{
					const float dx = std::abs(mouse.x - timeX(values[i].time));
					if (dx <= distance) { distance = dx; index = static_cast<int>(i); }
				}
				if (mouse.x < begin.x + labelWidth && !values.empty()) index = static_cast<int>(MovieKeyIndex(values, cursor));
				if (index >= 0) SelectKey(track, index);
				else Scrub((mouse.x - left) / timeWidth * sequence.duration);
			};
			switch (track)
			{
			case 0: pick(sequence.cameras); break;
			case 1: pick(sequence.player); break;
			case 2: pick(sequence.boss); break;
			case 3: pick(sequence.playerMotions); break;
			case 4: pick(sequence.bossMotions); break;
			case 5: pick(sequence.effects); break;
			case 6: pick(sequence.particles); break;
			case 7: pick(sequence.timeScales); break;
			case 8: pick(sequence.sounds); break;
			}
			if (selectedKey < 0 && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) RegisterKey();
		}
		else Scrub((mouse.x - left) / timeWidth * sequence.duration);
	}
	if (active && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::GetIO().MousePos.x >= left)
	{
		float time = std::clamp((ImGui::GetIO().MousePos.x - left) / timeWidth * sequence.duration, 0.0f, sequence.duration);
		const auto move = [&](auto& values) {
			if (selectedKey < 0 || selectedKey >= static_cast<int>(values.size())) { Scrub(time); return; }
			if (!dragUndoSaved) { PushUndo(); dragUndoSaved = true; }
			using Key = typename std::decay_t<decltype(values)>::value_type;
			const float minimum = selectedKey ? ((std::is_same_v<Key, ParticleKey> || std::is_same_v<Key, SoundKey>) ? values[selectedKey - 1].time :
				std::nextafter(values[selectedKey - 1].time, std::numeric_limits<float>::infinity())) : 0;
			const float maximum = selectedKey + 1 < static_cast<int>(values.size()) ?
				((std::is_same_v<Key, ParticleKey> || std::is_same_v<Key, SoundKey>) ? values[selectedKey + 1].time :
				std::nextafter(values[selectedKey + 1].time, -std::numeric_limits<float>::infinity())) : sequence.duration;
			time = std::clamp(time, minimum, maximum); values[selectedKey].time = time; dirty = true; Seek(time);
		};
		switch (selectedTrack)
		{
		case 0: move(sequence.cameras); break;
		case 1: move(sequence.player); break;
		case 2: move(sequence.boss); break;
		case 3: move(sequence.playerMotions); break;
		case 4: move(sequence.bossMotions); break;
		case 5: move(sequence.effects); break;
		case 6: move(sequence.particles); break;
		case 7: move(sequence.timeScales); break;
		case 8: move(sequence.sounds); break;
		}
	}
	if (ImGui::BeginPopup("MovieKeyMenu"))
	{
		if (ImGuiToolKit::MenuItem(u8"現在時刻にキーを登録", "K")) RegisterKey();
		if (ImGuiToolKit::MenuItem(u8"選択キーを削除", "Delete", false, selectedKey >= 0)) DeleteKey();
		if (ImGuiToolKit::MenuItem(u8"元に戻す", "Ctrl+Z", false, !undoStack.empty())) Undo();
		ImGui::EndPopup();
	}
}

void MovieCameraController::DrawKeyList()
{
	const auto list = [&](const auto& values) {
		char label[64];
		if (selectedKey >= 0 && selectedKey < static_cast<int>(values.size()))
			std::snprintf(label, sizeof(label), "Key %d / %.3f s", selectedKey + 1, values[selectedKey].time);
		else std::snprintf(label, sizeof(label), "New key / %.3f s", cursor);
		if (ImGuiToolKit::BeginCombo(u8"キー一覧", label))
		{
			for (size_t i = 0; i < values.size(); ++i)
			{
				std::snprintf(label, sizeof(label), "Key %zu / %.3f s", i + 1, values[i].time);
				if (ImGui::Selectable(label, selectedKey == static_cast<int>(i))) SelectKey(selectedTrack, static_cast<int>(i));
			}
			ImGui::EndCombo();
		}
	};
	switch (selectedTrack)
	{
	case 0: list(sequence.cameras); break;
	case 1: list(sequence.player); break;
	case 2: list(sequence.boss); break;
	case 3: list(sequence.playerMotions); break;
	case 4: list(sequence.bossMotions); break;
	case 5: list(sequence.effects); break;
	case 6: list(sequence.particles); break;
	case 7: list(sequence.timeScales); break;
	case 8: list(sequence.sounds); break;
	}
}

void MovieCameraController::DrawInspector()
{
	static const char* easingNames[] = { "Linear", "In Quad", "Out Quad", "In Out Quad",
		"In Cubic", "Out Cubic", "In Out Cubic", "In Sine", "Out Sine", "In Out Sine" };
	const char* tracks[] = { "Camera", "Player / Position", "Boss / Position", "Player / Motion", "Boss / Motion", "Post Effects", "Particles", "Time Scale", "Sounds" };
	int track = selectedTrack;
	if (ImGuiToolKit::Combo(u8"トラック", &track, tracks, 9)) { selectedTrack = track; selectedKey = -1; }
	DrawKeyList();
	ImGui::TextUnformatted(selectedKey >= 0 ? JText(u8"選択キーを編集（変更は即座に反映）") :
		JText(u8"現在時刻の値を編集するとキーを自動登録します"));
	if (ImGuiToolKit::Button(u8"現在の状態を登録 / K", {-1, 0})) RegisterKey();
	ImGui::Separator();
	const auto inspect = [&](auto& values, auto current) {
		using Key = std::decay_t<decltype(current)>;
		const bool selected = selectedKey >= 0 && selectedKey < static_cast<int>(values.size());
		Key key = selected ? values[selectedKey] : current;
		bool changed = ImGuiToolKit::DragDelayFloat(u8"キー時刻 (秒)", &key.time, 1.0f / 60.0f);
		const float minimum = selected && selectedKey ? ((std::is_same_v<Key, ParticleKey> || std::is_same_v<Key, SoundKey>) ? values[selectedKey - 1].time :
			std::nextafter(values[selectedKey - 1].time, std::numeric_limits<float>::infinity())) : 0;
		const float maximum = selected && selectedKey + 1 < static_cast<int>(values.size()) ?
			((std::is_same_v<Key, ParticleKey> || std::is_same_v<Key, SoundKey>) ? values[selectedKey + 1].time :
			std::nextafter(values[selectedKey + 1].time, -std::numeric_limits<float>::infinity())) : sequence.duration;
		key.time = std::clamp(key.time, minimum, maximum);
		if constexpr (!std::is_same_v<Key, MotionKey> && !std::is_same_v<Key, ParticleKey> && !std::is_same_v<Key, SoundKey>)
			changed |= ImGuiToolKit::Combo(u8"補間", &key.easing, easingNames, static_cast<int>(Easing::Type::Count));
		if constexpr (std::is_same_v<Key, CameraKey>)
		{
			changed |= ImGuiToolKit::DragPosFloat3(u8"位置", &key.eye.x, 0.05f);
			changed |= ImGuiToolKit::DragPosFloat3(u8"注視点", &key.focus.x, 0.05f);
			changed |= ImGui::SliderFloat("FOV", &key.fov, 10, 120);
			changed |= ImGuiToolKit::Checkbox(u8"このキーでカメラを切り替え", &key.cut);
		}
		else if constexpr (std::is_same_v<Key, ActorKey>)
		{
			changed |= ImGuiToolKit::DragPosFloat3(u8"配置位置", &key.position.x, 0.05f);
			changed |= ImGuiToolKit::DragRotFloat(u8"向き (度)", &key.yaw, 0.5f);
		}
		else if constexpr (std::is_same_v<Key, TimeScaleKey>)
		{
			changed |= ImGuiToolKit::DragSpeedFloat(u8"時間スケール", &key.scale, 0.01f, 0.01f, 10.0f);
			changed |= ImGuiToolKit::Checkbox(u8"次のキーへ補間", &key.interpolate);
			key.scale = std::clamp(key.scale, 0.01f, 10.0f);
			ImGuiToolKit::TextWrapped(u8"1: 通常速度 / 0.2: 5倍スロー。カメラ・配置・モーション・エフェクトをまとめて変更します。");
		}
		else if constexpr (std::is_same_v<Key, ParticleKey>)
		{
			if (!particlePathsLoaded)
			{
				particlePaths = EffectManager::GetEffectPaths();
				particlePathsLoaded = true;
			}
			if (ImGuiToolKit::BeginCombo(u8"エフェクト", key.path.empty() ? JText(u8"未設定") : key.path.c_str()))
			{
				ImGui::InputTextWithHint("##MovieParticleSearch", JText(u8"エフェクト名で検索"), &particleSearch);
				const ImGuiTextFilter filter(particleSearch.c_str());
				for (const auto& path : particlePaths)
				{
					if (!filter.PassFilter(path.c_str())) continue;
					if (ImGui::Selectable(path.c_str(), key.path == path)) { key.path = path; changed = true; }
				}
				ImGui::EndCombo();
			}
			if (ImGuiToolKit::Button(u8"エフェクト一覧を更新")) particlePathsLoaded = false;
			changed |= ImGuiToolKit::InputTextWithHint(u8"パス", "Resources/Effect/...efkpkg", &key.path);
			changed |= ImGuiToolKit::DragDelayFloat(u8"表示時間 (秒)", &key.duration, 0.05f, 0.01f, 3600.0f);
			const char* targets[] = {JText(u8"ムービー基準位置"), JText(u8"プレイヤー"), JText(u8"ボス")};
			changed |= ImGuiToolKit::Combo(u8"配置基準", &key.target, targets, 3);
			changed |= ImGuiToolKit::Checkbox(u8"配置基準へ追従", &key.follow);
			changed |= ImGuiToolKit::DragPosFloat3(u8"位置 / オフセット", &key.position.x, 0.05f);
			changed |= ImGuiToolKit::DragRotFloat3(u8"回転 (度)", &key.rotation.x, 0.5f);
			changed |= ImGuiToolKit::DragSizeFloat3(u8"サイズ", &key.scale.x, 0.01f, 0.001f, 1000.0f);
			changed |= ImGuiToolKit::DragSpeedFloat(u8"再生速度 (0で停止)", &key.speed, 0.01f, 0.0f, 10.0f);
			changed |= ImGuiToolKit::Checkbox(u8"ビルボード", &key.billboard);
			changed |= ImGuiToolKit::InputInt(u8"ランダムシード", &key.seed);
			key.duration = std::clamp(key.duration, 0.01f, 3600.0f);
			key.speed = std::clamp(key.speed, 0.0f, 10.0f);
			key.scale.x = std::clamp(key.scale.x, 0.001f, 1000.0f);
			key.scale.y = std::clamp(key.scale.y, 0.001f, 1000.0f);
			key.scale.z = std::clamp(key.scale.z, 0.001f, 1000.0f);
			particleDraft = key;
			ImGuiToolKit::TextWrapped(u8"登録時刻に一度発生し、表示時間の終了・ムービー終了で停止します。同時刻にも複数登録できます。");
		}
		else if constexpr (std::is_same_v<Key, SoundKey>)
		{
			if (!soundPathsLoaded) { soundPaths = SoundSystem::GetSoundPaths(); soundPathsLoaded = true; }
			if (ImGuiToolKit::BeginCombo(u8"サウンド", key.path.empty() ? JText(u8"未設定") : key.path.c_str()))
			{
				ImGui::InputTextWithHint("##MovieSoundSearch", JText(u8"サウンド名で検索"), &soundSearch);
				const ImGuiTextFilter filter(soundSearch.c_str());
				for (const auto& path : soundPaths)
				{
					if (!filter.PassFilter(path.c_str())) continue;
					if (ImGui::Selectable(path.c_str(), key.path == path)) { key.path = path; changed = true; }
				}
				ImGui::EndCombo();
			}
			if (ImGuiToolKit::Button(u8"サウンド一覧を更新")) soundPathsLoaded = false;
			changed |= ImGuiToolKit::InputTextWithHint(u8"パス", "SE/Threat", &key.path);
			changed |= ImGuiToolKit::DragDelayFloat(u8"再生時間 (秒)", &key.duration, 0.05f, 0.01f, 3600.0f);
			changed |= ImGuiToolKit::DragFloat(u8"音量", &key.volume, 0.01f, 0.0f, 4.0f);
			changed |= ImGuiToolKit::DragFloat(u8"ピッチ", &key.pitch, 0.01f, 0.01f, 2.0f);
			changed |= ImGuiToolKit::SliderFloat(u8"左右バランス", &key.pan, -1.0f, 1.0f);
			changed |= ImGuiToolKit::Checkbox(u8"サウンドをループ", &key.loop);
			key.duration = std::clamp(key.duration, 0.01f, 3600.0f);
			key.volume = std::clamp(key.volume, 0.0f, 4.0f);
			key.pitch = std::clamp(key.pitch, 0.01f, 2.0f);
			if (ImGuiToolKit::Button(u8"試聴"))
			{
				playing = false;
				UpdateSounds(cursor);
				SoundSystem::Instance().SetPaused(false);
				if (previewSound) SoundSystem::Instance().Stop(previewSound);
				SoundSystem::PlayOptions options;
				options.volume = key.volume; options.pitch = key.pitch; options.pan = key.pan; options.loop = key.loop;
				previewSound = SoundSystem::Instance().Play(key.path, options);
			}
			ImGui::SameLine();
			if (ImGuiToolKit::Button(u8"試聴停止"))
			{
				SoundSystem::Instance().Stop(previewSound);
				previewSound = SoundSystem::InvalidVoiceId;
			}
			soundDraft = key;
			ImGuiToolKit::TextWrapped(u8"再生中にキーを通過すると発音します。シーク中は発音せず、再生時間の終了で停止します。スローは発音時刻に適用し、音のピッチはこの設定を使います。");
		}
		else if constexpr (std::is_same_v<Key, EffectKey>)
		{
			Game::PostProcess editor;
			editor.ApplyMovieSettings(key.settings);
			ImGuiToolKit::TextUnformatted(u8"数値・色は補間、有効/無効・方式はキーで切り替え");
			editor.DrawGUI();
			const json settings = editor.CaptureMovieSettings();
			if (settings != key.settings) { key.settings = settings; changed = true; }
		}
		else
		{
			auto* model = (selectedTrack == 3 ? playerState : bossState).renderer->GetModel();
			if (ImGuiToolKit::BeginCombo(u8"アニメーション", key.animation.c_str()))
			{
				if (ImGuiToolKit::Selectable(u8"編集開始時の姿勢", key.animation.empty())) { key.animation.clear(); changed = true; }
				for (const auto& animation : model->GetAnimations())
					if (ImGui::Selectable(animation.name.c_str(), key.animation == animation.name))
					{ key.animation = animation.name; key.offset = 0; changed = true; }
				ImGui::EndCombo();
			}
			changed |= ImGuiToolKit::DragDelayFloat(u8"モーション開始秒", &key.offset, 0.01f, 0, 3600);
			changed |= ImGuiToolKit::DragSpeedFloat(u8"再生速度 (0で姿勢固定)", &key.speed, 0.01f, 0, 10);
			changed |= ImGuiToolKit::Checkbox(u8"モーションループ", &key.loop);
			key.offset = std::max(key.offset, 0.0f); key.speed = std::clamp(key.speed, 0.0f, 10.0f);
		}
		if (changed)
		{
			if (!editUndoSaved) { PushUndo(); editUndoSaved = true; }
			if (selected) values[selectedKey] = key;
			else { StoreMovieKey(values, key); selectedKey = static_cast<int>(MovieKeyIndex(values, key.time)); }
			dirty = true; playing = false;
			if constexpr (std::is_same_v<Key, CameraKey>) freeView = false;
			Seek(key.time);
		}
		if (selected && ImGuiToolKit::Button(u8"1フレーム後に複製"))
		{
			key.time = std::min(key.time + 1.0f / 60.0f, sequence.duration);
			PushUndo(); StoreMovieKey(values, key); dirty = true;
			SelectKey(selectedTrack, static_cast<int>(MovieKeyIndex(values, key.time)));
		}
	};
	if (selectedTrack == 0)
	{
		SyncCameraToController(*camera);
		inspect(sequence.cameras, CameraKey{cursor, eye - origin, focus - origin, fov});
	}
	else if (selectedTrack == 1 || selectedTrack == 2)
	{
		const auto& state = selectedTrack == 1 ? playerState : bossState;
		const Vector3 forward = state.actor->transform.GetForward();
		ActorKey current{cursor, state.actor->transform.position - origin,
			DirectX::XMConvertToDegrees(std::atan2(forward.x, forward.z))};
		if (selectedTrack == 1) inspect(sequence.player, current); else inspect(sequence.boss, current);
	}
	else if (selectedTrack == 3 || selectedTrack == 4)
	{
		auto& values = selectedTrack == 3 ? sequence.playerMotions : sequence.bossMotions;
		MotionKey current;
		if (!values.empty()) { current = values[MovieKeyIndex(values, cursor)]; current.offset += std::max(cursor - current.time, 0.0f) * current.speed; }
		current.time = cursor; inspect(values, current);
	}
	else if (selectedTrack == 5) inspect(sequence.effects, EffectKey{cursor, currentPostSettings});
	else if (selectedTrack == 6)
	{
		ParticleKey key = particleDraft;
		key.time = cursor;
		inspect(sequence.particles, key);
	}
	else if (selectedTrack == 7) inspect(sequence.timeScales, TimeScaleKey{cursor, GetMovieTimeScale(cursor)});
	else
	{
		SoundKey key = soundDraft;
		key.time = cursor;
		inspect(sequence.sounds, key);
	}
	ImGui::Separator();
	if (ImGuiToolKit::Button(u8"選択キーを削除 / Delete", {-1, 0})) DeleteKey();
}

void MovieCameraController::DrawEditor()
{
	PROFILE_FUNCTION();
	if (!editing) return;
	const auto* viewport = ImGui::GetMainViewport();
	if (runtimePlayback) return;
	ImGui::SetNextWindowSize({std::min(1260.0f, viewport->Size.x - 24), std::min(620.0f, viewport->Size.y * 0.65f)}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos({viewport->Pos.x + 12, viewport->Pos.y + viewport->Size.y - 12}, ImGuiCond_FirstUseEver, {0,1});
	ImGui::SetNextWindowSizeConstraints({760, 440}, {10000, 10000});
	bool open = true;
	if (ImGuiToolKit::Begin(u8"Movie Editor / ムービー編集", &open))
	{
		if (ImGuiToolKit::Button(u8"保存 / Ctrl+S")) Save(documentPath);
		ImGui::SameLine();
		if (ImGuiToolKit::Button(u8"別名保存"))
		{
			std::string filename = ResourcePath::Text(documentPath);
			if (Dialog::SaveFileName(filename, "Movie (*.vmovie)\0*.vmovie\0", "Save Movie", "vmovie") == DialogResult::OK)
			{ auto path = ResourcePath::Path(filename); path.replace_extension(".vmovie"); Save(path); }
		}
		ImGui::SameLine();
		if (ImGuiToolKit::Button(u8"開く"))
		{
			std::string filename;
			if (Dialog::OpenFileName(filename, "Movie (*.vmovie)\0*.vmovie\0", "Open Movie") == DialogResult::OK)
				Load(ResourcePath::Path(filename));
		}
		ImGui::SameLine(); if (ImGuiToolKit::Button(u8"元に戻す")) Undo();
		ImGui::SameLine(); if (ImGuiToolKit::Button(u8"やり直す")) Redo();
		ImGui::SameLine(); if (ImGuiToolKit::Button(u8"終了 / F6")) closeRequested = true;
		ImGui::Text("%s%s", ResourcePath::Text(documentPath).c_str(), dirty ? " *" : "");
		ImGui::Separator();
		const float inspectorWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.36f, 300.0f, 450.0f);
		ImGui::BeginChild("MovieTimelinePane", {std::max(ImGui::GetContentRegionAvail().x - inspectorWidth - 10, 220.0f), 0}, true);
		if (ImGui::Button(playing ? JText(u8"一時停止") : JText(u8"再生")))
		{ if (cursor >= sequence.duration) Seek(0); freeView = false; playing = !playing; }
		ImGui::SameLine();
		if (ImGuiToolKit::Button(u8"先頭から再生")) { Scrub(0); playing = true; }
		ImGui::SameLine(); ImGuiToolKit::Checkbox(u8"ループ", &loop);
		ImGui::Text("Time Scale: %.3f", GetMovieTimeScale(cursor));
		float time = cursor;
		if (ImGuiToolKit::SliderFloat(u8"時刻", &time, 0, sequence.duration, "%.3f s")) Scrub(time);
		ImGui::SameLine();
		if (ImGui::Button("-1F")) Scrub(cursor - 1.0f / 60.0f);
		ImGui::SameLine(); if (ImGui::Button("+1F")) Scrub(cursor + 1.0f / 60.0f);
		if (ImGuiToolKit::Checkbox(u8"自由カメラ", &freeView))
		{ playing = false; if (freeView) freeCamera->SyncCameraToController(*camera); else Evaluate(); }
		ImGui::SameLine(); if (ImGuiToolKit::Button(u8"＋キー / K")) RegisterKey();
		ImGuiToolKit::TextUnformatted(u8"キーをクリック→右に詳細 / 空白をダブルクリック→追加");
		DrawTimeline();
		if (ImGuiToolKit::CollapsingHeader(u8"ムービー設定 / QTE"))
		{
			float duration = sequence.duration;
			if (ImGuiToolKit::DragDelayFloat(u8"長さ (秒)", &duration, 0.1f, 0.05f, 3600))
			{
				float minimum = 0.05f;
				const auto last = [&](const auto& values) { if (!values.empty()) minimum = std::max(minimum, values.back().time); };
				last(sequence.cameras); last(sequence.player); last(sequence.boss); last(sequence.playerMotions); last(sequence.bossMotions); last(sequence.effects); last(sequence.particles); last(sequence.timeScales); last(sequence.sounds);
				PushUndo(); sequence.duration = std::clamp(duration, minimum, 3600.0f);
				sequence.qteEnd = std::clamp(sequence.qteEnd, 0.02f, sequence.duration);
				sequence.qteStart = std::clamp(sequence.qteStart, 0.0f, sequence.qteEnd - 0.01f); dirty = true; Scrub(cursor);
			}
			if (ImGuiToolKit::DragPosFloat3(u8"基準位置 (保存対象外)", &origin.x, 0.1f)) Evaluate();
			if (ImGuiToolKit::Button(u8"ステージ中央へ戻す")) { PlaceAtStageCenter(); Seek(cursor); }
			bool enabled = sequence.qteEnabled; float start = sequence.qteStart, end = sequence.qteEnd;
			bool changed = ImGuiToolKit::Checkbox(u8"攻撃入力判定プレビュー", &enabled);
			changed |= ImGuiToolKit::DragDelayFloat(u8"受付開始", &start, 0.05f);
			changed |= ImGuiToolKit::DragDelayFloat(u8"受付終了", &end, 0.05f);
			if (changed)
			{
				PushUndo(); sequence.qteEnabled = enabled;
				sequence.qteStart = std::clamp(start, 0.0f, sequence.duration - 0.01f);
				sequence.qteEnd = std::clamp(end, sequence.qteStart + 0.01f, sequence.duration); dirty = true; qteResult = 0;
			}
		}
		ImGui::TextWrapped("%s", JText(u8"右ドラッグ＋WASDQE:自由カメラ / K:登録 / Delete:削除 / Ctrl+Z,Y:元に戻す・やり直す"));
		if (qteResult) ImGui::TextUnformatted(qteResult > 0 ? "QTE: success" : "QTE: failed");
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("MovieInspectorPane", {0,0}, true);
		ImGuiToolKit::TextUnformatted(u8"キー詳細 / 現在時刻の値");
		DrawInspector();
		ImGui::EndChild();
	}
	ImGui::End();
	if (!open) closeRequested = true;

}
