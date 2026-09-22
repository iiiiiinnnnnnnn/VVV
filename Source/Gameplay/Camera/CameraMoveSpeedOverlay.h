// CameraMoveSpeedOverlay.h
#pragma once

#include <algorithm>
#include <cstdio>
#include <imgui.h>
#include "Application/Time/GameTime.h"

namespace CameraMoveSpeedOverlay
{
constexpr float Duration = 1.0f;
constexpr float FadeDuration = 0.2f;

inline void Show(float& timer)
{
	timer = Duration;
}

inline void Draw(float moveSpeed, float& timer)
{
	if (timer <= 0.0f) return;

	timer = std::max(timer - Game::Time::unscaledDeltaTime, 0.0f);
	const float alpha = std::min(timer / FadeDuration, 1.0f);

	char text[64];
	sprintf_s(text, "%.1f", moveSpeed);

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 textSize = ImGui::CalcTextSize(text);
	constexpr ImVec2 padding(18.0f, 10.0f);
	const ImVec2 center(
		viewport->Pos.x + viewport->Size.x * 0.5f,
		viewport->Pos.y + viewport->Size.y * 0.5f);
	const ImVec2 backgroundMin(
		center.x - textSize.x * 0.5f - padding.x,
		center.y - textSize.y * 0.5f - padding.y);
	const ImVec2 backgroundMax(
		center.x + textSize.x * 0.5f + padding.x,
		center.y + textSize.y * 0.5f + padding.y);

	ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
	drawList->AddRectFilled(
		backgroundMin, backgroundMax,
		IM_COL32(20, 20, 20, static_cast<int>(210.0f * alpha)), 6.0f);
	drawList->AddRect(
		backgroundMin, backgroundMax,
		IM_COL32(255, 255, 255, static_cast<int>(45.0f * alpha)), 6.0f);
	drawList->AddText(
		ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
		IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)), text);
}
}
