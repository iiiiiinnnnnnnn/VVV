#pragma once

#include "Scene.h"
#include "Application/SettingsAndDebug/SerialViewer.h"
#include <filesystem>

class EditorScene : public Scene
{
  protected:
	MouseCursorMode GetMouseCursorMode() const override { return MouseCursorMode::VisibleFree; }
	bool UpdatePanelWidths(const ImVec2& position, float totalWidth, float height, float& leftWidth, float& rightWidth);
	int panelResizeEdge = 0;
	bool panelResizeHovered = false;
	void DrawCommonVisibilityGUI(bool* meshVisibility = nullptr);
	void DrawSerialViewerTools();
	void DrawSerialViewer();
	void DrawDocumentTitle(const std::filesystem::path& path, bool dirty);
	void DrawGizmoButtons(int& operation);
	void HandleVisibilityShortcuts();
	void HandleGizmoShortcuts(int& operation);

	virtual SerialDocument* PrepareSerialDocument() = 0;
	virtual void OnSerialDocumentChanged() = 0;

	bool showSerialViewer = false;
	SerialDocument* serialViewerDocument = nullptr;
	SerialViewer serialViewer;
};
