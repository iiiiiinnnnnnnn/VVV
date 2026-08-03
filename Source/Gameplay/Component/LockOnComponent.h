// LockOnComponent.h

#pragma once

#include <algorithm>
#include <memory>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class Actor;
class Entity;
class SpriteWidget;
class VMDLModel;

class LockOnComponent : public Component
{
public:
	LockOnComponent(Object* owner);
	~LockOnComponent() override;

	void OnUpdate() override;
	void OnDisabled() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_BULLSEYE " LockOnComponent"; }

	void LockOn(Actor* actor);
	void ClearTarget();
	void PauseRotation(float duration);

	Actor* GetTarget() const { return target; }
	bool IsLockedOn() const { return target; }
	void SetAimActive(bool value) { aimActive = value; }
	void SetRotationPaused(bool value) { rotationPaused = value; }
	void SetLostRange(float value) { lostRange = std::max(value, 0.0f); }
	void SetRotationSpeed(float value) { rotationSpeed = std::max(value, 0.0f); }

private:
	int GetUpdateOrder() const override { return 200; }
	bool IsTargetValid() const;
	void EnsureIndicator();
	void ResolveTargetAnchor();
	void HideIndicator();

	Entity* ownerEntity = nullptr;
	Actor* target = nullptr;
	VMDLModel* targetModel = nullptr;
	int targetAnchorIndex = -1;
	std::shared_ptr<SpriteWidget> indicator;
	float lostRange = 30.0f;
	float rotationSpeed = 8.0f;
	float rotationPauseTimer = 0.0f;
	Vector2 indicatorSize = {96.0f, 64.0f};
	bool aimActive = false;
	bool rotationPaused = false;
};
