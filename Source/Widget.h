// Widget.h

#pragma once

#include "Object.h"
#include "Texture.h"
#include "RectTransform.h"

class Widget : public Object
{
public:
	Widget(const std::string& name = "", const std::string& tag = "", bool isActive = true)
        : Object(name, tag, isActive) {}
    virtual ~Widget() = default;

    RectTransform rect;

	// オーバーライドする必要があるやつだけ(rect transform割り込み)
    void Update(float elapsedTime) override;
    void DrawGUI(float elapsedTime) override;
};
