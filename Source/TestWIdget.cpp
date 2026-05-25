#include "TestWIdget.h"

TestWidget::TestWidget(Vector2 pos) : Widget("TestWidget")
{
    rect.position = pos;
    AddComponent<SpriteRenderComponent>(std::make_shared<Texture>("Data/Image/Test.png"), SpriteShaderId::GaussianFilter, &gaussianParams);
}

void TestWidget::OnUpdate(float elapsedTime)
{
    
}

void TestWidget::OnRender(const RenderContext& rc, float elapsedTime)
{

}

void TestWidget::OnDrawGUI(float elapsedTime)
{
    ImGui::SliderInt("Kernel Size", &gaussianParams.kernel_size, 1, GaussianFilterShader::KernelMax);
    ImGui::SliderFloat("Sigma", &gaussianParams.sigma, 0.1f, 100.0f);
}
