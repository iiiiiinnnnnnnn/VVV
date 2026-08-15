#pragma once

#include <Windows.h>
#include <string>

// ダイアログリザルト
enum class DialogResult
{
	Yes,
	No,
	OK,
	Cancel
};

// ダイアログ
class Dialog
{
public:
	// [ファイルを開く]ダイアログボックスを表示
	static DialogResult OpenFileName(
		char* filepath,
		int size,
		const char* filter = nullptr,
		const char* title = nullptr,
		const char* initialDir = nullptr,
		HWND hWnd = NULL,
		bool multiSelect = false);
	static DialogResult OpenFileName(
		std::string& filepath,
		const char* filter = nullptr,
		const char* title = nullptr,
		const char* initialDir = nullptr,
		HWND hWnd = NULL);

	// [ファイルを保存]ダイアログボックスを表示
	static DialogResult SaveFileName(
		char* filepath,
		int size,
		const char* filter = nullptr,
		const char* title = nullptr,
		const char* ext = nullptr,
		const char* initialDir = nullptr,
		HWND hWnd = NULL);
	static DialogResult SaveFileName(
		std::string& filepath,
		const char* filter = nullptr,
		const char* title = nullptr,
		const char* ext = nullptr,
		const char* initialDir = nullptr,
		HWND hWnd = NULL);
};
