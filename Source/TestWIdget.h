#pragma once

#include "Widget.h"

class TestWidget : public Widget
{
public:
    TestWidget();
    
    void OnUpdate(float elapsedTime) override;
    void OnRender(float elapsedTime) override;

	std::shared_ptr<Texture> texture = nullptr;
};