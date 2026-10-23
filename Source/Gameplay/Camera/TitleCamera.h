#pragma once

#include "Application/Time/Timer.h"

#include "Core/Object/Component.h"
#include "Core/Foundation/Common.h"

class TitleCamera final : public Component
{
public:
	TitleCamera(Object* owner, const Vector3& position);
	const char* GetDebugName() const override { return ICON_FA_VIDEO " TitleCamera"; }
	void SetMotionEnabled(bool enabled) { motionEnabled = enabled; }
	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	void UpdateView();
	void SyncOffsets();
	Vector3 appliedDrift = Vector3::Zero;
	Vector3 position;
	Vector3 eyeOffset = {3.15f, 0.68f, 5.0f};
	Vector3 focusOffset = {-0.04f, 1.22f, -0.15f};
	float fieldOfView = 42.34f;
	bool driftEnabled = true;
	Game::Timer elapsedTime{0.0f};
	bool motionEnabled = true;
};
