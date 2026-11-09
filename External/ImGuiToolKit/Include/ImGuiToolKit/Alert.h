#pragma once

#include <string>

namespace ImGuiToolKit
{
// GUIスレッドから通知を登録する。ImGuiフレーム外からも呼び出せる。
void Alert(const std::string& message, const std::string& title = "Error");
// 全ウィンドウの描画後、ImGui::Render()より前に毎フレーム呼ぶ。
void DrawAlerts();
void ClearAlerts();
}
