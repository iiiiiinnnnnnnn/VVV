// Main.cpp
#include "Resource/CacheBuilder.h"

#include <iostream>
#include <stdexcept>
#include <string_view>

int wmain(int argc, wchar_t** argv)
{
	try
	{
		std::filesystem::path source, output, saved;
		bool force = false;
		for (int i = 1; i < argc; ++i)
		{
			const std::wstring_view argument = argv[i];
			if (argument == L"--help")
			{
				std::cout << "CacheBuilder [--force] [--source <Resources>] [--output <Resources>] [--saved-source <file>]\n";
				return 0;
			}
			if (argument == L"--force") { force = true; continue; }
			if (argument != L"--source" && argument != L"--output" && argument != L"--saved-source")
				throw std::runtime_error("Unknown argument");
			if (++i >= argc) throw std::runtime_error("Missing argument value");
			if (argument == L"--source") source = argv[i];
			else if (argument == L"--output") output = argv[i];
			else saved = argv[i];
		}
		if (source.empty() || output.empty())
		{
			wchar_t filename[32768]{};
			if (!GetModuleFileNameW(nullptr, filename, 32768)) throw std::runtime_error("Cannot find executable");
			const auto directory = std::filesystem::path(filename).parent_path();
			auto root = directory;
			while (!std::filesystem::exists(root / "Game.sln"))
			{
				if (root == root.root_path()) throw std::runtime_error("Game.sln was not found");
				root = root.parent_path();
			}
			if (source.empty()) source = root / "Resources";
			if (output.empty()) output = root / "Bin" / directory.filename() / "Resources";
		}
		CacheBuilder::Build(source, output, force, saved);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "CacheBuilder error: " << error.what() << '\n';
		return 1;
	}
}
