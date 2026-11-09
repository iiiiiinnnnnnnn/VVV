#pragma once

#include <InputKit/GamePad.h>
#include <InputKit/Mouse.h>
#include <InputKit/InputActions.h>
#include <array>
#include <filesystem>
#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace InputKit
{
class InputSystem
{
  public:
	struct File
	{
		std::filesystem::path path;
		InputActions actions;
	};

	static InputSystem& Instance();

	void Initialize(HWND hwnd, const std::filesystem::path& directory = "Resources/Input");
	void Update();

	bool Load(const std::filesystem::path& path);
	bool Save();
	bool Save(const std::filesystem::path& path);

	const std::filesystem::path& GetFilePath() const
	{
		return files[selectedFile].path;
	}
	const std::vector<File>& GetFiles() const { return files; }
	size_t GetSelectedFile() const { return selectedFile; }
	void SelectFile(size_t index)
	{
		if (index < files.size()) selectedFile = index;
	}
	const std::string& GetLastError() const
	{
		return lastError;
	}

	// マウス基準のビューポート
	void SetViewportSize(UINT width, UINT height)
	{
		viewportWidth = width;
		viewportHeight = height;
	}
	void SetFocusFilter(bool (*filter)(bool))
	{
		focusCheck = filter;
	}

	static bool IsFocusedWindow(bool ignoreUI = false);
	void SuppressGameplayInput(int frames = 2)
	{
		suppressionFrames = std::max(suppressionFrames, frames);
	}
	bool IsGameplayInputSuppressed() const
	{
		return suppressionFrames > 0;
	}
	bool CanReadActions() const;
	void Update(bool acceptsInput, UINT viewportWidth, UINT viewportHeight);

	bool IsPressed(const std::string& action, bool physicalInput = false) const;
	bool IsDown(const std::string& action, bool ignoreUI = false) const;
	bool IsUp(const std::string& action) const;

	// Vector2の入力取得
	template <class T>
	T ReadValue(std::string_view action) const
	{
		return ReadVector2(action);
	}

	void Bind(const std::string& action, Binding binding);
	void SetBindings(const std::string& action, std::vector<Binding> bindings);
	void RemoveAction(const std::string& action);

	// GUIで選択しているファイルの定義。入力判定は全ファイルを対象にする。
	InputActions& GetActions()
	{
		return files[selectedFile].actions;
	}
	const InputActions& GetActions() const
	{
		return files[selectedFile].actions;
	}
	InputActionMap* FindActionMap(std::string_view name);
	const InputActionMap* FindActionMap(std::string_view name) const;
	bool RenameMap(std::string_view oldName, std::string_view newName);
	InputAction* FindAction(std::string_view path);
	const InputAction* FindAction(std::string_view path) const;

	ButtonState ReadBindings(std::span<const Binding> bindings, bool physicalInput = false) const;
	float ReadAxis(Axis axis) const;
	GamePad& GetGamePad()
	{
		return gamePad;
	}
	const GamePad& GetGamePad() const
	{
		return gamePad;
	}
	Mouse& GetMouse()
	{
		return mouse;
	}
	const Mouse& GetMouse() const
	{
		return mouse;
	}

  private:
	InputSystem() : mouse(nullptr) {}
	InputSystem(const InputSystem&) = delete;
	InputSystem& operator=(const InputSystem&) = delete;

	void LoadAll(const std::filesystem::path& directory);
	std::vector<File> files{File{}};
	size_t selectedFile = 0;
	std::string lastError;
	HWND window = nullptr;
	UINT viewportWidth = 0;
	UINT viewportHeight = 0;
	bool (*focusCheck)(bool) = nullptr;
	int suppressionFrames = 0;
	Vector2 ReadVector2(std::string_view path) const;
	Vector2 ReadVector2(const Vector2Binding& binding) const;
	std::array<bool, 256> keys{};
	std::array<bool, 256> previousKeys{};
	GamePad gamePad;
	Mouse mouse;
	bool enabled = false;
};
} // namespace InputKit