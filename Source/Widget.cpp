// Widget.cpp

#include "Widget.h"

void Widget::Update(float elapsedTime)
{
	// アルファ値を目標値に近づける
    if (alpha != targetAlpha)
    {
        float diff = targetAlpha - alpha;
        float delta = fadeSpeed * elapsedTime;
        if (fabsf(diff) <= delta)
            alpha = targetAlpha;
        else
            alpha += (diff > 0.0f ? 1.0f : -1.0f) * delta;
    }

    OnUpdate(elapsedTime);
}

void Widget::Render(float elapsedTime)
{
    OnRender(elapsedTime);
}

void Widget::DrawGUI(float elapsedTime)
{
    ImGui::PushID(this);
    if (ImGui::CollapsingHeader(name.empty() ? "Unnamed Widget" : name.c_str()))
    {
        ImGui::TreePush(this);
		OnDrawGUI(elapsedTime);
        ImGui::TreePop();
    }
    ImGui::PopID();

    ImGui::Separator();
}