#include "Gameplay/Component/ActorHitstopComponent.h"

#include "Core/Object/Object.h"

#include <algorithm>
#include <cmath>

void ActorHitstopComponent::Request(float duration)
{
	if (!IsActive() || model.expired() || !std::isfinite(duration) || duration <= 0.0f) return;

	remaining.Reset(std::max(remaining.GetRemaining(), duration));
}

bool ActorHitstopComponent::IsStoppingFor(const VMDLModel* target) const
{
	return IsStopping() && model.lock().get() == target;
}

void ActorHitstopComponent::OnLateUpdate()
{
	remaining.Update(Game::Time::unscaledDeltaTime);
}

bool ActorHitstopComponent::IsModelStopping(const Object* owner, const VMDLModel* model)
{
	if (!owner || !model) return false;
	for (const auto* hitstop : owner->GetComponents<ActorHitstopComponent>())
		if (hitstop->IsStoppingFor(model)) return true;
	return false;
}
