// LoadingScene.cpp

#include "Gameplay/Scene/LoadingScene.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Scene/SceneManager.h"

LoadingScene::LoadingScene(SceneMessage message) : Scene(message)
{
	float& screenWidth = Game::Graphics::ScreenWidth;
	float& screenHeight = Game::Graphics::ScreenHeight;

	// イラスト背景
	{
		ShaderParamList params;
		params.push_back({
			"color",
			ColorFromRGBA(0xB2E2FFFF)
		});

		BG_Illust = std::make_shared<SpriteWidget>(
			"Data/Image/loading.png",
			SpriteShaderId::Basic,
			params);

		BG_Illust->rect.position =
		{
			screenWidth * 0.5f,
			screenHeight * 0.5f
		};

		BG_Illust->rect.anchor = { 0.5f, 0.5f };
		BG_Illust->rect.size =
		{
			screenWidth,
			screenHeight
		};
		BG_Illust->rect.size *= 1.2f;

		widgetManager.Register(BG_Illust);
	}

	// 進捗バー背景
	{
		ShaderParamList params;
		params.push_back({
			"color",
			Color(0.0f, 0.0f, 0.0f, 0.55f)
		});

		auto progressBarBack =
			std::make_shared<SpriteWidget>(
				"",
				SpriteShaderId::Basic,
				params);

		progressBarBack->rect.position =
		{
			screenWidth - progressBarWidth - 64.0f,
			screenHeight - 54.0f
		};

		progressBarBack->rect.anchor = { 0.0f, 0.5f };
		progressBarBack->rect.size =
		{
			progressBarWidth,
			6.0f
		};

		widgetManager.Register(progressBarBack);
	}

	// 進捗バー本体
	{
		ShaderParamList params;
		params.push_back({
			"color",
			Color(0.72f, 0.92f, 1.0f, 1.0f)
		});

		progressBarFill =
			std::make_shared<SpriteWidget>(
				"",
				SpriteShaderId::Basic,
				params);

		progressBarFill->rect.position =
		{
			screenWidth - progressBarWidth - 64.0f,
			screenHeight - 54.0f
		};

		progressBarFill->rect.anchor = { 0.0f, 0.5f };
		progressBarFill->rect.size = { 0.0f, 2.0f };

		widgetManager.Register(progressBarFill);
	}
}

void LoadingScene::OnUpdate()
{
	// マウス移動に合わせて背景を動かす。
	{
		if (!cursorInit)
		{
			POINT tmp;
			GetCursorPos(&tmp);
			oldCursorPos.x = static_cast<float>(tmp.x);
			oldCursorPos.y = static_cast<float>(tmp.y);
			cursorInit = true;
		}

		Vector2 currentCursorPos{};
		{
			POINT tmp{};
			GetCursorPos(&tmp);
			currentCursorPos.x = static_cast<float>(tmp.x);
			currentCursorPos.y = static_cast<float>(tmp.y);
		}

		cursorMoveVec += currentCursorPos - oldCursorPos;

		const float limitX =
			Game::Graphics::ScreenWidth * 0.07f;
		const float limitY =
			Game::Graphics::ScreenHeight * 0.07f;

		cursorMoveVec.x = std::clamp(
			cursorMoveVec.x,
			-limitX,
			limitX);
		cursorMoveVec.y = std::clamp(
			cursorMoveVec.y,
			-limitY,
			limitY);

		const Vector2 center =
		{
			Game::Graphics::ScreenWidth * 0.5f,
			Game::Graphics::ScreenHeight * 0.5f
		};

		const Vector2 target = center + cursorMoveVec;

		bgVelocity = Vector2::Lerp(
			bgVelocity,
			target - BG_Illust->rect.position,
			0.1f * Game::Time::deltaTime);

		bgVelocity *= 0.85f;
		BG_Illust->rect.position += bgVelocity;

		oldCursorPos = currentCursorPos;
	}

	if (progressBarFill)
	{
		const float progress =
			SceneManager::Instance().GetLoadProgress();

		progressBarFill->rect.size.x =
			progressBarWidth * progress;
	}
}

void LoadingScene::OnDrawGUI()
{
	const float progress =
		SceneManager::Instance().GetLoadProgress();

	ImGui::Text(
		"Load Progress: %d%%",
		static_cast<int>(progress * 100.0f + 0.5f));

	const std::string error =
		SceneManager::Instance().GetLastLoadError();

	if (!error.empty())
	{
		ImGui::TextWrapped("Load Error: %s", error.c_str());
	}
}
