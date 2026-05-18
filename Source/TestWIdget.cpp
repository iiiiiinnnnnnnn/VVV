#include "TestWIdget.h"
#include <Graphics.h>

TestWidget::TestWidget(Vector2 pos) : Widget("TestWidget")
{
    this->pos = pos;
	texture = std::make_shared<Texture>("Data/Image/Test.png");
}

void TestWidget::OnUpdate(float elapsedTime)
{
    
}

void TestWidget::OnRender(float elapsedTime)
{
    auto params = std::make_shared<GaussianFilterShader::GaussianFilterData>(gaussianParams);

    Graphics::Instance().GetSpriteRenderer()->Draw(
        SpriteShaderID::GaussianFilter, texture,
        pos.x, pos.y, 0.0f,
        size.x, size.y,
        0.0f, 0.0f,
        (float)texture->GetWidth(),
        (float)texture->GetHeight(),
        0.0f,
        1.0f, 1.0f, 1.0f, alpha,
        params);
}

void TestWidget::OnDrawGUI(float elapsedTime)
{
    ImGui::SliderInt("Kernel Size", &gaussianParams.kernel_size, 1, GaussianFilterShader::KernelMax);
    ImGui::SliderFloat("Sigma", &gaussianParams.sigma, 0.1f, 100.0f);
}
