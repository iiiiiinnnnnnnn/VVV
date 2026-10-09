#include "Application/Tools/Dialog.h"
#include <array>
#include <algorithm>
#include <filesystem>
#include <cstring>
#include <vector>
#include "imgui.h"

static std::string lastPath;

static std::wstring DialogToWide(const char* text, UINT codePage, int length = -1)
{
	if (!text) return {};
	const int count = MultiByteToWideChar(codePage, 0, text, length, nullptr, 0);
	if (!count) return {};
	std::wstring result(count, L'\0');
	MultiByteToWideChar(codePage, 0, text, length, result.data(), count);
	return result;
}

static std::wstring DialogFilterToWide(const char* filter)
{
	const char* end = filter;
	while (*end) end += strlen(end) + 1;
	return DialogToWide(filter, CP_UTF8, static_cast<int>(end - filter + 1));
}

static bool CopyDialogPath(const wchar_t* source, char* destination, int size, bool multiSelect = false)
{
	const wchar_t* end = source + wcslen(source);
	if (multiSelect)
	{
		++end;
		while (*end) end += wcslen(end) + 1;
	}
	const int length = static_cast<int>(end - source + 1);
	const int count = WideCharToMultiByte(CP_ACP, 0, source, length, nullptr, 0, nullptr, nullptr);
	if (!count || count > size) return false;
	return WideCharToMultiByte(CP_ACP, 0, source, length, destination, size, nullptr, nullptr) != 0;
}

// [ファイルを開く]ダイアログボックスを表示
DialogResult Dialog::OpenFileName(
	char* filepath,
	int size,
	const char* filter,
	const char* title,
	const char* initialDir,
	HWND hWnd,
	bool multiSelect)
{
	if (!filepath || size <= 0) return DialogResult::Cancel;
	std::string dirname;
	if (!hWnd) hWnd = ::GetActiveWindow();
	if (!hWnd) hWnd = ::GetForegroundWindow();
	if (hWnd)
	{
		HWND rootWindow = ::GetAncestor(hWnd, GA_ROOTOWNER);
		if (rootWindow) hWnd = rootWindow;
		::SetForegroundWindow(hWnd);
	}

	if (initialDir != nullptr && initialDir[0] != '\0')
	{
		dirname = initialDir;
	}
	else if (filepath[0] != '\0')
	{
		dirname = std::filesystem::path(filepath).parent_path().string();
	}

	if (filter == nullptr)
	{
		filter = "All Files\0*.*\0\0";
	}

	const auto wideFilter = DialogFilterToWide(filter);
	const auto wideTitle = DialogToWide(title, CP_UTF8);
	const auto wideDirectory = DialogToWide(dirname.c_str(), CP_ACP);
	const auto initialFile = DialogToWide(filepath, CP_ACP);
	std::vector<wchar_t> wideFile(size);
	if (initialFile.size() > wideFile.size()) return DialogResult::Cancel;
	std::copy(initialFile.begin(), initialFile.end(), wideFile.begin());
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = wideFilter.c_str();
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = wideFile.data();
	ofn.nMaxFile = size;
	ofn.lpstrTitle = title ? wideTitle.c_str() : nullptr;
	ofn.lpstrInitialDir = dirname.empty() ? nullptr : wideDirectory.c_str();
	ofn.Flags =
		OFN_FILEMUSTEXIST |
		OFN_HIDEREADONLY |
		OFN_NOCHANGEDIR;

	if (multiSelect)
	{
		ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	}

	const BOOL accepted = ::GetOpenFileNameW(&ofn);
	if (hWnd)
	{
		::SetForegroundWindow(hWnd);
		::BringWindowToTop(hWnd);
	}
	// ダイアログ中に届かなかったキー・マウスの解放を反映する。
	// Ctrlの押下状態が残ると、DragFloatがクリックで直接入力に入ってしまう。
	if (ImGui::GetCurrentContext())
	{
		auto& io = ImGui::GetIO();
		io.ClearEventsQueue();
		io.ClearInputKeys();
		io.ClearInputMouse();
		io.AddFocusEvent(::GetActiveWindow() != nullptr);
	}
	if (accepted == FALSE)
	{
		return DialogResult::Cancel;
	}

	if (!CopyDialogPath(wideFile.data(), filepath, size, multiSelect)) return DialogResult::Cancel;
	lastPath = filepath;
	return DialogResult::OK;
}

DialogResult Dialog::OpenFileName(
	std::string& filepath,
	const char* filter,
	const char* title,
	const char* initialDir,
	HWND hWnd)
{
	std::array<char, MAX_PATH> buffer{};
	if (filepath.size() >= buffer.size()) return DialogResult::Cancel;
	strcpy_s(buffer.data(), buffer.size(), filepath.c_str());
	const DialogResult result = OpenFileName(
		buffer.data(), static_cast<int>(buffer.size()), filter, title, initialDir, hWnd);
	if (result == DialogResult::OK) filepath = buffer.data();
	return result;
}

// [ファイルを保存]ダイアログボックスを表示
DialogResult Dialog::SaveFileName(
	char* filepath,
	int size,
	const char* filter,
	const char* title,
	const char* ext,
	const char* initialDir,
	HWND hWnd)
{
	if (!filepath || size <= 0) return DialogResult::Cancel;
	std::filesystem::path initialDirectory;
	if (!hWnd) hWnd = ::GetActiveWindow();
	if (!hWnd) hWnd = ::GetForegroundWindow();
	if (hWnd)
	{
		HWND rootWindow = ::GetAncestor(hWnd, GA_ROOTOWNER);
		if (rootWindow) hWnd = rootWindow;
		::SetForegroundWindow(hWnd);
	}
	if (initialDir && initialDir[0] != '\0')
		initialDirectory = initialDir;
	else if (filepath[0] != '\0')
		initialDirectory = std::filesystem::path(filepath).parent_path();
	else if (!lastPath.empty())
		initialDirectory = std::filesystem::path(lastPath).parent_path();

	const std::string dirname = initialDirectory.string();

	if (filter == nullptr)
	{
		filter = "All Files\0*.*\0\0";
	}

	const auto wideFilter = DialogFilterToWide(filter);
	const auto wideTitle = DialogToWide(title, CP_UTF8);
	const auto wideDirectory = DialogToWide(dirname.c_str(), CP_ACP);
	const auto wideExtension = DialogToWide(ext, CP_UTF8);
	const auto initialFile = DialogToWide(filepath, CP_ACP);
	std::vector<wchar_t> wideFile(size);
	if (initialFile.size() > wideFile.size()) return DialogResult::Cancel;
	std::copy(initialFile.begin(), initialFile.end(), wideFile.begin());
	OPENFILENAMEW ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = wideFilter.c_str();
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = wideFile.data();
	ofn.nMaxFile = size;
	ofn.lpstrTitle = title ? wideTitle.c_str() : nullptr;
	ofn.lpstrInitialDir =
		dirname.empty() ? nullptr : wideDirectory.c_str();
	ofn.lpstrDefExt = ext ? wideExtension.c_str() : nullptr;
	ofn.Flags =
		OFN_OVERWRITEPROMPT |
		OFN_HIDEREADONLY |
		OFN_PATHMUSTEXIST;

	char currentDir[MAX_PATH]{};

	if (!::GetCurrentDirectoryA(MAX_PATH, currentDir))
	{
		currentDir[0] = '\0';
	}

	const BOOL accepted = ::GetSaveFileNameW(&ofn);
	if (hWnd)
	{
		::SetForegroundWindow(hWnd);
		::BringWindowToTop(hWnd);
	}
	// ダイアログ中に届かなかったキー・マウスの解放を反映する。
	// Ctrlの押下状態が残ると、DragFloatがクリックで直接入力に入ってしまう。
	if (ImGui::GetCurrentContext())
	{
		auto& io = ImGui::GetIO();
		io.ClearEventsQueue();
		io.ClearInputKeys();
		io.ClearInputMouse();
		io.AddFocusEvent(::GetActiveWindow() != nullptr);
	}
	if (accepted == FALSE)
	{
		if (currentDir[0] != '\0')
		{
			::SetCurrentDirectoryA(currentDir);
		}

		return DialogResult::Cancel;
	}

	if (currentDir[0] != '\0')
	{
		::SetCurrentDirectoryA(currentDir);
	}

	if (!CopyDialogPath(wideFile.data(), filepath, size)) return DialogResult::Cancel;
	std::filesystem::path selectedPath(filepath);
	if (ext && ext[0] != '\0')
	{
		std::string extension = ext;
		if (extension.front() != '.') extension.insert(extension.begin(), '.');
		selectedPath.replace_extension(extension);
	}
	else if (ofn.nFilterIndex == 1)
	{
		selectedPath.replace_extension(".png");
	}
	else if (ofn.nFilterIndex == 2)
	{
		selectedPath.replace_extension(".dds");
	}

	const std::string finalPath = selectedPath.string();

	if (finalPath.size() + 1 > static_cast<size_t>(size))
	{
		return DialogResult::Cancel;
	}

	strcpy_s(filepath, size, finalPath.c_str());
	lastPath = filepath;

	return DialogResult::OK;
}

DialogResult Dialog::SaveFileName(
	std::string& filepath,
	const char* filter,
	const char* title,
	const char* ext,
	const char* initialDir,
	HWND hWnd)
{
	std::array<char, MAX_PATH> buffer{};
	if (filepath.size() >= buffer.size()) return DialogResult::Cancel;
	strcpy_s(buffer.data(), buffer.size(), filepath.c_str());
	const DialogResult result = SaveFileName(
		buffer.data(), static_cast<int>(buffer.size()), filter, title, ext, initialDir, hWnd);
	if (result == DialogResult::OK) filepath = buffer.data();
	return result;
}
