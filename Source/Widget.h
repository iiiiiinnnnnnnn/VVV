// Widget.h

#pragma once

#include "Common.h"
#include "Texture.h"

class Widget
{
public:
    virtual ~Widget() = default;
    virtual void Update(float elapsedTime);
    virtual void Render(float elapsedTime);
    virtual void DrawGUI(float elapsedTime);

    bool IsVisible() const { return alpha > 0.0f; }
    bool IsActive() const { return active; }

    void SetVisible(bool visibility) { alpha = visibility ? 1.0f : 0.0f; targetAlpha = alpha; }
    void SetVisible(bool visibility, float speed) { targetAlpha = visibility ? 1.0f : 0.0f; fadeSpeed = speed; }
    void SetActive(bool a) { active = a; }

    void SetPosition(float x, float y) { posX = x; posY = y; }
    void SetSize(float w, float h) { width = w; height = h; }
    void SetAnchor(float x, float y) { anchorX = x; anchorY = y; }

    float GetPosX() const { return posX; }
    float GetPosY() const { return posY; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }
    float GetAlpha() const { return alpha; }

protected:
    virtual void OnUpdate(float elapsedTime) {}
    virtual void OnRender(float elapsedTime) {}
    virtual void OnDrawGUI(float elapsedTime) {}

    float posX = 0.0f;
    float posY = 0.0f;
    float width = 100.0f;
    float height = 100.0f;

    // アンカー（0.0=左/上, 0.5=中央, 1.0=右/下）
    float anchorX = 0.0f;
    float anchorY = 0.0f;

    float alpha = 1.0f;
    float targetAlpha = 1.0f;
    float fadeSpeed = 1.0f;

    bool active = true;
};
