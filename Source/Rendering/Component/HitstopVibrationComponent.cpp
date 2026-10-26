#include "pch.h"
#include "HitstopVibrationComponent.h"
#include "Core/Object/Object.h"
#include "Rendering/Component/VMDLModelComponent.h"

#include <algorithm>
#include <cmath>

HitstopVibrationComponent::HitstopVibrationComponent(
	Object* owner, std::shared_ptr<VMDLModel> sourceModel)
	: Component(owner), vmdlModel(sourceModel)
{
}

void HitstopVibrationComponent::HitStop(float duration, float power)
{
	if (!IsActive() || !std::isfinite(duration) || duration <= 0.0f ||
		!std::isfinite(power) || power <= 0.0f || vmdlModel.expired()) return;

	startPower = currentPower;
	targetPower = timer.IsRunning() ? std::max(targetPower, power) : power;
	timer.Reset(timer.GetRemaining() + duration);
	fadeInDuration = std::min(0.02f, duration * 0.25f);
	fadeOutDuration = std::min(0.06f, duration * 0.5f);
}

void HitstopVibrationComponent::OnUpdate()
{
	if (!timer.IsRunning()) return;
	const float deltaTime = Game::Time::unscaledDeltaTime;
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
	if (vmdlModel.expired() || timer.Update(deltaTime))
	{
		OnDisabled();
		return;
	}

	const float fadeIn = std::clamp(timer.GetElapsed() / fadeInDuration, 0.0f, 1.0f);
	const float fadeOut = std::clamp(timer.GetRemaining() / fadeOutDuration, 0.0f, 1.0f);
	currentPower = std::lerp(startPower, targetPower, fadeIn);
	currentPower = std::lerp(0.0f, currentPower, fadeOut);

	// ランダム方にずらす
	const float y = Random::Range(-1.0f, 1.0f);
	const float angle = Random::Range(0.0f, DirectX::XM_2PI);
	const float radius = sqrtf(std::max(0.0f, 1.0f - y * y));
	ApplyVibration(Vector3(radius * cosf(angle), y, radius * sinf(angle)), currentPower);
}

void HitstopVibrationComponent::OnDisabled()
{
	timer.Stop();
	currentPower = startPower = targetPower = 0.0f;
	ApplyVibration(Vector3::Zero, 0.0f);
}

void HitstopVibrationComponent::ApplyVibration(const Vector3& direction, float power)
{
	const auto model = vmdlModel.lock();
	if (!owner || !model) return;
	for (auto* renderer : owner->GetComponents<VMDLModelComponent>())
	{
		if (renderer->GetModel() != model.get()) continue;
		auto& params = renderer->GetRenderParams();
		params.vibrationDirection = direction;
		params.vibrationPower = power;
	}
}
