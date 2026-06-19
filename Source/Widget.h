// Widget.h

#pragma once

#include "Object.h"
#include "RectTransform.h"
#include "Components.h"
#include "Texture.h"
class WidgetManager;

class Widget : public Object
{
public:
	Widget(const std::string& name = "", bool isActive = true)
        : Object(name, isActive) {}
    virtual ~Widget() = default;

    RectTransform rect;

	// オーバーライドする必要があるやつだけ(rect transform割り込み)
    void Update() override;
    void DrawGUI() override;

protected:
	friend class WidgetManager;
    WidgetManager* widgetManager = nullptr;
};
