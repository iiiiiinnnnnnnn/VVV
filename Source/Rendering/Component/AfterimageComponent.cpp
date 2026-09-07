#include "Rendering/Component/AfterimageComponent.h"

#include <algorithm>

#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/ModelRenderer.h"
#include "Resource/VMDLModel.h"

AfterimageComponent::AfterimageComponent(
	Object* owner, std::shared_ptr<VMDLModel> sourceModel)
	: Component(owner), sourceModel(std::move(sourceModel))
{
	afterimages.reserve(maxAfterimages);
}

void AfterimageComponent::Play()
{
	Play(captureDuration);
}

void AfterimageComponent::Play(float duration)
{
	if (!sourceModel || duration <= 0.0f) return;

	captureTimeRemaining = duration;
	captureTimer = 0.0f;
}

void AfterimageComponent::Clear()
{
	afterimages.clear();
	captureTimeRemaining = 0.0f;
	captureTimer = 0.0f;
}

void AfterimageComponent::OnLateUpdate()
{
	const float deltaTime = std::max(Game::Time::deltaTime, 0.0f);

	for (Afterimage& afterimage : afterimages)
		afterimage.age += deltaTime;

	afterimages.erase(
		std::remove_if(afterimages.begin(), afterimages.end(),
			[this](const Afterimage& afterimage) { return afterimage.age >= lifetime; }),
		afterimages.end());

	if (captureTimeRemaining > 0.0f)
	{
		captureTimer -= deltaTime;
		if (captureTimer <= 0.0f)
		{
			CapturePose();
			captureTimer = std::max(captureInterval, 0.001f);
		}

		captureTimeRemaining = std::max(captureTimeRemaining - deltaTime, 0.0f);
	}

	for (Afterimage& afterimage : afterimages)
		UpdateRenderParams(afterimage);
}

void AfterimageComponent::OnRender(const RenderContext& rc)
{
	if (!sourceModel || afterimages.empty()) return;

	ModelRenderer* renderer = Game::Graphics::Instance().GetModelRenderer();
	for (const Afterimage& afterimage : afterimages)
		renderer->Draw(ModelShaderId::VMat, afterimage.model, &afterimage.renderParams);
}

void AfterimageComponent::OnDrawGUI()
{
	ImGui::DragFloat("Capture duration", &captureDuration, 0.01f, 0.01f, 1.0f);
	ImGui::DragFloat("Capture interval", &captureInterval, 0.005f, 0.01f, 0.25f);
	ImGui::DragFloat("Lifetime", &lifetime, 0.01f, 0.05f, 2.0f);
	ImGui::DragFloat("Max opacity", &maxOpacity, 0.01f, 0.01f, 0.98f);
	ImGui::DragFloat("Emissive intensity", &emissiveIntensity, 0.1f, 0.0f, 20.0f);
	ImGui::DragInt("Max afterimages", &maxAfterimages, 1.0f, 1, 32);
	maxAfterimages = std::clamp(maxAfterimages, 1, 32);
	ImGui::Text("Active: %zu", afterimages.size());
	if (ImGui::Button("Preview")) Play();
	ImGui::SameLine();
	if (ImGui::Button("Clear")) Clear();
}

void AfterimageComponent::CapturePose()
{
	if (!sourceModel || sourceModel->GetNodes().empty()) return;

	Afterimage& afterimage = afterimages.emplace_back();
	afterimage.model = sourceModel->CloneRenderPose();
	afterimage.renderParams.unlit = true;

	for (const VMDLModel::Material& material : afterimage.model->GetMaterials())
	{
		VMatMaterialParams& params = afterimage.renderParams.materials[material.name];
		params.useBaseColorTexture = false;
	}

	if (static_cast<int>(afterimages.size()) > maxAfterimages)
		afterimages.erase(afterimages.begin());
}

void AfterimageComponent::UpdateRenderParams(Afterimage& afterimage) const
{
	const float normalizedAge = lifetime > 0.0f
		? std::clamp(afterimage.age / lifetime, 0.0f, 1.0f)
		: 1.0f;
	// 回避中に見やすくするため最初だけ濃く保持してから滑らかに薄くする
	const float fade = 1.0f - normalizedAge * normalizedAge;
	const float alpha = std::clamp(maxOpacity * fade, 0.0f, 0.98f);

	for (auto& [name, params] : afterimage.renderParams.materials)
	{
		params.baseColor = Color(0.08f, 0.08f, 0.08f, alpha);
		params.emissionColor = Color(1.0f, 1.0f, 1.0f, emissiveIntensity);
	}
}
