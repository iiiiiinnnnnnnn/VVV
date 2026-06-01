// TestWidget.h

#pragma once

#include "Widget.h"

class TestWidget : public Widget
{
public:
    TestWidget(Vector2 pos);
    
    void OnUpdate() override;
	void OnDrawGUI() override;

private:
    ShaderParamList shaderParam;
};