#pragma once

#include "PhysicsManager.h"

// シーン基底
class Scene
{
public:
	Scene() = default;
	virtual ~Scene() = default;

	// 更新処理
	virtual void Update(float elapsedTime) {}

	// 描画処理
	virtual void Render(float elapsedTime) {}

	// GUI描画処理
	virtual void DrawGUI() {}

	const PhysicsSceneContext& GetPhysicsSceneContext() const { return psc; }

protected:
	PhysicsSceneContext psc;
};
