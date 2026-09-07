// ResourceRefreshTest.cpp
#include "Resource/CacheBuilder.h"
#include "Resource/ResourceManager.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Resource/VSTG.h"

#include <fstream>
#include <array>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

void Check(bool success, const char* name)
{
	if (!success) throw std::runtime_error(name);
	std::cout << "PASS: " << name << '\n';
}

void WriteBitmap(const fs::path& path, char color)
{
	BITMAPFILEHEADER file{0x4d42, 102, 0, 0, 54};
	BITMAPINFOHEADER info{};
	info.biSize = 40;
	info.biWidth = info.biHeight = 4;
	info.biPlanes = 1;
	info.biBitCount = 24;
	info.biSizeImage = 48;
	std::array<char, 48> pixels;
	pixels.fill(color);
	std::ofstream output(path, std::ios::binary);
	output.write(reinterpret_cast<const char*>(&file), sizeof(file));
	output.write(reinterpret_cast<const char*>(&info), sizeof(info));
	output.write(pixels.data(), pixels.size());
}

int wmain(int argc, wchar_t** argv)
{
	try
	{
		if (argc != 2) return 1;
		const auto root = fs::absolute(argv[1]);
		const auto source = root / "Resources";
		const auto runtime = root / "Bin/Debug/Resources";
		fs::create_directories(source / "Model");
		fs::create_directories(runtime);
		std::ofstream(root / "Game.sln") << "fixture";
		const auto gltf = source / "Model/empty.gltf";
		std::ofstream(gltf) << R"({"asset":{"version":"2.0"},"nodes":[{"name":"root"}],"scenes":[{"nodes":[0]}],"scene":0})";
		const auto modelPath = source / "Model/empty.vmdl";
		// メッシュなしの実モデルを使い、描画初期化なしで保存と再読込を検証
		VMDLModel editor(gltf.string().c_str(), 60, modelPath.string().c_str());
		fs::create_directories(source / "Terrain/Layers");
		const auto firstImage = source / "Terrain/Layers/first.bmp";
		const auto secondImage = source / "Terrain/Layers/second.bmp";
		WriteBitmap(firstImage, 10);
		WriteBitmap(secondImage, 20);
		CacheSettings settings;
		Check(settings.Get("Resources/Image/UI/icon.PNG").preload &&
			!settings.Get("Resources/Image/not-an-image.bin").preload &&
			!settings.Get("Resources/Other/icon.png").preload, "only Resources/Image images preload by default");
		settings.Set("Resources/Model/empty.vmdl", {false, true});
		settings.Set("Resources/Stage/test.vstg", {false, true});
		settings.Save(source / "ResourceSettings.ini");
		VSTG stage;
		Check(stage.Save(source / "Stage/test.vstg"), "save preload stage fixture");
		CacheBuilder::Build(source, runtime);
		Check(!fs::exists(runtime / "ResourceSettings.ini") &&
			fs::exists(source / "ResourceSettings.ini"),
			"runtime uses manifest only and source settings remain editable");
		Check(fs::exists(runtime / "Terrain/Layers/second.dds"), "multiple DDS conversions in one build");
		CacheBuilder::Build(source, runtime, true);
		Check(true, "repeated DDS builds in the same process");
		fs::current_path(runtime.parent_path());
		auto& resources = ResourceManager::Instance();
		Check(resources.PrepareGameResources(), "load generated resource list");
		Check(resources.ResolveSourcePath(source / "Vmdl/empty.vmdl") == source / "Model/empty.vmdl",
			"old model history resolves to renamed source folder");
		Check(resources.ResolveSourcePath(source / "Vstg/test.vstg") == source / "Stage/test.vstg",
			"old stage history resolves to renamed source folder");
		Check(resources.ResolveSourcePath("Resources/Vmdl/empty.vmdl") == source / "Model/empty.vmdl",
			"old relative model history resolves to source");
		fs::rename(runtime / "Model/empty.vmdl", runtime / "Model/empty.hold");
		auto oldModel = resources.LoadModel("Resources/Model/empty.vmdl");
		Check(oldModel != nullptr, "configured model is already in memory at startup");
		fs::rename(runtime / "Model/empty.hold", runtime / "Model/empty.vmdl");
		fs::rename(runtime / "Stage/test.vstg", runtime / "Stage/test.hold");
		VSTG preloadedStage;
		Check(preloadedStage.Load("Resources/Stage/test.vstg"), "VSTG consumes preloaded file without disk access");
		fs::rename(runtime / "Stage/test.hold", runtime / "Stage/test.vstg");
		Check(oldModel && oldModel->GetModelScale() == 1, "initial model loaded and cached");
		const auto originalTime = fs::last_write_time(modelPath);
		editor.SetModelScale(2);
		Check(editor.SaveVmdl(), "model save succeeds");
		fs::last_write_time(modelPath, originalTime);
		Check(resources.RefreshResources(modelPath), "save notification refreshes runtime data");
		auto updated = resources.LoadModel("Resources/Model/empty.vmdl");
		Check(updated && updated->GetModelScale() == 2, "next load uses saved model instead of old memory cache");
		Check(oldModel->GetModelScale() == 1, "existing model instances remain valid");
		Check(resources.ResolveSourcePath("Resources/Model/empty.vmdl") == modelPath &&
			resources.ResolveSourcePath(runtime / "Model/empty.vmdl") == modelPath,
			"editor runtime paths resolve to source Resources");
		const auto outside = root / "outside.vmdl";
		Check(resources.ResolveSourcePath(outside) == outside, "external save path remains unchanged");

		editor.SetModelScale(3);
		const HANDLE locked = CreateFileW(modelPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Check(locked != INVALID_HANDLE_VALUE, "lock fixture for failed save");
		const bool saved = editor.SaveVmdl();
		CloseHandle(locked);
		Check(!saved, "failed file replacement is reported as save failure");
		VMDLModel preserved(modelPath.string().c_str());
		Check(preserved.GetModelScale() == 2, "failed save preserves previous model file");
		Check(editor.SaveVmdl() && resources.RefreshResources(modelPath), "retry updates model after failure");
		Check(resources.LoadModel("Resources/Model/empty.vmdl")->GetModelScale() == 3, "retry invalidates memory cache");

		const auto newPath = source / "Model/new.vmdl";
		Check(editor.SaveVmdl(newPath) && resources.RefreshResources(newPath), "save as registers new resource");
		auto added = resources.LoadModel("Resources/Model/new.vmdl");
		Check(added && added->GetModelScale() == 3, "new model loads without restarting game");
		editor.SetModelScale(4);
		Check(editor.SaveVmdl(), "save before external build");
		CacheBuilder::Build(source, runtime);
		Check(resources.RefreshResources(), "refresh after external build");
		Check(resources.LoadModel("Resources/Model/new.vmdl")->GetModelScale() == 4,
			"already rebuilt file also invalidates old memory cache");

		// 保存通知を通らない変更もシーン移動時に反映
		auto& scenes = SceneManager::Instance();
		const auto beforeSave = fs::last_write_time(newPath);
		editor.SetModelScale(5);
		Check(editor.SaveVmdl(), "save model before scene transition");
		fs::last_write_time(newPath, beforeSave + fs::file_time_type::duration(1));
		Check(scenes.LoadScene<Scene>(), "scene request checks changed resources");
		Check(resources.LoadModel("Resources/Model/new.vmdl")->GetModelScale() == 5,
			"edited model is ready before destination scene construction");
		scenes.Finalize();
		const auto outputTime = fs::last_write_time(runtime / "Model/new.vmdl");
		const auto manifestTime = fs::last_write_time(runtime / "ResourceManifest.ini");
		Check(scenes.LoadScene<Scene>(), "next scene request checks unchanged resources");
		Check(fs::last_write_time(runtime / "Model/new.vmdl") == outputTime &&
			fs::last_write_time(runtime / "ResourceManifest.ini") == manifestTime,
			"unchanged scene transition does not rewrite caches");
		scenes.Finalize();
		std::ofstream(firstImage) << "broken image";
		Check(!scenes.LoadScene<Scene>(), "cache failure blocks scene transition");
		Check(scenes.GetLastLoadError().find("DDS generation failed") != std::string::npos,
			"scene transition reports cache failure");
		WriteBitmap(firstImage, 30);
		Check(scenes.LoadScene<Scene>(), "scene transition retries after cache repair");
		scenes.Finalize();
		settings.Set("Resources/Model/new.vmdl", {true, true});
		settings.Save(source / "ResourceSettings.ini");
		Check(resources.RefreshResources(), "apply exclusion settings");
		Check(!fs::exists(runtime / "Model/new.vmdl") && fs::exists(newPath) &&
			!resources.LoadModel("Resources/Model/new.vmdl"), "exclusion removes output and memory cache but preserves source");
		const auto candidates = CacheBuilder::ListResources(source);
		Check(std::find(candidates.begin(), candidates.end(), "Resources/Model/new.vmdl") != candidates.end(),
			"excluded resources remain available in manager list");
		settings.Set("Resources/Model/new.vmdl", {false, true});
		settings.Save(source / "ResourceSettings.ini");
		Check(resources.RefreshResources() && resources.LoadModel("Resources/Model/new.vmdl")->GetModelScale() == 5,
			"reenabling restores and preloads resource");
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
