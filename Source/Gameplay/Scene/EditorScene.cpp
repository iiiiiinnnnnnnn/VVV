#include <ImGuiToolKit/ImGuiToolKit.h>
#include <ImGuiToolKit/Widgets.h>
#include "EditorScene.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include <imguizmo/ImGuizmo.h>

void EditorScene::DrawCommonVisibilityGUI(bool* meshVisibility)
{
	const struct { const char* label; ShadingMode mode; } modes[] = {
		{JText(ICON_FA_ADJUST u8" シェーディング"), ShadingMode::Shading},
		{JText(ICON_FA_SUN    u8" アンリット"), ShadingMode::Unlit},
		{JText(ICON_FA_ADJUST u8" ソリッド"), ShadingMode::Solid},
		{JText(ICON_FA_ADJUST u8" ワイヤー"), ShadingMode::Wire},
		{JText(ICON_FA_ADJUST u8" ソリッド＋ワイヤー"), ShadingMode::SolidAndWire},
	};
	for (const auto& item : modes)
		if (ImGui::MenuItem(item.label, nullptr, renderSettings.shadingMode == item.mode))
			renderSettings.shadingMode = item.mode;
	if (meshVisibility)
		ImGuiToolKit::MenuItem(ICON_FA_CUBE u8" メッシュ", "M", meshVisibility);
	ImGui::Separator();
	ImGuiToolKit::MenuItem(ICON_FA_LEAF u8" 草マップ", "G", &renderSettings.showGrass);
	ImGuiToolKit::MenuItem(ICON_FA_SMOG u8" フォグ", "F", &renderSettings.showFog);
	ImGui::Separator();
}

void EditorScene::DrawSerialViewerTools()
{
	if (ImGuiToolKit::MenuItem(ICON_FA_SITEMAP u8" シリアルビューアー"))
	{
		showSerialViewer = true;
		serialViewerDocument = nullptr;
	}
}

void EditorScene::DrawSerialViewer()
{
	if (!showSerialViewer) return;
	try
	{
		if (!serialViewerDocument) serialViewerDocument = PrepareSerialDocument();
		if (!serialViewerDocument) return;
		if (serialViewer.Draw(*serialViewerDocument, &showSerialViewer)) OnSerialDocumentChanged();
	}
	catch (const std::exception& exception)
	{
		showSerialViewer = false;
		ImGuiToolKit::Alert(exception.what());
	}
}

void EditorScene::DrawDocumentTitle(const std::filesystem::path& path, bool dirty)
{
	const auto utf8Path = path.u8string();
	std::string title = path.empty() ? JText(u8"未保存")
		: std::string(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
	if (dirty) title += " *";
	DrawEditorText(title);
}

void EditorScene::DrawGizmoButtons(int& operation)
{
	const struct { const char* icon; const char* tooltip; int operation; } buttons[] = {
		{ICON_FA_ARROWS_ALT, JText(u8"移動 (W)"), ImGuizmo::TRANSLATE},
		{ICON_FA_SYNC_ALT, JText(u8"回転 (E)"), ImGuizmo::ROTATE},
		{ICON_FA_EXPAND_ARROWS_ALT, JText(u8"サイズ (R)"), ImGuizmo::SCALE},
	};
	for (const auto& button : buttons)
	{
		ImGui::SameLine();
		if (ImGui::Button(button.icon)) operation = button.operation;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", button.tooltip);
	}
}

void EditorScene::HandleGizmoShortcuts(int& operation)
{
	const auto& io = ImGui::GetIO();
	if (!InputKit::InputSystem::IsFocusedWindow(true) || io.WantTextInput || io.KeyCtrl || io.KeyShift ||
		io.KeyAlt || io.KeySuper || ImGui::IsAnyItemActive() ||
		ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) ||
		ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGuizmo::IsUsingAny()) return;
	if (ImGui::IsKeyPressed(ImGuiKey_W, false)) operation = ImGuizmo::TRANSLATE;
	else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) operation = ImGuizmo::ROTATE;
	else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) operation = ImGuizmo::SCALE;
}

void EditorScene::HandleVisibilityShortcuts()
{
	const auto& io = ImGui::GetIO();
	if (!InputKit::InputSystem::IsFocusedWindow(true) || io.WantTextInput || io.KeyCtrl || io.KeyShift ||
		io.KeyAlt || io.KeySuper || ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
		ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
		ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) return;
	if (ImGui::IsKeyPressed(ImGuiKey_F, false)) renderSettings.showFog = !renderSettings.showFog;
	if (ImGui::IsKeyPressed(ImGuiKey_G, false)) renderSettings.showGrass = !renderSettings.showGrass;
}

// エディターの左右パネルはゲーム内GUIとは別に配置されるため、ここで境界を扱う
bool EditorScene::UpdatePanelWidths(const ImVec2& position, float totalWidth, float height,
	float& leftWidth, float& rightWidth)
{
	const auto& io = ImGui::GetIO();
	const float minimumWidth = std::min(180.0f, totalWidth * 0.2f);
	const float maximumTotal = std::max(minimumWidth * 2.0f, totalWidth - std::min(240.0f, totalWidth * 0.3f));
	if (!std::isfinite(leftWidth)) leftWidth = 340.0f;
	if (!std::isfinite(rightWidth)) rightWidth = 300.0f;
	leftWidth = std::clamp(leftWidth, minimumWidth, maximumTotal - minimumWidth);
	rightWidth = std::clamp(rightWidth, minimumWidth, maximumTotal - leftWidth);
	int hoveredEdge = 0;
	if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
	{
		const auto hitsEdge = [&](float x) {
			return ImGui::IsMouseHoveringRect({x - 6.0f, position.y}, {x + 6.0f, position.y + height}, false);
		};
		if (hitsEdge(position.x + leftWidth)) hoveredEdge = 1;
		else if (hitsEdge(position.x + totalWidth - rightWidth)) hoveredEdge = 2;
	}
	const bool released = panelResizeEdge && !ImGui::IsMouseDown(ImGuiMouseButton_Left);
	if (released) panelResizeEdge = 0;
	if (hoveredEdge && !ImGui::IsAnyItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		panelResizeEdge = hoveredEdge;
	panelResizeHovered = hoveredEdge || panelResizeEdge;
	if (panelResizeHovered)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		ImGui::SetNextFrameWantCaptureMouse(true);
	}
	if (panelResizeEdge == 1)
		leftWidth = std::clamp(leftWidth + io.MouseDelta.x, minimumWidth, maximumTotal - rightWidth);
	else if (panelResizeEdge == 2)
		rightWidth = std::clamp(rightWidth - io.MouseDelta.x, minimumWidth, maximumTotal - leftWidth);
	ImDrawList* drawList = ImGui::GetForegroundDrawList(ImGui::GetMainViewport());
	for (int edge = 1; edge <= 2; ++edge)
	{
		const float x = position.x + (edge == 1 ? leftWidth : totalWidth - rightWidth);
		const bool active = panelResizeEdge == edge;
		const bool hovered = hoveredEdge == edge;
		drawList->AddLine({x, position.y}, {x, position.y + height},
			ImGui::GetColorU32(active ? ImGuiCol_SeparatorActive : hovered ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator),
			active || hovered ? 3.0f : 1.0f);
	}
	return released;
}
