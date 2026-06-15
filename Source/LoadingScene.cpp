// LoadingScene.cpp

#include "LoadingScene.h"
#include "GameTime.h"
#include "SceneManager.h"
#include "TestPlayScene.h"

LoadingScene::LoadingScene(SceneMessage message)
    : Scene("LoadingScene", message)
{
    ID3D11Device* device = Game::Graphics::Instance().GetDevice();
    float screenWidth  = Game::Graphics::ScreenWidth;
    float screenHeight = Game::Graphics::ScreenHeight;

    // widget
    {
        ShaderParamList params;
        params.push_back({"color", Color(1.0f, 1.0f, 1.0f, 0.5f)});

        BG = std::make_shared<SpriteWidget>("Data/Image/loading.png", SpriteShaderId::Basic, params);
		BG->rect.position = {screenWidth * 0.5f, screenHeight * 0.5f};
		BG->rect.anchor = {0.5f, 0.5f};
        BG->rect.size = {Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight};
		BG->rect.size *= 1.2f;
		widgets.Register(BG);
    }

    SceneManager::Instance().LoadSceneAsync<TestPlayScene>();
}

void LoadingScene::OnUpdate()
{
    if (!cursorInit)
    {
        POINT tmp;
        GetCursorPos(&tmp);
		oldCursorPos.x = tmp.x;
		oldCursorPos.y = tmp.y;
        cursorInit = true;
    }

    Vector2 norCursorPos{};
    {
        POINT tmp{};
        GetCursorPos(&tmp);
        norCursorPos.x = tmp.x;
        norCursorPos.y = tmp.y;
    }

	cursorMoveVec += norCursorPos - oldCursorPos;

    float limitX = Game::Graphics::ScreenWidth  * 0.07f;
    float limitY = Game::Graphics::ScreenHeight * 0.07f;
    cursorMoveVec.x = std::clamp(cursorMoveVec.x, -limitX, limitX);
    cursorMoveVec.y = std::clamp(cursorMoveVec.y, -limitY, limitY);
    Vector2 center = { Game::Graphics::ScreenWidth * 0.5f, Game::Graphics::ScreenHeight * 0.5f };
    Vector2 target = center + cursorMoveVec;
    bgVelocity = Vector2::Lerp(bgVelocity, target - BG->rect.position, 0.1f * Game::Time::deltaTime);
    bgVelocity *= 0.85f; // Œ¸Š
    BG->rect.position += bgVelocity;

	oldCursorPos = norCursorPos;
}

void LoadingScene::OnDrawGUI()
{
}
