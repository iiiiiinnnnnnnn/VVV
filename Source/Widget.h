// Widget.h

#pragma once

#include "Common.h"
#include "Texture.h"

class Widget
{
public:
	Widget(const std::string& name = "") : name(name) {}
    virtual ~Widget() = default;
    virtual void Update(float elapsedTime);
    virtual void Render(float elapsedTime);
    virtual void DrawGUI(float elapsedTime);

    bool IsVisible() const { return alpha > 0.0f; }
    bool IsActive() const { return active; }

    void SetVisible(bool visibility) { alpha = visibility ? 1.0f : 0.0f; targetAlpha = alpha; }
    void SetVisible(bool visibility, float speed) { targetAlpha = visibility ? 1.0f : 0.0f; fadeSpeed = speed; }
    void SetActive(bool a) { active = a; }

    void SetPosition(const Vector2 pos) { this->pos = pos; }
	void SetSize(const Vector2 size) { this->size = size; }
	void SetAnchor(const Vector2 anchor) { this->anchor = anchor; }
    
	Vector2 GetPosition() const { return pos; }
	Vector2 GetSize() const { return size; }
	Vector2 GetAnchor() const { return anchor; }
    float GetAlpha() const { return alpha; }

protected:
    virtual void OnUpdate(float elapsedTime) {}
    virtual void OnRender(float elapsedTime) {}
    virtual void OnDrawGUI(float elapsedTime) {}

	Vector2 pos = Vector2::Zero; // 位置
    Vector2 size = {100, 100}; // サイズ

    // アンカー（0.0=左/上, 0.5=中央, 1.0=右/下）
    Vector2 anchor = Vector2::Zero;

    float alpha = 1.0f;
    float targetAlpha = 1.0f;
    float fadeSpeed = 1.0f;

    bool active = true;

    std::string name;
};
