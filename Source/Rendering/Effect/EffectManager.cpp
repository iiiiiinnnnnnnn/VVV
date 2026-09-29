// EffectManager.cpp
#include "Rendering/Core/Graphics.h"
#include "Rendering/Effect/EffectManager.h"
#include "Rendering/Effect/Effect.h"
#include "Application/Time/GameTime.h"
#include "Core/Object/Object.h"

#include <algorithm>

namespace
{
Matrix CreateBillboardTransform(const Matrix& source, const Matrix& view)
{
	Vector3 scale;
	Quaternion rotation;
	Vector3 position;
	Matrix matrix = source;
	if (!matrix.Matrix::Decompose(scale, rotation, position)) return source;

	const Matrix camera = view.Invert();
	const Vector3 right = Vector3(camera._11, camera._12, camera._13) * scale.x;
	const Vector3 up = Vector3(camera._21, camera._22, camera._23) * scale.y;
	const Vector3 front = Vector3(camera._31, camera._32, camera._33) * scale.z;
	return Matrix(
		right.x, right.y, right.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		front.x, front.y, front.z, 0.0f,
		position.x, position.y, position.z, 1.0f);
}

void SetBaseMatrix(const Effekseer::ManagerRef& manager, Effekseer::Handle handle,
	const Matrix& transform)
{
	if (!manager || handle < 0 || !manager->Exists(handle)) return;
	Effekseer::Matrix43 matrix;
	for (int row = 0; row < 4; ++row)
		for (int column = 0; column < 3; ++column)
			matrix.Value[row][column] = transform.m[row][column];
	manager->SetBaseMatrix(handle, matrix);
}

class DetachedEffectObject final : public Object
{
public:
	DetachedEffectObject(std::shared_ptr<Effect> effect, const Matrix& transform)
		: Object("Detached Effect"), effect(std::move(effect)),
		manager(EffectManager::Instance().GetEffekseerManager())
	{
		if (!this->effect) return;
		handle = this->effect->Play(Vector3::Zero);
		if (handle >= 0) this->effect->SetTransform(handle, transform);
	}

	~DetachedEffectObject() override
	{
		if (effect && manager && handle >= 0 && manager->Exists(handle)) effect->Stop(handle);
	}

	Effekseer::Handle GetHandle() const { return handle; }

protected:
	void OnUpdate() override
	{
		if (!manager || handle < 0 || !manager->Exists(handle)) Destroy();
	}

private:
	std::shared_ptr<Effect> effect;
	Effekseer::ManagerRef manager;
	Effekseer::Handle handle = -1;
};
}

// 初期化
void EffectManager::Initialize()
{
	Game::Graphics& graphics = Game::Graphics::Instance();

	// Effekseerレンダラ生成
	effekseerRenderer = EffekseerRendererDX11::Renderer::Create(graphics.GetDevice(),
		graphics.GetDeviceContext(), 2048);

	// Effekseerマネージャー生成
	effekseerManager = Effekseer::Manager::Create(2048);

	// Effekseerレンダラの各種設定（特別なカスタマイズをしない場合は定型的に以下の設定でOK）
	effekseerManager->SetSpriteRenderer(effekseerRenderer->CreateSpriteRenderer());
	effekseerManager->SetRibbonRenderer(effekseerRenderer->CreateRibbonRenderer());
	effekseerManager->SetRingRenderer(effekseerRenderer->CreateRingRenderer());
	effekseerManager->SetTrackRenderer(effekseerRenderer->CreateTrackRenderer());
	effekseerManager->SetModelRenderer(effekseerRenderer->CreateModelRenderer());
	// Effekseer内でのローダーの設定（特別なカスタマイズをしない場合は以下の設定でOK）
	effekseerManager->SetTextureLoader(effekseerRenderer->CreateTextureLoader());
	effekseerManager->SetModelLoader(effekseerRenderer->CreateModelLoader());
	effekseerManager->SetMaterialLoader(effekseerRenderer->CreateMaterialLoader());

	// Effekseerを左手座標系で計算する
	effekseerManager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);
}

// 終了化
void EffectManager::Finalize()
{
	playbackObjects.clear();
	effectPlaybacks.clear();
}

// 更新処理
void EffectManager::Update()
{
	// エフェクト更新処理（引数にはフレームの経過時間を渡す）
	effekseerManager->Update(Game::Time::deltaTime * 60.0f);
	for (const auto& object : playbackObjects)
		if (object && !object->IsPendingDestroy()) object->Update();
	std::erase_if(playbackObjects, [](const std::shared_ptr<Object>& object)
	{
		return !object || object->IsPendingDestroy();
	});
	std::erase_if(effectPlaybacks, [this](const EffectPlayback& playback)
	{
		return !effekseerManager || playback.handle < 0 ||
			!effekseerManager->Exists(playback.handle);
	});
}

Effekseer::Handle EffectManager::PlayDetached(
	const std::shared_ptr<Effect>& effect, const Matrix& transform)
{
	if (!effect || !effect->IsValid()) return -1;
	auto object = std::make_shared<DetachedEffectObject>(effect, transform);
	const Effekseer::Handle handle = object->GetHandle();
	if (handle < 0) return -1;
	object->Awake();
	object->Start();
	playbackObjects.push_back(std::move(object));
	return handle;
}

void EffectManager::RegisterPlayback(
	const Effect* effect, Effekseer::Handle handle, bool billboard)
{
	if (!effect || handle < 0) return;
	effectPlaybacks.push_back({effect, handle, Matrix::Identity, billboard});
}

void EffectManager::SetPlaybackTransform(
	const Effect* effect, Effekseer::Handle handle, const Matrix& transform)
{
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect || playback.handle != handle) continue;
		playback.transform = transform;
		SetBaseMatrix(effekseerManager, handle, transform);
		return;
	}
}

void EffectManager::SetPlaybackPosition(
	const Effect* effect, Effekseer::Handle handle, const Vector3& position)
{
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect || playback.handle != handle) continue;
		playback.transform._41 = position.x;
		playback.transform._42 = position.y;
		playback.transform._43 = position.z;
		SetBaseMatrix(effekseerManager, handle, playback.transform);
		return;
	}
}

void EffectManager::SetPlaybackScale(
	const Effect* effect, Effekseer::Handle handle, const Vector3& scale)
{
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect || playback.handle != handle) continue;
		[[maybe_unused]] Vector3 currentScale;
		Quaternion rotation;
		Vector3 position;
		Matrix matrix = playback.transform;
		if (!matrix.Matrix::Decompose(currentScale, rotation, position)) return;
		playback.transform = Matrix::CreateScale(scale) *
			Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
		SetBaseMatrix(effekseerManager, handle, playback.transform);
		return;
	}
}

void EffectManager::SetBillboard(const Effect* effect, bool value)
{
	if (!effect) return;
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect) continue;
		playback.billboard = value;
		if (!value) SetBaseMatrix(effekseerManager, playback.handle, playback.transform);
	}
}

void EffectManager::StopPlayback(const Effect* effect, Effekseer::Handle handle)
{
	std::erase_if(effectPlaybacks, [effect, handle](const EffectPlayback& playback)
	{
		return playback.effect == effect && playback.handle == handle;
	});
}

void EffectManager::ReleaseEffect(const Effect* effect)
{
	std::erase_if(effectPlaybacks, [effect](const EffectPlayback& playback)
	{
		return playback.effect == effect;
	});
}

// 描画処理
void EffectManager::Render(const Matrix& view, const Matrix& projection)
{
	// ビュー＆プロジェクション行列をEffekseerレンダラに設定
	effekseerRenderer->SetCameraMatrix(*reinterpret_cast<const Effekseer::Matrix44*>(&view));
	effekseerRenderer->SetProjectionMatrix(*reinterpret_cast<const Effekseer::Matrix44*>(&projection));
	for (const EffectPlayback& playback : effectPlaybacks)
		if (playback.billboard)
			SetBaseMatrix(effekseerManager, playback.handle,
				CreateBillboardTransform(playback.transform, view));

	// Effekseer描画開始
	effekseerRenderer->BeginRendering();

	// Effekseer描画実行
	// マネージャー単位で描画するので描画順を制御する場合はマネージャーを複数個作成し、
	// Draw()関数を実行する順序で制御できそう
	effekseerManager->Draw();

	// Effekseer描画終了
	effekseerRenderer->EndRendering();
}
