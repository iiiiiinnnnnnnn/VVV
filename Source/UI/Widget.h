// Widget.h

#pragma once

#include "Core/Object/Object.h"
#include "Core/Object/RectTransform.h"
#include "Resource/Texture.h"
#include <string>
#include <imgui.h>

class WidgetManager;

class Widget : public Object
{
public:
	Widget(const std::string& name = "", const std::string& tag = "", bool isActive = true)
        : Object(name, tag, isActive) {}
    virtual ~Widget() = default;

    RectTransform rect;

	// オーバーライドする必要があるやつだけ(rect transform割り込み)
    void Update() override;
    void DrawGUI() override;
	RectTransform* GetRectTransform() override { return &rect; }
	const RectTransform* GetRectTransform() const override { return &rect; }

	void SetAffectedByPostProcess(bool affected) { affectedByPostProcess = affected; }
	bool GetAffectedByPostProcess() const { return affectedByPostProcess; }

protected:
	friend class WidgetManager;
    WidgetManager* widgetManager = nullptr;
	bool affectedByPostProcess = false;
};
