#pragma once

#include <memory>
#include <vector>

#include "Core/Object/Component.h"
#include "Rendering/Core/VMatRenderParams.h"

class VMDLModel;

// モデルのポーズを取得して残像を描画する
// 各残像は描画リソースとポーズデータだけ保持
class AfterimageComponent : public Component
{
public:
	AfterimageComponent(Object* owner, std::shared_ptr<VMDLModel> sourceModel);

	const char* GetDebugName() const override { return ICON_FA_IMAGES " AfterimageComponent"; }
	void OnLateUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

	void Play();
	void Play(float duration);
	void Clear();

private:
	struct Afterimage
	{
		std::shared_ptr<VMDLModel> model;
		VMatRenderParams renderParams;
		float age = 0.0f;
	};

	void CapturePose();
	void UpdateRenderParams(Afterimage& afterimage) const;

	std::shared_ptr<VMDLModel> sourceModel;
	std::vector<Afterimage> afterimages;

	float captureTimeRemaining = 0.0f;
	float captureTimer = 0.0f;

	float captureDuration = 0.22f;
	float captureInterval = 0.045f;
	float lifetime = 0.30f;
	float maxOpacity = 0.28f;
	float emissiveIntensity = 4.0f;
	int maxAfterimages = 7;
};
