#include "Gameplay/Scene/LoadingScene.h"

#include <algorithm>
#include <cmath>

#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/SpriteWidget.h"
#include "UI/Widget.h"

namespace
{
constexpr const char* LoadingImagePath = "Resources/UI/loading.png";
}

LoadingScene::LoadingScene()
{
	Mouse& mouse = Game::Input::Instance().GetMouse();
	mouse.SetCursorLock(false);
	mouse.ForceCursorVisible(false);

	background = std::make_shared<SpriteWidget>(
		LoadingImagePath, SpriteShaderId::Basic, Color(1, 1, 1, 1));
	background->SetName("Loading Background");
	background->SetAffectedByPostProcess(false);
	widgetManager.Register(background);

	// 背景とは別レイヤーにして、画像の移動に影響されない画面端の暗さを作る。
	vignetteOverlay = std::make_shared<SpriteWidget>(
		LoadingImagePath,
		SpriteShaderId::VignetteOverlay,
		Color(0.0f, 0.01f, 0.03f, 0.82f),
		SpriteRenderParams{SpriteVignetteParams{0.98f, 0.86f}});
	vignetteOverlay->SetName("Loading Vignette Overlay");
	vignetteOverlay->SetAffectedByPostProcess(false);
	widgetManager.Register(vignetteOverlay);

	const auto makeFill = [this](const char* name, const Color& color)
	{
		auto widget = std::make_shared<Widget>(name);
		widget->AddComponent<SpriteRenderComponent>(
			std::make_shared<Texture>(Color(1, 1, 1, 1)),
			SpriteShaderId::Basic, color);
		widget->SetAffectedByPostProcess(false);
		widgetManager.Register(widget);
		return widget;
	};
	progressTrack = makeFill("Loading Progress Track", Color(0.02f, 0.05f, 0.09f, 0.72f));
	progressFill = makeFill("Loading Progress Fill", Color(0.52f, 0.76f, 1.0f, 0.95f));
}

void LoadingScene::OnUpdate()
{
	// ImGuiのWin32バックエンドがフレーム開始時に再表示しても、
	// ロード画面ではカーソルを常に隠す。
	Game::Input::Instance().GetMouse().SetCursorVisible(false);

	elapsedTime += std::max(Game::Time::unscaledDeltaTime, 0.0f);
	UpdateLayout();

	const float progress = std::clamp(
		SceneManager::Instance().GetLoadProgress(), 0.0f, 1.0f);
	if (progressFill)
	{
		if (auto* sprite = progressFill->GetComponent<SpriteRenderComponent>())
		{
			sprite->SetHorizontalFill(progress);
			const float pulse = 0.86f + std::sin(elapsedTime * 4.0f) * 0.12f;
			sprite->SetColor(Color(0.52f, 0.76f, 1.0f, pulse));
		}
	}
}

void LoadingScene::UpdateLayout()
{
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) return;

	if (background)
	{
		float imageAspect = 16.0f / 9.0f;
		if (auto* sprite = background->GetComponent<SpriteRenderComponent>())
		{
			if (Texture* texture = sprite->GetTexture();
				texture && texture->GetWidth() > 0 && texture->GetHeight() > 0)
			{
				imageAspect = static_cast<float>(texture->GetWidth()) / texture->GetHeight();
			}
		}

		// Cover表示を少し拡大し、その余白の範囲だけをゆっくり漂わせる。
		float width = screenWidth;
		float height = width / imageAspect;
		if (height < screenHeight)
		{
			height = screenHeight;
			width = height * imageAspect;
		}
		const float zoom = 1.09f + 0.025f * (0.5f + 0.5f * std::sin(elapsedTime * 0.35f));
		width *= zoom;
		height *= zoom;
		const float driftX = (width - screenWidth) * 0.38f * std::sin(elapsedTime * 0.12f);
		const float driftY = (height - screenHeight) * 0.34f * std::sin(elapsedTime * 0.09f + 1.2f);

		background->rect.position = {
			screenWidth * 0.5f + driftX,
			screenHeight * 0.5f + driftY};
		background->rect.anchor = {0.5f, 0.5f};
		background->rect.size = {width, height};
	}

	if (vignetteOverlay)
	{
		vignetteOverlay->rect.position = Vector2::Zero;
		vignetteOverlay->rect.anchor = Vector2::Zero;
		vignetteOverlay->rect.size = {screenWidth, screenHeight};
	}

	const float progressWidth = std::min(screenWidth * 0.42f, 720.0f);
	const Vector2 progressPosition(screenWidth * 0.5f, screenHeight - 58.0f);
	if (progressTrack)
	{
		progressTrack->rect.position = progressPosition;
		progressTrack->rect.anchor = {0.5f, 0.5f};
		progressTrack->rect.size = {progressWidth, 6.0f};
	}
	if (progressFill)
	{
		progressFill->rect.position = progressPosition;
		progressFill->rect.anchor = {0.5f, 0.5f};
		progressFill->rect.size = {progressWidth, 3.0f};
	}
}
