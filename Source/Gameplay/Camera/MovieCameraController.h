#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "Gameplay/Camera/CameraController.h"
#include "Audio/SoundSystem.h"
#include "Core/Foundation/Easing.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Shader/GamePostProcess.h"

class Stage;
class Effect;
class Actor;
class FreeCameraController;
class VMDLModelComponent;

class MovieCameraController : public CameraController
{
public:
	enum class QteResult { None, Success, Failed };
	struct CameraKey
	{
		float time = 0.0f;
		Vector3 eye = { 8, 5, -12 };
		Vector3 focus = { 0, 2, -3 };
		float fov = 55.0f;
		int easing = static_cast<int>(Easing::Type::InOutSine);
		bool cut = false;
	};
	struct ActorKey
	{
		float time = 0.0f;
		Vector3 position = Vector3::Zero;
		float yaw = 0.0f;
		int easing = static_cast<int>(Easing::Type::InOutSine);
	};
	struct MotionKey
	{
		float time = 0.0f;
		std::string animation;
		float offset = 0.0f;
		float speed = 1.0f;
		bool loop = false;
	};
	struct EffectKey
	{
		float time = 0.0f;
		json settings;
		int easing = static_cast<int>(Easing::Type::InOutSine);
	};
	struct ParticleKey
	{
		float time = 0.0f;
		std::string path;
		float duration = 1.0f;
		Vector3 position = {0, 1, 0};
		Vector3 rotation = Vector3::Zero;
		Vector3 scale = Vector3::One;
		float speed = 1.0f;
		int target = 0; // 0: ムービー基準位置、1: プレイヤー、2: ボス
		bool follow = true;
		bool billboard = false;
		int seed = 0;
	};
	struct TimeScaleKey
	{
		float time = 0.0f;
		float scale = 1.0f;
		int easing = static_cast<int>(Easing::Type::InOutSine);
		bool interpolate = false;
	};
	struct SoundKey
	{
		float time = 0.0f;
		std::string path;
		float duration = 2.0f;
		float volume = 1.0f;
		float pitch = 1.0f;
		float pan = 0.0f;
		bool loop = false;
	};
	struct Sequence
	{
		float duration = 5.0f;
		float qteStart = 2.5f;
		float qteEnd = 3.5f;
		bool qteEnabled = true;
		std::vector<CameraKey> cameras;
		std::vector<ActorKey> player;
		std::vector<ActorKey> boss;
		std::vector<MotionKey> playerMotions;
		std::vector<MotionKey> bossMotions;
		std::vector<EffectKey> effects;
		std::vector<ParticleKey> particles;
		std::vector<TimeScaleKey> timeScales;
		std::vector<SoundKey> sounds;
	};

	MovieCameraController(Object* owner);
	~MovieCameraController() override;
	const char* GetDebugName() const override { return ICON_FA_VIDEO "MovieCameraController"; }
	void SyncCameraToController(const Camera& camera) override;
	void SyncControllerToCamera(Camera& camera) override;
	bool Begin(Stage& stage, Actor& player, Actor* boss, Game::PostProcess& postProcess);
	bool BeginPlayback(Stage& stage, Actor& player, Actor& boss, Game::PostProcess& postProcess);
	void End(bool keepActorPositions = false);
	bool IsPlayback() const { return runtimePlayback; }
	int GetPlaybackResult() const { return playbackResult; }
	void Tick(bool qtePressed = false);
	QteResult ConsumeQteResult();
	float GetQteRemaining() const;
	float GetQteDuration() const;
	void DrawEditor();
	void ApplyPostEffects(Game::PostProcess& postProcess) const;
	bool IsEditing() const { return editing; }

private:
	struct ActorState
	{
		Actor* actor = nullptr;
		VMDLModelComponent* renderer = nullptr;
		Transform transform;
		bool active = true;
		std::vector<VMDLModel::NodePose> pose;
	};
	struct VisibilityState { Actor* actor; bool active; };
	struct CameraState { Camera* camera; int priority; };
	struct ControllerState { CameraController* controller; bool active; };

	struct ParticlePlayback
	{
		std::shared_ptr<Effect> effect;
		int handle = -1;
		float frame = -1.0f;
		Matrix transform = Matrix::Identity;
		bool started = false;
	};

	struct SoundPlayback
	{
		SoundSystem::VoiceId voice = SoundSystem::InvalidVoiceId;
		bool started = false;
		bool paused = false;
	};
	void ResolveQte(QteResult result);
	void StopSounds();
	void UpdateSounds(float previousTime);
	void StopParticles();
	void EvaluateParticles();
	Matrix GetParticleTransform(const ParticleKey& key) const;
	float GetMovieTimeScale(float time) const;
	void AdvanceTime(float deltaTime);
	void PlaceAtStageCenter();
	void SeedSequence();
	void Seek(float time);
	void Evaluate();
	void EvaluateActor(ActorState& actor, const std::vector<ActorKey>& keys,
		const std::vector<MotionKey>& motions);
	void DrawTimeline();
	void DrawInspector();
	void RegisterKey();
	void DeleteKey();
	void SelectKey(int track, int index);
	void Scrub(float time);
	void DrawKeyList();
	void PushUndo();
	void Undo();
	void Redo();
	bool Save(const std::filesystem::path& path);
	bool Load(const std::filesystem::path& path);

	Sequence sequence;
	SoundKey soundDraft;
	std::vector<SoundPlayback> soundPlaybacks;
	SoundSystem::VoiceId previewSound = SoundSystem::InvalidVoiceId;
	std::vector<std::string> soundPaths;
	std::string soundSearch;
	bool soundPathsLoaded = false;
	ParticleKey particleDraft;
	std::vector<ParticlePlayback> particlePlaybacks;
	std::vector<std::string> particlePaths;
	std::string particleSearch;
	bool particlePathsLoaded = false;
	float particleCursor = -1.0f;
	std::vector<Sequence> undoStack, redoStack;
	Game::PostProcess* moviePostProcess = nullptr;
	json savedPostSettings;
	json currentPostSettings;
	bool dragUndoSaved = false;
	bool editUndoSaved = false;
	Stage* stage = nullptr;
	Camera* camera = nullptr;
	FreeCameraController* freeCamera = nullptr;
	std::shared_ptr<Actor> temporaryBoss;
	ActorState playerState;
	ActorState bossState;
	std::vector<VisibilityState> visibility;
	std::vector<CameraState> cameraStates;
	std::vector<ControllerState> controllerStates;
	Vector3 origin = Vector3::Zero;
	Vector3 savedEye, savedFocus, savedUp;
	float savedFov = 55.0f;
	float fov = 55.0f;
	float cursor = 0.0f;
	bool runtimePlayback = false;
	int playbackResult = 0;
	bool editing = false;
	bool playing = false;
	bool loop = true;
	bool freeView = false;
	bool wasPaused = false;
	float savedTimeScale = 1.0f;
	bool dirty = false;
	bool closeRequested = false;
	int selectedTrack = 0;
	int selectedKey = -1;
	int qteResult = 0;
	QteResult pendingQteResult = QteResult::None;
	std::filesystem::path documentPath;
};
