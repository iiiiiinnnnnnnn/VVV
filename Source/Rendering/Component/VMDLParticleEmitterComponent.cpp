#include "Rendering/Component/VMDLParticleEmitterComponent.h"

#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Effect/ParticleSystem.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"

VMDLParticleEmitterComponent::VMDLParticleEmitterComponent(Object* owner, VMDLModel* model,
	const VMDLModel::VmdlParticleEmitter& settings, bool initiallyEmitting)
	: Component(owner), model(model), settings(settings), emitting(initiallyEmitting),
	  burstPending(initiallyEmitting)
{
	texture = ResourceManager::Instance().LoadTexture(settings.texturePath);
	if (!texture) texture = std::make_shared<Texture>(Color(1.0f, 1.0f, 1.0f, 1.0f));
	particleSystem = std::make_unique<ParticleSystem>(Game::Graphics::Instance().GetDevice(),
		texture ? texture->GetShaderResourceView() : nullptr,
		std::max(1, settings.columns), std::max(1, settings.rows),
		std::clamp(settings.capacity, 1, 8192));
}

VMDLParticleEmitterComponent::~VMDLParticleEmitterComponent() = default;

void VMDLParticleEmitterComponent::SetEmitting(bool value)
{
	if (emitting == value) return;
	emitting = value;
	if (emitting) burstPending = true;
	else emissionAccumulator = 0.0f;
}

void VMDLParticleEmitterComponent::Burst()
{
	const int count = std::clamp(settings.burstCount, 0, settings.capacity);
	for (int i = 0; i < count; ++i) SpawnOne();
}

void VMDLParticleEmitterComponent::OnLateUpdate()
{
	if (settings.rendererType == 1)
	{
		UpdateRibbon();
		return;
	}
	if (!particleSystem) return;
	if (emitting)
	{
		if (burstPending)
		{
			Burst();
			burstPending = false;
		}
		emissionAccumulator += std::max(0.0f, settings.emissionRate) * Game::Time::deltaTime;
		const int spawnCount = std::min(static_cast<int>(emissionAccumulator), settings.capacity);
		for (int i = 0; i < spawnCount; ++i) SpawnOne();
		emissionAccumulator -= static_cast<float>(spawnCount);
	}
	particleSystem->Update();
}

void VMDLParticleEmitterComponent::UpdateRibbon()
{
	const float dt = Game::Time::deltaTime;
	const float lifetime = std::max(0.01f, settings.ribbonLifetime);
	for (auto& point : ribbonPoints)
	{
		point.age += dt;
		const float t = std::clamp(point.age / lifetime, 0.0f, 1.0f);
		point.tip = Vector3::Lerp(point.fullTip, point.root,
			t * t * std::max(0.0f, settings.ribbonTipRatio));
	}
	while (!ribbonPoints.empty() && ribbonPoints.back().age >= lifetime)
		ribbonPoints.pop_back();
	if (!emitting || !model || settings.nodeIndex < 0 ||
		settings.nodeIndex >= static_cast<int>(model->GetNodes().size())) return;

	ribbonSampleTimer += dt;
	const float interval = std::clamp(settings.ribbonSampleInterval, 0.001f, 1.0f);
	if (!ribbonPoints.empty() && ribbonSampleTimer < interval) return;
	ribbonSampleTimer = 0.0f;
	const Matrix node = model->GetNodes()[settings.nodeIndex].worldTransform;
	const Matrix rootMatrix = model->GetScaledAttachmentTransform(
		Matrix::CreateTranslation(settings.offset + settings.ribbonRootOffset) * node);
	const Matrix tipMatrix = model->GetScaledAttachmentTransform(
		Matrix::CreateTranslation(settings.offset + settings.ribbonTipOffset) * node);
	RibbonPoint point;
	point.root = rootMatrix.Translation();
	point.tip = point.fullTip = tipMatrix.Translation();
	ribbonPoints.push_front(point);
	while (ribbonPoints.size() > static_cast<size_t>(std::clamp(settings.ribbonMaxPoints, 2, 1024)))
		ribbonPoints.pop_back();
}

void VMDLParticleEmitterComponent::SpawnOne()
{
	if (!model || !particleSystem || settings.nodeIndex < 0 ||
		settings.nodeIndex >= static_cast<int>(model->GetNodes().size())) return;

	const auto randomRange = [](float a, float b) {
		return Random::Range(std::min(a, b), std::max(a, b));
	};
	const Vector3 localPosition = settings.offset + Vector3(
		randomRange(-settings.spawnExtents.x, settings.spawnExtents.x),
		randomRange(-settings.spawnExtents.y, settings.spawnExtents.y),
		randomRange(-settings.spawnExtents.z, settings.spawnExtents.z));
	const Matrix nodeWorld = model->GetNodes()[settings.nodeIndex].worldTransform;
	const Matrix spawnTransform = model->GetScaledAttachmentTransform(
		Matrix::CreateTranslation(localPosition) * nodeWorld);
	const Vector3 position = spawnTransform.Translation();

	Vector3 velocity(
		randomRange(settings.velocityMin.x, settings.velocityMax.x),
		randomRange(settings.velocityMin.y, settings.velocityMax.y),
		randomRange(settings.velocityMin.z, settings.velocityMax.z));
	Vector3 acceleration = settings.acceleration;
	if (settings.localVelocity)
	{
		velocity = Vector3::TransformNormal(velocity, nodeWorld);
		acceleration = Vector3::TransformNormal(acceleration, nodeWorld);
	}
	const float lifetime = std::max(0.01f,
		randomRange(settings.lifetimeMin, settings.lifetimeMax));
	const Vector2 size(
		std::max(0.001f, randomRange(settings.sizeMin.x, settings.sizeMax.x)),
		std::max(0.001f, randomRange(settings.sizeMin.y, settings.sizeMax.y)));
	const int frameCount = std::max(1, settings.columns * settings.rows);
	particleSystem->Set(std::clamp(settings.frame, 0, frameCount - 1), lifetime, position,
		velocity, acceleration, size, settings.animated, settings.animationSpeed, settings.color,
		std::clamp(settings.fadeInDuration, 0.0f, lifetime),
		std::clamp(settings.fadeOutDuration, 0.0f, lifetime));
}

void VMDLParticleEmitterComponent::RenderParticles(const RenderContext& rc)
{
	if (settings.rendererType == 1)
	{
		RenderRibbon(rc);
		return;
	}
	if (!particleSystem) return;
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(
		settings.additive ? BlendState::Additive : BlendState::Transparency), nullptr, 0xFFFFFFFF);
	rc.deviceContext->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.deviceContext->RSSetState(
		rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
	particleSystem->Render(rc);
}

void VMDLParticleEmitterComponent::RenderRibbon(const RenderContext& rc)
{
	if (ribbonPoints.size() < 2) return;
	const float lifetime = std::max(0.01f, settings.ribbonLifetime);
	auto* renderer = Game::Graphics::Instance().GetTrailRenderer();
	for (const auto& point : ribbonPoints)
		renderer->AddPoint(point.root, point.tip, std::clamp(point.age / lifetime, 0.0f, 1.0f));
	rc.deviceContext->OMSetBlendState(rc.renderState->GetBlendState(
		settings.additive ? BlendState::Additive : BlendState::Transparency), nullptr, 0xFFFFFFFF);
	rc.deviceContext->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestOnly), 0);
	rc.deviceContext->RSSetState(
		rc.renderState->GetRasterizerState(RasterizerState::SolidCullNone));
	renderer->Render(rc.deviceContext, rc.camera->GetView(), rc.camera->GetProjection(),
		settings.color, settings.ribbonEndColor);
}

void VMDLParticleEmitterComponent::OnDrawGUI()
{
	ImGui::Text("Emitter: %s", settings.name.c_str());
	ImGui::Text("Texture: %s", settings.texturePath.c_str());
	ImGui::Checkbox("Emitting", &emitting);
	if (ImGui::Button("Burst")) Burst();
}
