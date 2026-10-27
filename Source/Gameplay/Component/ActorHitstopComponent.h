#pragma once

#include <memory>

#include "Application/Time/Timer.h"
#include "Core/Object/Component.h"

class VMDLModel;

class ActorHitstopComponent : public Component
{
public:
	ActorHitstopComponent(Object* owner, const std::shared_ptr<VMDLModel>& model)
		: Component(owner), model(model) {}

	// 所有Actorの移動と指定モデルのアニメーションを停止する。時間は実時間の秒数。
	void Request(float duration);
	bool IsStopping() const { return IsActive() && remaining.IsRunning() && !model.expired(); }
	bool IsStoppingFor(const VMDLModel* target) const;
	static bool IsModelStopping(const Object* owner, const VMDLModel* model);
	bool RunsWhenOwnerSuspended() const override { return true; }
	void OnLateUpdate() override;
	int GetUpdateOrder() const override { return 10000; }
	void OnDisabled() override { remaining.Stop(); }
	const char* GetDebugName() const override
	{
		return ICON_FA_TIMES "ActorHitstopComponent";
	}

private:
	std::weak_ptr<VMDLModel> model;
	Game::Timer remaining{0.0f};
};
