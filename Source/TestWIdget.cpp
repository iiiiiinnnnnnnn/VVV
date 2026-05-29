// TestWidget.cpp

#include "TestWIdget.h"

TestWidget::TestWidget(Vector2 pos) : Widget("TestWidget")
{
    rect.position = pos;

    shaderParam = {
        {"kernel_size", 20},
        {"sigma", 20.0f}
	};
    AddComponent<SpriteRenderComponent>(
        std::make_shared<Texture>("Data/Image/Test.png"),
        SpriteShaderId::GaussianFilter, shaderParam);
}

void TestWidget::OnUpdate(float elapsedTime)
{
    
}

void TestWidget::OnDrawGUI(float elapsedTime)
{

}
