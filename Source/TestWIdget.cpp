#include "TestWIdget.h"
#include <Graphics.h>

TestWidget::TestWidget()
{
	texture = std::make_shared<Texture>("Data/Image/Test.png");
}

void TestWidget::OnUpdate(float elapsedTime)
{
}

void TestWidget::OnRender(float elapsedTime)
{
    Graphics::Instance().GetSpriteRenderer()->Draw(
        SpriteShaderID::Basic, texture,
        posX, posY, 0.0f,           // dx, dy, dz
        width * 2, height * 2,               // dw, dh
        0.0f, 0.0f,                  // sx, sy
        (float)texture->GetWidth(),  // sw
        (float)texture->GetHeight(), // sh
        0.0f,                        // angle
        1.0f, 1.0f, 1.0f, alpha);   // r, g, b, a
}
