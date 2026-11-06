#pragma once

#include "Core/Object/Object.h"
#include "Core/Object/RectTransform.h"
#include "Resource/Texture.h"
#include <string>
#include <imgui.h>

class WidgetManager;
class WidgetGroup;

class Widget : public Object
{
public:
	Widget(const std::string& name = "", const std::string& tag = "", bool isActive = true)
        : Object(name, tag, isActive) {}
    virtual ~Widget() = default;

    RectTransform rect;

	// テクスチャの有無に依存しない、回転を考慮した矩形の当たり判定。
	bool ContainsPoint(const Vector2& point) const;

	// オーバーライドする必要があるやつだけ(rect transform割り込み)
	void Render(const RenderContext& rc) override;
	Matrix GetWorldTransform() const;
	float GetWorldAlpha() const;
	Vector2 GetWorldCenter() const;
	bool ScreenToLocal(const Vector2& screen, Vector2& local) const;
	bool SetScreenPosition(const Vector2& screen);
	WidgetGroup* GetParent() const { return parent; }

    void Update() override;
    void DrawGUI() override;
    void DrawInspector();
	RectTransform* GetRectTransform() override { return &rect; }
	const RectTransform* GetRectTransform() const override { return &rect; }

	void SetAffectedByPostProcess(bool affected) { affectedByPostProcess = affected; }
	bool GetAffectedByPostProcess() const { return affectedByPostProcess; }
	virtual void DrawShadow(const RenderContext& rc, const Vector2& offset,
		const Color& color) {}

protected:
	friend class WidgetManager;
	friend class WidgetGroup;
    WidgetManager* widgetManager = nullptr;
	bool affectedByPostProcess = false;

private:
	WidgetGroup* parent = nullptr;

};
