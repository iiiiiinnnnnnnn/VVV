#pragma once
#include <InputKit/InputSystem.h>

namespace InputKit
{
class InputSystemGUI
{
public:
	static InputSystemGUI& Instance()
	{
		static InputSystemGUI instance;
		return instance;
	}
	void DrawGUI(InputSystem& system);

private:
	void DrawFileControls(InputSystem& system);
	void DrawMaps(InputSystem& system);
	void DrawActions(InputActions& actions);
	bool DrawBindings(InputSystem& system, const char* label, std::vector<Binding>& bindings);
	void DrawDetails(InputSystem& system);
	bool DrawVector2Editor(InputAction& action);
	bool DrawBindingEditor(InputSystem& system, Binding& binding);

	std::string filePathEdit;
	std::string filePathSeen;
	std::string selectedMap;
	std::string editingTarget;
	std::string editName;
	bool nameError = false;
	std::string selectedAction;
};
}
