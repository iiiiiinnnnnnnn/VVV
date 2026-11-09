#include <ImGuiToolKit/ImGuiToolKit.h>
#include "EditorScene.h"
#include "Rendering/Renderer/ImGuiRenderer.h"
#include <imguizmo/ImGuizmo.h>

void EditorScene::DrawCommonVisibilityGUI(bool* meshVisibility)
{
	const struct { const char* label; ShadingMode mode; } modes[] = {
		{(const char*)(ICON_FA_ADJUST u8" シェーディング"), ShadingMode::Shading},
		{(const char*)(ICON_FA_SUN    u8" アンリット"), ShadingMode::Unlit},
		{(const char*)(ICON_FA_ADJUST u8" ソリッド"), ShadingMode::Solid},
		{(const char*)(ICON_FA_ADJUST u8" ワイヤー"), ShadingMode::Wire},
		{(const char*)(ICON_FA_ADJUST u8" ソリッド＋ワイヤー"), ShadingMode::SolidAndWire},
	};
	for (const auto& item : modes)
		if (ImGui::MenuItem(item.label, nullptr, renderSettings.shadingMode == item.mode))
			renderSettings.shadingMode = item.mode;
	if (meshVisibility)
		ImGui::MenuItem((const char*)(ICON_FA_CUBE u8" メッシュ"), "M", meshVisibility);
	ImGui::Separator();
	ImGui::MenuItem((const char*)(ICON_FA_LEAF u8" 草マップ"), "G", &renderSettings.showGrass);
	ImGui::MenuItem((const char*)(ICON_FA_SMOG u8" フォグ"), "F", &renderSettings.showFog);
	ImGui::Separator();
}

void EditorScene::DrawSerialViewerTools()
{
	if (ImGui::MenuItem((const char*)(ICON_FA_SITEMAP u8" シリアルビューアー")))
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
	std::string title = path.empty() ? (const char*)u8"名称未設定"
		: std::string(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
	if (dirty) title += " *";
	DrawEditorText(title);
}

void EditorScene::DrawGizmoButtons(int& operation, bool showLabels)
{
	const struct { const char* icon; const char* label; const char* tooltip; int operation; } buttons[] = {
		{ICON_FA_ARROWS_ALT, (const char*)(ICON_FA_ARROWS_ALT u8" 移動"), (const char*)u8"移動 (W)", ImGuizmo::TRANSLATE},
		{ICON_FA_SYNC_ALT, (const char*)(ICON_FA_SYNC_ALT u8" 回転"), (const char*)u8"回転 (E)", ImGuizmo::ROTATE},
		{ICON_FA_EXPAND_ARROWS_ALT, (const char*)(ICON_FA_EXPAND_ARROWS_ALT u8" サイズ"), (const char*)u8"サイズ (R)", ImGuizmo::SCALE},
	};
	for (const auto& button : buttons)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton(showLabels ? button.label : button.icon)) operation = button.operation;
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
		io.KeyAlt || io.KeySuper || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) return;
	if (ImGui::IsKeyPressed(ImGuiKey_F, false)) renderSettings.showFog = !renderSettings.showFog;
	if (ImGui::IsKeyPressed(ImGuiKey_G, false)) renderSettings.showGrass = !renderSettings.showGrass;
}
