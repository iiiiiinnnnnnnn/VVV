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
	if (!camera || !freeCamera) { message = "Movie camera requires Camera and FreeCameraController."; return false; }
	try
	{
		if (!boss)
		{
			temporaryBoss = std::make_shared<Actor>("Scene Boss Preview");
			auto model = ResourceManager::Instance().LoadModel("Aracore", true);
			model->ApplyInitialMorphs();
			auto* renderer = temporaryBoss->AddComponent<VMDLModelComponent>(
				model, ModelShaderId::VMat, VMatRenderParams{}, false);
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
		for (const auto& state : controllerStates) state.controller->SetActive(false);
		Terrain* terrain = stage->GetComponent<Terrain>();
		if (!terrain) terrain = stage->GetTerrain();
		origin = temporaryBoss ? player.transform.position : boss->transform.position;
		if (terrain)
		{
			const Matrix world = stage->GetTransform()->GetMatrix();
			Vector3 local = Vector3::Transform(origin, world.Invert());
			local.y = terrain->GetSurfaceHeightByUV(local.x / terrain->GetTerrainSize() + 0.5f,
				local.z / terrain->GetTerrainSize() + 0.5f);
			origin = Vector3::Transform(local, world);
		}
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
		Seek(0.0f);
		return true;
	}
	catch (const std::exception& error)
	{
		message = error.what();
		if (editing) End();
		else if (temporaryBoss) { temporaryBoss->SetActive(false); temporaryBoss->Destroy(); temporaryBoss.reset(); }
		return false;
	}
}

void MovieCameraController::End()
{
	if (!editing) return;
	playing = false;
	for (ActorState* state : { &playerState, &bossState })
	{
		state->actor->transform = state->transform;
		state->renderer->GetModel()->SetNodePoses(state->pose);
		state->renderer->UpdateModelTransform(state->transform.GetMatrix());
	}
	for (const auto& state : visibility) state.actor->SetActive(state.active);
	playerState.actor->SetActive(playerState.active);
	bossState.actor->SetActive(bossState.active);
	camera->SetLookAt(savedEye, savedFocus, savedUp);
	const float aspect = std::max(Game::Graphics::ScreenWidth, 1.0f) / std::max(Game::Graphics::ScreenHeight, 1.0f);
	camera->SetPerspectiveFov(RAD(savedFov), aspect, 0.1f, 1000.0f);
	for (const auto& state : cameraStates) state.camera->SetPriority(state.priority);
	for (const auto& state : controllerStates) state.controller->SetActive(state.active);
	Game::Time::paused = wasPaused;
	Game::Time::deltaTime = 0.0f;
	SoundSystem::Instance().SetPaused(wasPaused);
	if (moviePostProcess) moviePostProcess->ApplyMovieSettings(savedPostSettings);
	moviePostProcess = nullptr;
	if (temporaryBoss) { temporaryBoss->SetActive(false); temporaryBoss->Destroy(); temporaryBoss.reset(); }
	editing = false; closeRequested = false; freeView = false;
	stage = nullptr;
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
	undoStack.clear(); redoStack.clear();
	dirty = false;
}

void MovieCameraController::Seek(float time)
{
	cursor = std::clamp(time, 0.0f, sequence.duration);
	qteResult = 0;
	Evaluate();
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

void MovieCameraController::Evaluate()
{
	if (!editing || sequence.cameras.empty()) return;
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
	if (freeView) return;
	const size_t index = MovieKeyIndex(sequence.cameras, cursor);
	const auto& a = sequence.cameras[index];
	const auto& b = sequence.cameras[std::min(index + 1, sequence.cameras.size() - 1)];
	const float blend = b.cut ? 0.0f : MovieKeyBlend(cursor, a.time, b.time, a.easing);
	eye = origin + Vector3::Lerp(a.eye, b.eye, blend);
	focus = origin + Vector3::Lerp(a.focus, b.focus, blend);
	fov = std::lerp(a.fov, b.fov, blend);
	SyncControllerToCamera(*camera);
}

void MovieCameraController::Tick()
{
	if (!editing) return;
	Game::Time::deltaTime = 0.0f;
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
	if (!playing) return;
	cursor += Game::Time::unscaledDeltaTime;
	if (sequence.qteEnabled && !qteResult)
	{
		if (cursor >= sequence.qteStart && cursor <= sequence.qteEnd &&
			!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false)) qteResult = 1;
		else if (cursor > sequence.qteEnd) qteResult = -1;
	}
	if (cursor >= sequence.duration)
	{
		if (loop) { cursor = std::fmod(cursor, sequence.duration); qteResult = 0; }
		else { cursor = sequence.duration; playing = false; }
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
			if (!std::isfinite(key.time) || key.time < 0.0f || key.time > movie.duration || key.time <= previous)
				throw std::runtime_error("Movie key times must be ordered, unique and within the duration.");
			previous = key.time;
			using Key = std::decay_t<decltype(key)>;
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
			if constexpr (!std::is_same_v<Key, MovieCameraController::MotionKey>)
				if (key.easing < 0 || key.easing >= static_cast<int>(Easing::Type::Count))
					throw std::runtime_error("Invalid movie easing.");
		}
	};
	validate(movie.cameras, true); validate(movie.player, true); validate(movie.boss, true);
	validate(movie.playerMotions, false); validate(movie.bossMotions, false);
	validate(movie.effects, true);
}

bool MovieCameraController::Save(const std::filesystem::path& path)
{
	std::filesystem::path temporary;
	try
	{
		ValidateMovieSequence(sequence);
		json root = {{"format", "VEER.Movie"}, {"version", 2}, {"duration", sequence.duration},
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
		auto destination = path; destination.replace_extension(".vmovie");
		if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path());
		temporary = destination; temporary += L".saving.tmp";
		std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
		stream << root.dump(2);
		stream.close();
		if (!stream || !MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Failed to save movie timeline.");
		documentPath = destination; dirty = false;
		message = "Saved: " + ResourcePath::Text(destination);
		return true;
	}
	catch (const std::exception& error)
	{
		if (!temporary.empty()) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
		message = error.what();
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
		const bool legacy = root.at("format") == "VEER.Scene" && root.at("version") == 1;
		if (!legacy && (root.at("format") != "VEER.Movie" || root.at("version") != 2))
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
		if (root.contains("effects"))
			for (const auto& item : root.at("effects"))
			{
				json settings = savedPostSettings;
				settings.update(item.at("settings"));
				candidate.effects.push_back({item.at("time").get<float>(), std::move(settings), item.at("easing").get<int>()});
			}
		if (candidate.effects.empty()) candidate.effects.push_back({0.0f, savedPostSettings});
		ValidateMovieSequence(candidate);
		sequence = std::move(candidate);
		documentPath = path; documentPath.replace_extension(".vmovie");
		undoStack.clear(); redoStack.clear();
		dirty = false; playing = false; freeView = false;
		selectedTrack = 0; selectedKey = -1;
		Seek(0.0f);
		message = "Loaded: " + ResourcePath::Text(path);
		return true;
	}
	catch (const std::exception& error) { message = error.what(); return false; }
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
	else
	{
		EffectKey key{cursor, currentPostSettings};
		if (!sequence.effects.empty()) key.easing = sequence.effects[MovieKeyIndex(sequence.effects, cursor)].easing;
		store(sequence.effects, key);
	}
	dirty = true; playing = false;
}

void MovieCameraController::DeleteKey()
{
	bool erased = false;
	const auto erase = [&](auto& keys, bool required) {
		if (selectedKey < 0 || selectedKey >= static_cast<int>(keys.size())) return;
		if (required && keys.size() == 1) { message = (const char*)u8"このトラックには最低1個のキーが必要です。"; return; }
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
	}
	if (erased) { selectedKey = -1; dirty = true; Evaluate(); }
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
	ImGui::InvisibleButton("MovieTimelineCanvas", {width, rulerHeight + rowHeight * 7},
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool hovered = ImGui::IsItemHovered();
	const bool active = ImGui::IsItemActive();
	auto* draw = ImGui::GetWindowDrawList();
	draw->AddRectFilled(begin, {begin.x + width, begin.y + rulerHeight + rowHeight * 7}, IM_COL32(23,26,33,255));
	for (int tick = 0; tick <= 10; ++tick)
	{
		char text[24]; std::snprintf(text, sizeof(text), "%.2f", sequence.duration * tick / 10.0f);
		const float x = left + timeWidth * tick / 10.0f;
		draw->AddText({x - ImGui::CalcTextSize(text).x * 0.5f,begin.y + 3}, IM_COL32(180,190,205,255), text);
		draw->AddLine({x,begin.y + rulerHeight}, {x,begin.y + rulerHeight + rowHeight * 7}, IM_COL32(50,55,65,255));
	}
	const char* labels[] = { "Camera", "Player / Position", "Boss / Position", "Player / Motion", "Boss / Motion", "Post Effects" };
	for (int track = 0; track < 6; ++track)
	{
		const float y = begin.y + rulerHeight + rowHeight * track;
		if (selectedTrack == track) draw->AddRectFilled({begin.x,y}, {begin.x + width,y + rowHeight}, IM_COL32(40,58,78,150));
		draw->AddText({begin.x + 6,y + 8}, IM_COL32(225,230,240,255), labels[track]);
		draw->AddLine({begin.x,y + rowHeight}, {begin.x + width,y + rowHeight}, IM_COL32(60,65,75,255));
	}
	const auto keys = [&](const auto& values, int track) {
		const float y = begin.y + rulerHeight + rowHeight * (track + 0.5f);
		for (size_t i = 0; i < values.size(); ++i)
		{
			const bool selected = selectedTrack == track && selectedKey == static_cast<int>(i);
			const float x = timeX(values[i].time);
			draw->AddCircleFilled({x,y}, selected ? 7.0f : 5.5f,
				selected ? IM_COL32(255,190,60,255) : IM_COL32(90,180,255,255));
			if (selected) draw->AddCircle({x,y}, 10.0f, IM_COL32(255,220,130,255));
			if (hovered && std::abs(ImGui::GetIO().MousePos.x - x) <= 12 && std::abs(ImGui::GetIO().MousePos.y - y) <= 14)
				ImGui::SetTooltip("%s / Key %zu / %.3f s", labels[track], i + 1, values[i].time);
		}
	};
	keys(sequence.cameras, 0); keys(sequence.player, 1); keys(sequence.boss, 2);
	keys(sequence.playerMotions, 3); keys(sequence.bossMotions, 4); keys(sequence.effects, 5);
	const float qteY = begin.y + rulerHeight + rowHeight * 6;
	draw->AddText({begin.x + 6,qteY + 8}, IM_COL32(225,230,240,255), "QTE / SPACE");
	if (sequence.qteEnabled)
		draw->AddRectFilled({timeX(sequence.qteStart),qteY + 4}, {timeX(sequence.qteEnd),qteY + rowHeight - 4}, IM_COL32(155,110,30,200));
	draw->AddLine({timeX(cursor),begin.y}, {timeX(cursor),qteY + rowHeight}, IM_COL32(255,100,100,255), 2.0f);
	if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)))
	{
		dragUndoSaved = false;
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("MovieKeyMenu");
		const auto& mouse = ImGui::GetIO().MousePos;
		const int track = static_cast<int>(std::floor((mouse.y - begin.y - rulerHeight) / rowHeight));
		if (track >= 0 && track < 6)
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
			const float minimum = selectedKey ? std::nextafter(values[selectedKey - 1].time, std::numeric_limits<float>::infinity()) : 0;
			const float maximum = selectedKey + 1 < static_cast<int>(values.size()) ?
				std::nextafter(values[selectedKey + 1].time, -std::numeric_limits<float>::infinity()) : sequence.duration;
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
		}
	}
	if (ImGui::BeginPopup("MovieKeyMenu"))
	{
		if (ImGui::MenuItem((const char*)u8"現在時刻にキーを登録", "K")) RegisterKey();
		if (ImGui::MenuItem((const char*)u8"選択キーを削除", "Delete", false, selectedKey >= 0)) DeleteKey();
		if (ImGui::MenuItem((const char*)u8"元に戻す", "Ctrl+Z", false, !undoStack.empty())) Undo();
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
		if (ImGui::BeginCombo((const char*)u8"キー一覧", label))
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
	}
}

void MovieCameraController::DrawInspector()
{
	static const char* easingNames[] = { "Linear", "In Quad", "Out Quad", "In Out Quad",
		"In Cubic", "Out Cubic", "In Out Cubic", "In Sine", "Out Sine", "In Out Sine" };
	const char* tracks[] = { "Camera", "Player / Position", "Boss / Position", "Player / Motion", "Boss / Motion", "Post Effects" };
	int track = selectedTrack;
	if (ImGui::Combo((const char*)u8"トラック", &track, tracks, 6)) { selectedTrack = track; selectedKey = -1; }
	DrawKeyList();
	ImGui::TextUnformatted(selectedKey >= 0 ? (const char*)u8"選択キーを編集（変更は即座に反映）" :
		(const char*)u8"現在時刻の値を編集するとキーを自動登録します");
	if (ImGui::Button((const char*)u8"現在の状態を登録 / K", {-1, 0})) RegisterKey();
	ImGui::Separator();
	const auto inspect = [&](auto& values, auto current) {
		using Key = std::decay_t<decltype(current)>;
		const bool selected = selectedKey >= 0 && selectedKey < static_cast<int>(values.size());
		Key key = selected ? values[selectedKey] : current;
		bool changed = ImGui::DragFloat((const char*)u8"キー時刻 (秒)", &key.time, 1.0f / 60.0f);
		const float minimum = selected && selectedKey ? std::nextafter(values[selectedKey - 1].time, std::numeric_limits<float>::infinity()) : 0;
		const float maximum = selected && selectedKey + 1 < static_cast<int>(values.size()) ?
			std::nextafter(values[selectedKey + 1].time, -std::numeric_limits<float>::infinity()) : sequence.duration;
		key.time = std::clamp(key.time, minimum, maximum);
		if constexpr (!std::is_same_v<Key, MotionKey>)
			changed |= ImGui::Combo((const char*)u8"補間", &key.easing, easingNames, static_cast<int>(Easing::Type::Count));
		if constexpr (std::is_same_v<Key, CameraKey>)
		{
			changed |= ImGui::DragFloat3((const char*)u8"位置", &key.eye.x, 0.05f);
			changed |= ImGui::DragFloat3((const char*)u8"注視点", &key.focus.x, 0.05f);
			changed |= ImGui::SliderFloat("FOV", &key.fov, 10, 120);
			changed |= ImGui::Checkbox((const char*)u8"このキーでカメラを切り替え", &key.cut);
		}
		else if constexpr (std::is_same_v<Key, ActorKey>)
		{
			changed |= ImGui::DragFloat3((const char*)u8"配置位置", &key.position.x, 0.05f);
			changed |= ImGui::DragFloat((const char*)u8"向き (度)", &key.yaw, 0.5f);
		}
		else if constexpr (std::is_same_v<Key, EffectKey>)
		{
			Game::PostProcess editor;
			editor.ApplyMovieSettings(key.settings);
			ImGui::TextUnformatted((const char*)u8"数値・色は補間、有効/無効・方式はキーで切り替え");
			editor.DrawGUI();
			const json settings = editor.CaptureMovieSettings();
			if (settings != key.settings) { key.settings = settings; changed = true; }
		}
		else
		{
			auto* model = (selectedTrack == 3 ? playerState : bossState).renderer->GetModel();
			if (ImGui::BeginCombo((const char*)u8"アニメーション", key.animation.c_str()))
			{
				if (ImGui::Selectable((const char*)u8"編集開始時の姿勢", key.animation.empty())) { key.animation.clear(); changed = true; }
				for (const auto& animation : model->GetAnimations())
					if (ImGui::Selectable(animation.name.c_str(), key.animation == animation.name))
					{ key.animation = animation.name; key.offset = 0; changed = true; }
				ImGui::EndCombo();
			}
			changed |= ImGui::DragFloat((const char*)u8"モーション開始秒", &key.offset, 0.01f, 0, 3600);
			changed |= ImGui::DragFloat((const char*)u8"再生速度 (0で姿勢固定)", &key.speed, 0.01f, 0, 10);
			changed |= ImGui::Checkbox((const char*)u8"モーションループ", &key.loop);
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
		if (selected && ImGui::Button((const char*)u8"1フレーム後に複製"))
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
	else inspect(sequence.effects, EffectKey{cursor, currentPostSettings});
	ImGui::Separator();
	if (ImGui::Button((const char*)u8"選択キーを削除 / Delete", {-1, 0})) DeleteKey();
}

void MovieCameraController::DrawEditor()
{
	if (!editing) return;
	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowSize({std::min(1260.0f, viewport->Size.x - 24), std::min(620.0f, viewport->Size.y * 0.65f)}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos({viewport->Pos.x + 12, viewport->Pos.y + viewport->Size.y - 12}, ImGuiCond_FirstUseEver, {0,1});
	ImGui::SetNextWindowSizeConstraints({760, 440}, {10000, 10000});
	bool open = true;
	if (ImGui::Begin((const char*)u8"Movie Editor / ムービー編集", &open))
	{
		if (ImGui::Button((const char*)u8"保存 / Ctrl+S")) Save(documentPath);
		ImGui::SameLine();
		if (ImGui::Button((const char*)u8"別名保存"))
		{
			std::string filename = ResourcePath::Text(documentPath);
			if (Dialog::SaveFileName(filename, "Movie (*.vmovie)\0*.vmovie\0", "Save Movie", "vmovie") == DialogResult::OK)
			{ auto path = ResourcePath::Path(filename); path.replace_extension(".vmovie"); Save(path); }
		}
		ImGui::SameLine();
		if (ImGui::Button((const char*)u8"開く"))
		{
			std::string filename;
			if (Dialog::OpenFileName(filename, "Movie (*.vmovie)\0*.vmovie\0Legacy (*.vscene)\0*.vscene\0", "Open Movie") == DialogResult::OK)
				Load(ResourcePath::Path(filename));
		}
		ImGui::SameLine(); if (ImGui::Button((const char*)u8"元に戻す")) Undo();
		ImGui::SameLine(); if (ImGui::Button((const char*)u8"やり直す")) Redo();
		ImGui::SameLine(); if (ImGui::Button((const char*)u8"終了 / F6")) closeRequested = true;
		ImGui::Text("%s%s", ResourcePath::Text(documentPath).c_str(), dirty ? " *" : "");
		ImGui::Separator();
		const float inspectorWidth = std::clamp(ImGui::GetContentRegionAvail().x * 0.36f, 300.0f, 450.0f);
		ImGui::BeginChild("MovieTimelinePane", {std::max(ImGui::GetContentRegionAvail().x - inspectorWidth - 10, 220.0f), 0}, true);
		if (ImGui::Button(playing ? (const char*)u8"一時停止" : (const char*)u8"再生"))
		{ if (cursor >= sequence.duration) Seek(0); freeView = false; playing = !playing; }
		ImGui::SameLine();
		if (ImGui::Button((const char*)u8"先頭から再生")) { Scrub(0); playing = true; }
		ImGui::SameLine(); ImGui::Checkbox((const char*)u8"ループ", &loop);
		float time = cursor;
		if (ImGui::SliderFloat((const char*)u8"時刻", &time, 0, sequence.duration, "%.3f s")) Scrub(time);
		ImGui::SameLine();
		if (ImGui::Button("-1F")) Scrub(cursor - 1.0f / 60.0f);
		ImGui::SameLine(); if (ImGui::Button("+1F")) Scrub(cursor + 1.0f / 60.0f);
		if (ImGui::Checkbox((const char*)u8"自由カメラ", &freeView))
		{ playing = false; if (freeView) freeCamera->SyncCameraToController(*camera); else Evaluate(); }
		ImGui::SameLine(); if (ImGui::Button((const char*)u8"＋キー / K")) RegisterKey();
		ImGui::TextUnformatted((const char*)u8"キーをクリック→右に詳細 / 空白をダブルクリック→追加");
		DrawTimeline();
		if (ImGui::CollapsingHeader((const char*)u8"ムービー設定 / QTE"))
		{
			float duration = sequence.duration;
			if (ImGui::DragFloat((const char*)u8"長さ (秒)", &duration, 0.1f, 0.05f, 3600))
			{
				float minimum = 0.05f;
				const auto last = [&](const auto& values) { if (!values.empty()) minimum = std::max(minimum, values.back().time); };
				last(sequence.cameras); last(sequence.player); last(sequence.boss); last(sequence.playerMotions); last(sequence.bossMotions); last(sequence.effects);
				PushUndo(); sequence.duration = std::clamp(duration, minimum, 3600.0f);
				sequence.qteEnd = std::clamp(sequence.qteEnd, 0.02f, sequence.duration);
				sequence.qteStart = std::clamp(sequence.qteStart, 0.0f, sequence.qteEnd - 0.01f); dirty = true; Scrub(cursor);
			}
			if (ImGui::DragFloat3((const char*)u8"基準位置 (保存対象外)", &origin.x, 0.1f)) Evaluate();
			bool enabled = sequence.qteEnabled; float start = sequence.qteStart, end = sequence.qteEnd;
			bool changed = ImGui::Checkbox((const char*)u8"SPACE判定プレビュー", &enabled);
			changed |= ImGui::DragFloat((const char*)u8"受付開始", &start, 0.05f);
			changed |= ImGui::DragFloat((const char*)u8"受付終了", &end, 0.05f);
			if (changed)
			{
				PushUndo(); sequence.qteEnabled = enabled;
				sequence.qteStart = std::clamp(start, 0.0f, sequence.duration - 0.01f);
				sequence.qteEnd = std::clamp(end, sequence.qteStart + 0.01f, sequence.duration); dirty = true; qteResult = 0;
			}
		}
		ImGui::TextWrapped("%s", (const char*)u8"右ドラッグ＋WASDQE:自由カメラ / K:登録 / Delete:削除 / Ctrl+Z,Y:元に戻す・やり直す");
		if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());
		if (qteResult) ImGui::TextUnformatted(qteResult > 0 ? "QTE: success" : "QTE: failed");
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("MovieInspectorPane", {0,0}, true);
		ImGui::TextUnformatted((const char*)u8"キー詳細 / 現在時刻の値");
		DrawInspector();
		ImGui::EndChild();
	}
	ImGui::End();
	if (!open) closeRequested = true;
	if (sequence.qteEnabled && cursor >= sequence.qteStart && cursor <= sequence.qteEnd)
	{
		const char* text = qteResult == 1 ? (const char*)u8"成功！" : (const char*)u8"SPACE 押せ！";
		const ImVec2 size = ImGui::CalcTextSize(text);
		ImGui::GetForegroundDrawList()->AddText({viewport->Pos.x + (viewport->Size.x - size.x) * 0.5f,
			viewport->Pos.y + viewport->Size.y * 0.2f}, IM_COL32(255,220,80,255), text);
	}
}
