#include <stdexcept>
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>
#include "Core/Foundation/Random.h"
#include <ProfileKit/ProfileKit.h>
#include "Gameplay/Stage/Stage01.h"
#include "Resource/ResourceManager.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Actor/Deer.h"
#include "Gameplay/Stage/Component/Terrain.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Aracore.h"
#include "Gameplay/Player/Player.h"
#include "Gameplay/Actor/Crystal.h"
#include "Gameplay/Actor/RigidRock.h"

Stage01::Stage01(Player* player) : Stage(), player(player)
{
}

void Stage01::OnAwake()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();
	Game::Graphics& graphics = Game::Graphics::Instance();
	graphics.GetSkyBoxRenderer()->SetIntensity(0.0f);

	bgmId = SoundSystem::Instance().Play("BGM/cave_ambient",
		SoundSystem::PlayOptions{.loop = true}, -1);

	if (!LoadVSTG("cave_02"))
		throw std::runtime_error("Failed to load stage");

	stageLoader = GetComponent<StageLoader>();
	if (player && stageLoader && stageLoader->HasPlayerStart())
	{
		// まぁ使わないとは思うけど、複数ある場合はランダムな初期位置から開始する
		player->SetSpawnTransform(stageLoader->GetRandomPlayerStartTransform());
	}

	//	パーティクル準備
	{
		//	パーティクル用画像ロード
		auto sozai = ResourceManager::Instance().LoadTexture("Resources/Image/particle256x256.png");

		//	パーティクルシステム生成
		particleSystem = std::make_unique<ParticleSystem>(
			device, sozai ? sozai->GetShaderResourceView() : nullptr, 4, 4, 1000);
	}
	{
		// フォグ(霧)パーティクル用画像ロード
		auto fogTexture =
			ResourceManager::Instance().LoadTexture("Resources/Image/fog_particle.png");
		fogParticleSystem = std::make_unique<ParticleSystem>(
			device, fogTexture ? fogTexture->GetShaderResourceView() : nullptr, 1, 1, 256);
	}

	//////////////////////////////////////////////////////
	// 登録したスポナー動作させるためのファクトリ関数
	// StageLoaderから読み込んでスポナーであるならここに来る
	//////////////////////////////////////////////////////

	// 動く石
	stageLoader->RegisterSpawnerFactory("RigidRock",
		[](const Transform& transform, const std::string& modelPath, const std::string& spawnTag) {
			return std::make_shared<RigidRock>(transform, modelPath);
		});

	// 鹿
	stageLoader->RegisterSpawnerFactory("Deer",
		[](const Transform& transform, const std::string& modelPath, const std::string& spawnTag) {
			return std::make_shared<Deer>(
				transform, modelPath, spawnTag == "BossField" ? true : false);
		});

	// ボス
	Terrain* terrain = GetComponent<Terrain>();
	stageLoader->RegisterSpawnerFactory(
		"Aracore", [player = player, terrain, loader = stageLoader](const Transform& transform,
					   const std::string& modelPath, const std::string& spawnTag) {
			auto queen = std::make_shared<Aracore>(player, transform, terrain, modelPath);
			queen->SetStageLoader(loader);
			return queen;
		});

	// 結晶
	stageLoader->RegisterSpawnerFactory("Crystal",
		[](const Transform& transform, const std::string& modelPath, const std::string& spawnTag) {
			return std::make_shared<Crystal>(transform, modelPath);
		});
}

Stage01::~Stage01()
{
	chaseAudio.Stop();
	SoundSystem::Instance().Stop(bgmId);
}

void Stage01::Update()
{
	Stage::Update();
	UpdateAmbientBgm();
}

void Stage01::UpdateAmbientBgm()
{
	PROFILE_FUNCTION();
	// player死んだらbgm全部止める
	if (player && player->IsDead())
	{
		chaseAudio.Stop();
		SoundSystem::Instance().Stop(bgmId);
		bgmId = SoundSystem::InvalidVoiceId;
		return;
	}

	bool chasingPlayer = false;
	aracoreThreating = false;
	float largeLife = 0.0f;
	for (const auto* actor : actorManager.GetActors())
	{
		// aracoreで管理すると複数音が鳴るので多少余計な計算が入ってもステージ側でまとめて管理する
		const auto* aracore = dynamic_cast<const Aracore*>(actor);
		if (!aracore || !aracore->IsActive() || aracore->IsPendingDestroy() ||
			aracore->IsDead() || !aracore->IsChasingPlayer()) continue;

		largeLife = std::max(aracore->GetLife(), largeLife);
		chasingPlayer = true;
		if (aracore->IsThreating())
		{
			aracoreThreating = true;
		}
		break;
	}
	chaseAudio.Update(chasingPlayer, largeLife, aracoreThreating);

	if (chaseAudio.IsPlaying())
	{
		SoundSystem::Instance().Stop(bgmId);
		bgmId = SoundSystem::InvalidVoiceId;
		return;
	}
	if (!SoundSystem::Instance().IsPlaying(bgmId))
	{
		bgmId = SoundSystem::Instance().Play(
			"BGM/cave_ambient", SoundSystem::PlayOptions{.loop = true}, -1);
	}
}

void Stage01::OnUpdate()
{
	PROFILE_FUNCTION();
	////////////////////////////////////////////////////////////////////////
	// particle systemを直接計算(ParticleSystemは手動で使うEffectみたいな感じ)
	// しかしジオメトリシェーダーで位置やカメラ情報から4頂点を生成してる
	////////////////////////////////////////////////////////////////////////

	//	降雪
	Vector3 pos = Vector3((rand() % 30 - 15) * 0.1f, rand() % 30 * 0.1f + 1, (rand() % 30 - 15) * 0.1f + 3);
	int max = 2;
	for (int i = 0; i < max; i++)
	{
		//	発生位置
		Vector3 p = { 0,0,0 };
		p.x = pos.x + (rand() % 10001 - 5000) * 0.01f;
		p.y = pos.y;
		p.z = pos.z + (rand() % 10001 - 5000) * 0.01f;
		//	発生方向
		Vector3 v = { 0,0,0 };
		v.y = -(rand() % 10001) * 0.0002f - 0.002f;
		//	力
		Vector3 f = { 0,0,0 };
		f.x = (rand() % 10001) * 0.00001f + 0.1f;
		f.z = (rand() % 10001 - 5000) * 0.00001f;
		//	大きさ
		Vector2 s = { 0.2f,0.2f };

		particleSystem->Set(12, 5, p, v, f, s);
	}

	particleSystem->Update();

	// 煙

	if (!fogParticleSystem) return;
	if (!fogPrewarmed)
	{
		for (int i = 0; i < 96; ++i) SpawnFogParticle();
		fogPrewarmed = true;
	}

	const auto spawnCount = fogSpawnTimer.TickCount(fogSpawnRate * Game::Time::deltaTime);
	for (std::size_t i = 0; i < spawnCount; ++i)
		SpawnFogParticle();
	fogParticleSystem->Update();
}

void Stage01::RenderEffects(const RenderContext& rc)
{
	// 降雪
	rc.graphics->AdditiveBlendState->Bind(rc.deviceContext);
	rc.graphics->DepthReadState->Bind(rc.deviceContext);
	rc.graphics->SolidRasterizerState->Bind(rc.deviceContext);
	particleSystem->Render(rc);

	// 霧
	if (!fogParticleSystem) return;
	rc.graphics->TransparentBlendState->Bind(rc.deviceContext);
	fogParticleSystem->Render(rc);
}

void Stage01::OnDrawGUI()
{
	if (ImGui::TreeNode("Chase BGM"))
	{
		float fadeIn = chaseAudio.GetFadeInSeconds();
		float fadeOut = chaseAudio.GetFadeOutSeconds();
		if (ImGuiToolKit::DragDelayFloat(u8"Fade In", &fadeIn, 0.05f, 0.0f, 10.0f, "%.2f sec"))
			chaseAudio.SetFadeInSeconds(fadeIn);
		if (ImGuiToolKit::DragDelayFloat(u8"Fade Out", &fadeOut, 0.05f, 0.0f, 10.0f, "%.2f sec"))
			chaseAudio.SetFadeOutSeconds(fadeOut);
		ImGui::TreePop();
	}
	if (!ImGui::TreeNode("Fog")) return;
	ImGui::DragFloat("Spawn Rate", &fogSpawnRate, 0.1f, 0.0f, 30.0f);
	ImGuiToolKit::DragSizeFloat(u8"Area Half Width", &fogAreaHalfWidth, 0.5f, 1.0f, 500.0f);
	ImGuiToolKit::DragSizeFloat(u8"Area Half Depth", &fogAreaHalfDepth, 0.5f, 1.0f, 500.0f);
	ImGui::DragFloat("Min Height", &fogMinHeight, 0.1f, -10.0f, fogMaxHeight);
	ImGui::DragFloat("Max Height", &fogMaxHeight, 0.1f, fogMinHeight, 20.0f);
	ImGuiToolKit::DragDelayFloat(u8"Min Lifetime", &fogMinLifetime, 0.1f, 0.1f, fogMaxLifetime);
	ImGuiToolKit::DragDelayFloat(u8"Max Lifetime", &fogMaxLifetime, 0.1f, fogMinLifetime, 30.0f);
	ImGuiToolKit::DragDelayFloat(u8"Fade In", &fogFadeInDuration, 0.1f, 0.0f, fogMinLifetime);
	ImGuiToolKit::DragDelayFloat(u8"Fade Out", &fogFadeOutDuration, 0.1f, 0.0f, fogMinLifetime);
	ImGuiToolKit::DragSizeFloat(u8"Min Size", &fogMinSize, 0.1f, 0.1f, fogMaxSize);
	ImGuiToolKit::DragSizeFloat(u8"Max Size", &fogMaxSize, 0.1f, fogMinSize, 30.0f);
	ImGuiToolKit::DragSpeedFloat(u8"Drift Speed", &fogDriftSpeed, 0.01f, 0.0f, 5.0f);
	ImGuiToolKit::ColorEdit4(u8"Fog Color", &fogColor.x);
	ImGui::TreePop();
}

void Stage01::SpawnFogParticle()
{
	if (!fogParticleSystem) return;

	const Vector3 position = transform.position + Vector3(
		Random::Range(-fogAreaHalfWidth, fogAreaHalfWidth),
		Random::Range(fogMinHeight, fogMaxHeight),
		Random::Range(-fogAreaHalfDepth, fogAreaHalfDepth));
	const float driftAngle = Random::Range(0.0f, DirectX::XM_2PI);
	const Vector3 velocity = {
		cosf(driftAngle) * fogDriftSpeed,
		Random::Range(0.0f, fogDriftSpeed * 0.25f),
		sinf(driftAngle) * fogDriftSpeed};
	const float size = Random::Range(fogMinSize, fogMaxSize);
	const Vector2 particleSize = {
		size * Random::Range(0.8f, 1.2f),
		size * Random::Range(0.55f, 0.9f)};

	fogParticleSystem->Set(
		0,
		Random::Range(fogMinLifetime, fogMaxLifetime),
		position,
		velocity,
		Vector3::Zero,
		particleSize,
		false,
		0.0f,
		fogColor,
		fogFadeInDuration,
		fogFadeOutDuration);
}

// 追跡BGMの再生とフェード

bool Stage01::ChaseAudio::IsPlaying() const
{
	return SoundSystem::Instance().IsPlaying(voiceId);
}

void Stage01::ChaseAudio::Update(bool engaged, float life, bool threatening)
{
	SoundSystem& sound = SoundSystem::Instance();
	const bool lastTime = engaged && life <= 100.0f;
	retryRemaining = std::max(retryRemaining - Game::Time::unscaledDeltaTime, 0.0f);

	if (engaged && retryRemaining <= 0.0f && !sound.IsPlaying(voiceId))
	{
		SoundSystem::PlayOptions options;
		options.volume = 0.0f;
		options.loop = true;
		voiceId = sound.Play("BGM/boss_battle", options, -1);
		if (!voiceId) retryRemaining = 1.0f;
		volume = 0.0f;
	}

	if (!sound.IsPlaying(voiceId))
	{
		voiceId = SoundSystem::InvalidVoiceId;
		volume = 0.0f;
		return;
	}

	const float deltaTime = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	if (engaged)
		volume = std::min(volume + deltaTime / std::max(fadeInSeconds, 0.001f), 1.0f);
	else
		volume = std::max(volume - deltaTime / std::max(fadeOutSeconds, 0.001f), 0.0f);

	if (volume <= 0.0f && !engaged)
	{
		Stop();
		return;
	}

	const float threatVolumeScale = threatening ? 0.5f : 1.0f;
	if (!sound.SetVolume(voiceId, volume * threatVolumeScale))
	{
		Stop();
		retryRemaining = 1.0f;
	}
}

void Stage01::ChaseAudio::Stop()
{
	if (voiceId != SoundSystem::InvalidVoiceId)
		SoundSystem::Instance().Stop(voiceId);
	voiceId = SoundSystem::InvalidVoiceId;
	volume = 0.0f;
}
