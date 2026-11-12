#include "StageTitle.h"
#include "Rendering/Core/Graphics.h"

#include <filesystem>

void StageTitle::OnAwake()
{
	if (!LoadVSTG("cave_02"))
		throw std::runtime_error("Failed to load stage");

	Camera* camera = GetDefaultCameraActor()->GetComponent<Camera>();
	const Vector3 eye{64.835f, 2.609f, -18.214f};
	const Quaternion rotation = Quaternion::CreateFromYawPitchRoll(RAD(0.014f), RAD(29.762f), 0.0f);
	camera->SetLookAt(eye, eye + Vector3::Transform(Vector3::UnitZ, rotation), Vector3::Up);
	camera->SetPerspectiveFov(RAD(75.0f), Game::Graphics::ScreenWidth / Game::Graphics::ScreenHeight, 0.1f, 1000.0f);
}

bool StageTitle::ShouldLoadStageProp(const std::string& path) const
{
	const std::string name = std::filesystem::path(path).stem().string();
	return name == "Crystal" || name == "RigidRock" || name.rfind("Rock", 0) == 0;
}
