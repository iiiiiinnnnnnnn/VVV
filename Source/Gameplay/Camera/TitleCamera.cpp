#include "Gameplay/Camera/TitleCamera.h"

#include <algorithm>
#include <cmath>
#include "Application/Time/GameTime.h"
#include "Core/Object/Object.h"
#include "Gameplay/Camera/Camera.h"
#include "Rendering/Core/Graphics.h"

TitleCamera::TitleCamera(Object* owner, const Vector3& position)
	: Component(owner), position(position)
{
	UpdateView();
}

void TitleCamera::OnUpdate()
{
	SyncOffsets();
	if (motionEnabled)
	{
		elapsedTime.Advance(std::max(Game::Time::unscaledDeltaTime, 0.0f));
		UpdateView();
	}
}

void TitleCamera::OnDrawGUI()
{
	SyncOffsets();
	bool changed = ImGui::DragFloat3("Eye Offset", &eyeOffset.x, 0.01f);
	changed |= ImGui::DragFloat3("Focus Offset", &focusOffset.x, 0.01f);
	changed |= ImGui::DragFloat("Field of View", &fieldOfView, 0.1f, 1.0f, 179.0f);
	changed |= ImGui::Checkbox("Drift", &driftEnabled);
	if (changed) UpdateView();
}

void TitleCamera::UpdateView()
{
	auto* camera = owner->GetComponent<Camera>();
	if (!camera) return;

	const Vector3 drift(std::sin(elapsedTime.GetElapsed() * 0.23f) * 0.18f,
		std::sin(elapsedTime.GetElapsed() * 0.31f) * 0.06f, 0);
	const Vector3 up = camera->GetUp();
	appliedDrift = driftEnabled ? drift : Vector3::Zero;
	camera->SetLookAt(position + eyeOffset + appliedDrift,
		position + focusOffset, up);
	const float width = std::max(Game::Graphics::ScreenWidth, 1.0f);
	const float height = std::max(Game::Graphics::ScreenHeight, 1.0f);
	camera->SetPerspectiveFov(RAD(fieldOfView), width / height, 0.1f, 1000.0f);
}

void TitleCamera::SyncOffsets()
{
	auto* camera = owner->GetComponent<Camera>();
	if (!camera) return;

	eyeOffset = camera->GetEye() - position - appliedDrift;
	focusOffset = camera->GetFocus() - position;
}
