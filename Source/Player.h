// Player.h

#pragma once

#include "Common.h"
#include "RenderContext.h"

class Player
{
public:
	Player();
	~Player() = default;
	void UpdateNetwork(float elapsedTime);
	void UpdateMove(float elapsedTime, Vector3 position);
	void Render(const RenderContext& rc, float elapsedTime);

	struct Transform
	{
		Vector3 position;
		Quaternion rotation;
		Vector3 scale;
		Vector3 forward;
	} transform;
};