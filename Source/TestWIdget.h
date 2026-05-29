// TestWidget.h

#pragma once

#include "Widget.h"

class TestWidget : public Widget
{
public:
    TestWidget(Vector2 pos);
    
    void OnUpdate(float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;

private:
    ShaderParamList shaderParam;
};