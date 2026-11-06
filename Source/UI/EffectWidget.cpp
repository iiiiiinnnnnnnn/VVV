#include "EffectWidget.h"
#include "Resource/ResourceManager.h"
#include "Rendering/Effect/EffectManager.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Core/Graphics.h"
#include "Application/Time/GameTime.h"

EffectWidget::EffectWidget(const std::string& path, float scale)
	: effectPath(path),
	  effectScale(scale)
{
	// 初期状態ではrect.positionをエフェクトの再生原点にする。
	rect.anchor = {0.5f, 0.5f};
}

EffectWidget::~EffectWidget()
{
	Stop();
}

void EffectWidget::Stop()
{
	if (EffectManager::Instance().GetEffekseerManager())
		for (const auto& playback : playbacks) playback.effect->Stop(playback.handle);
	playbacks.clear();
}

void EffectWidget::SetUseUnscaledTime(bool value)
{
	if (useUnscaledTime == value) return;
	useUnscaledTime = value;
	if (auto manager = EffectManager::Instance().GetEffekseerManager())
		for (const auto& playback : playbacks) manager->SetPaused(playback.handle, value);
}

void EffectWidget::OnAwake()
{
	effectPath = ResourceManager::Instance().ResolveResourcePath(effectPath, ResourceManager::ResourceType::Effect);
}

void EffectWidget::Play()
{
	Awake();
	if (effectPath.empty()) return;
	if (!effect) effect = EffectManager::Instance().LoadEffect(effectPath);
	if (!effect) return;
	const auto handle = EffectManager::Instance().Play2D(
		effect, GetEffectPosition(), effectScale, GetAffectedByPostProcess());
	if (handle >= 0)
	{
		playbacks.push_back({effect, handle});
		effect->SetTransform(handle, GetEffectTransform());
		effect->SetAlpha(handle, GetWorldAlpha() * screenAlpha);
		if (useUnscaledTime)
		{
			auto manager = EffectManager::Instance().GetEffekseerManager();
			manager->BeginUpdate();
			manager->UpdateHandle(handle, 0.0f);
			manager->EndUpdate();
			manager->SetPaused(handle, true);
		}
	}
}

Vector2 EffectWidget::GetEffectPosition() const
{
	return Vector2::Transform(rect.size * 0.5f, screenTransform ? *screenTransform : GetWorldTransform());
}

Matrix EffectWidget::GetEffectTransform() const
{
	// エフェクトの上向き座標をUI座標へ変換し、親子の変換後に画面中央基準へ戻す。
	return Matrix::CreateScale(effectScale, -effectScale, effectScale)
		* Matrix::CreateTranslation(rect.size.x * 0.5f, rect.size.y * 0.5f, 0.0f)
		* (screenTransform ? *screenTransform : GetWorldTransform())
		* Matrix::CreateScale(1.0f, -1.0f, 1.0f)
		* Matrix::CreateTranslation(-Game::Graphics::ScreenWidth * 0.5f, Game::Graphics::ScreenHeight * 0.5f, 0.0f);
}

void EffectWidget::Update()
{
	auto manager = EffectManager::Instance().GetEffekseerManager();
	if (!manager) return;
	std::erase_if(playbacks, [&](const Playback& playback) { return !manager->Exists(playback.handle); });
	const Matrix transform = GetEffectTransform();
	for (const Playback& playback : playbacks)
	{
		playback.effect->SetTransform(playback.handle, transform);
		playback.effect->SetAlpha(playback.handle, GetWorldAlpha() * screenAlpha);
	}
	if (useUnscaledTime && !playbacks.empty())
	{
		manager->BeginUpdate();
		for (const auto& playback : playbacks)
		{
			manager->SetPaused(playback.handle, false);
			manager->UpdateHandle(playback.handle, std::max(Game::Time::unscaledDeltaTime, 0.0f) * 60.0f);
			manager->SetPaused(playback.handle, true);
		}
		manager->EndUpdate();
	}
	Widget::Update();
}
