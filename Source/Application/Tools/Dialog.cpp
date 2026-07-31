// Dialog.cpp

#include "Application/Tools/Dialog.h"
#include <filesystem>

static char pathBuffer[MAX_PATH];

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
	std::string dirname;

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

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = size;
	ofn.lpstrTitle = title;
	ofn.lpstrInitialDir = dirname.empty() ? nullptr : dirname.c_str();
	ofn.Flags =
		OFN_FILEMUSTEXIST |
		OFN_HIDEREADONLY |
		OFN_NOCHANGEDIR;

	if (multiSelect)
	{
		ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_EXPLORER;
	}

	if (::GetOpenFileNameA(&ofn) == FALSE)
	{
		return DialogResult::Cancel;
	}

	strcpy_s(pathBuffer, MAX_PATH, filepath);
	return DialogResult::OK;
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
	std::filesystem::path initialDirectory;
	if (initialDir && initialDir[0] != '\0')
		initialDirectory = initialDir;
	else if (filepath[0] != '\0')
		initialDirectory = std::filesystem::path(filepath).parent_path();
	else if (pathBuffer[0] != '\0')
		initialDirectory = std::filesystem::path(pathBuffer).parent_path();

	const std::string dirname = initialDirectory.string();

	if (filter == nullptr)
	{
		filter = "All Files\0*.*\0\0";
	}

	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = filepath;
	ofn.nMaxFile = size;
	ofn.lpstrTitle = title;
	ofn.lpstrInitialDir =
		dirname.empty() ? nullptr : dirname.c_str();
	ofn.lpstrDefExt = ext;
	ofn.Flags =
		OFN_OVERWRITEPROMPT |
		OFN_HIDEREADONLY |
		OFN_PATHMUSTEXIST;

	char currentDir[MAX_PATH]{};

	if (!::GetCurrentDirectoryA(MAX_PATH, currentDir))
	{
		currentDir[0] = '\0';
	}

	if (::GetSaveFileNameA(&ofn) == FALSE)
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

	std::filesystem::path selectedPath(filepath);
	if (ext && ext[0] != '\0')
	{
		std::string extension = ext;
		if (extension.front() != '.') extension.insert(extension.begin(), '.');
		selectedPath.replace_extension(extension);
	}
	else if (ofn.nFilterIndex == 1)
	{
		selectedPath.replace_extension(".dds");
	}
	else if (ofn.nFilterIndex == 2)
	{
		selectedPath.replace_extension(".png");
	}

	const std::string finalPath = selectedPath.string();

	if (finalPath.size() + 1 > static_cast<size_t>(size))
	{
		return DialogResult::Cancel;
	}

	strcpy_s(filepath, size, finalPath.c_str());
	strcpy_s(pathBuffer, MAX_PATH, filepath);

	return DialogResult::OK;
}
