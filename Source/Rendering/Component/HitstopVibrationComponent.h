#pragma once

#include "Application/Time/Timer.h"

#include <memory>

#include "Core/Object/Component.h"
#include "Rendering/Core/VMatRenderParams.h"
#include "Resource/VMDLModel.h"

class HitstopVibrationComponent : public Component
{
  public:
	HitstopVibrationComponent(Object* owner, std::shared_ptr<VMDLModel> sourceModel);

	// durationは秒、powerはワールド座標での最大振幅。実行中は残り時間に加算する
	void HitStop(float duration, float power);
	void OnUpdate() override;
	void OnDisabled() override;
	const char* GetDebugName() const override { return ICON_FA_CROSSHAIRS " HitstopVibrationComponent"; }

  private:
	void ApplyVibration(const Vector3& direction, float power);
	std::weak_ptr<VMDLModel> vmdlModel;
	Game::Timer timer{0.0f};
	float currentPower = 0.0f;
	float startPower = 0.0f;
	float targetPower = 0.0f;
	float fadeInDuration = 0.02f;
	float fadeOutDuration = 0.06f;
};
