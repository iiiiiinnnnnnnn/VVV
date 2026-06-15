// LoadingScene.cpp

#include "LoadingScene.h"
#include "GameTime.h"
#include "SceneManager.h"
#include "TestPlayScene.h"

LoadingScene::LoadingScene(SceneMessage message)
    : Scene("LoadingScene", message)
{
    ID3D11Device* device = Game::Graphics::Instance().GetDevice();
    float& screenWidth  = Game::Graphics::ScreenWidth;
    float& screenHeight = Game::Graphics::ScreenHeight;

    // ê^Ç¡çïîwåi
    {
        /*ShaderParamList params;
        params.push_back({"color", Color(0.0f, 0.0f, 0.0f, 1.0f)});

        auto BG_Black = std::make_shared<SpriteWidget>("",
            SpriteShaderId::Basic, params);
        BG_Black->rect.position = {0, 0};
		BG_Black->rect.size = {screenWidth, screenHeight};
        widgets.Register(BG_Black);*/
    }

    // ÉCÉâÉXÉgîwåi
    {
        ShaderParamList params;
        params.push_back({"color", Color(1.0f, 1.0f, 1.0f, 0.7f)});

        BG_Illust = std::make_shared<SpriteWidget>("Data/Image/loading.png",
            SpriteShaderId::Basic, params);
        BG_Illust->rect.position = {screenWidth * 0.5f, screenHeight * 0.5f};
        BG_Illust->rect.anchor = {0.5f, 0.5f};
        BG_Illust->rect.size = {Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight};
        BG_Illust->rect.size *= 1.2f;
		widgets.Register(BG_Illust);
    }

    // Vignette
    {
        ShaderParamList params;
        params.push_back({"color", Color(0.0f, 0.0f, 0.0f, 1.0f)});
        auto vignette = std::make_shared<SpriteWidget>("",
            SpriteShaderId::Vignette, params);
        vignette->rect.position = {screenWidth * 0.5f, screenHeight * 0.5f};
        vignette->rect.anchor = {0.5f, 0.5f};
        float size = 1.5f;
        vignette->rect.size = {screenWidth * size, screenHeight * size};
		widgets.Register(vignette);
    }

    //SceneManager::Instance().LoadSceneAsync<TestPlayScene>();
}

void LoadingScene::OnUpdate()
{
    // É}ÉEÉXÇ≈îwåiìÆÇ©Ç∑
    {
        if (!cursorInit)
        {
            POINT tmp;
            GetCursorPos(&tmp);
            oldCursorPos.x = static_cast<float>(tmp.x);
            oldCursorPos.y = static_cast<float>(tmp.y);
            cursorInit = true;
        }

        Vector2 norCursorPos{};
        {
            POINT tmp{};
            GetCursorPos(&tmp);
            norCursorPos.x = static_cast<float>(tmp.x);
            norCursorPos.y = static_cast<float>(tmp.y);
        }

        cursorMoveVec += norCursorPos - oldCursorPos;

        float limitX = Game::Graphics::ScreenWidth * 0.07f;
        float limitY = Game::Graphics::ScreenHeight * 0.07f;
        cursorMoveVec.x = std::clamp(cursorMoveVec.x, -limitX, limitX);
        cursorMoveVec.y = std::clamp(cursorMoveVec.y, -limitY, limitY);
        Vector2 center = {Game::Graphics::ScreenWidth * 0.5f, Game::Graphics::ScreenHeight * 0.5f};
        Vector2 target = center + cursorMoveVec;
        bgVelocity = Vector2::Lerp(bgVelocity, target - BG_Illust->rect.position, 0.1f * Game::Time::deltaTime);
        bgVelocity *= 0.85f; // å∏êä
        BG_Illust->rect.position += bgVelocity;

        oldCursorPos = norCursorPos;
    }
}

void LoadingScene::OnDrawGUI()
{
}
