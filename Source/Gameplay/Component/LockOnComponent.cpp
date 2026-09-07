#include "Gameplay/Component/LockOnComponent.h"

#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Entity.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Scene/Scene.h"
#include "Gameplay/Scene/SceneManager.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/VMDLModel.h"
#include "UI/SpriteWidget.h"

LockOnComponent::LockOnComponent(Object* owner)
	: Component(owner), ownerEntity(dynamic_cast<Entity*>(owner))
{
}

LockOnComponent::~LockOnComponent()
{
	if (indicator) indicator->Destroy();
}

void LockOnComponent::OnUpdate()
{
	rotationPauseTimer = std::max(
		rotationPauseTimer - Game::Time::deltaTime,
		0.0f);

	if (!target) return;
	if (!IsTargetValid())
	{
		ClearTarget();
		return;
	}

	Transform* ownerTransform = owner->GetTransform();
	if (!ownerTransform) return;

	Vector3 direction = target->transform.position - ownerTransform->position;
	direction.y = 0.0f;
	if (direction.LengthSquared() > lostRange * lostRange)
	{
		ClearTarget();
		return;
	}
	if (!aimActive || ownerEntity->IsDead() || rotationPaused || rotationPauseTimer > 0.0f) return;
	if (direction.LengthSquared() <= eps) return;
	direction.Normalize();

	Vector3 forward = ownerTransform->forward;
	forward.y = 0.0f;
	if (forward.LengthSquared() <= eps) forward = Vector3::UnitZ;
	forward.Normalize();

	const float targetYaw = atan2f(direction.x, direction.z);
	const Quaternion targetRotation =
		Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
	const float rate =
		1.0f - expf(-rotationSpeed * Game::Time::deltaTime);
	ownerTransform->SetRotation(
		Quaternion::Slerp(ownerTransform->rotation, targetRotation, rate));
}

void LockOnComponent::OnDisabled()
{
	HideIndicator();
}

void LockOnComponent::OnRender(const RenderContext& rc)
{
	EnsureIndicator();
	if (!indicator || !target || !rc.camera)
	{
		HideIndicator();
		return;
	}

	if (!targetModel || targetAnchorIndex < 0) ResolveTargetAnchor();
	if (!targetModel || targetAnchorIndex < 0)
	{
		HideIndicator();
		return;
	}

	const auto& colliders = targetModel->GetVmdlExtensionData().colliders;
	const auto& nodes = targetModel->GetNodes();
	if (targetAnchorIndex >= static_cast<int>(colliders.size()))
	{
		HideIndicator();
		return;
	}
	const VMDLModel::VmdlCollider& anchor = colliders[targetAnchorIndex];
	if (anchor.nodeIndex < 0 || anchor.nodeIndex >= static_cast<int>(nodes.size()))
	{
		HideIndicator();
		return;
	}

	const Matrix offset =
		Matrix::CreateFromYawPitchRoll(
			RAD(anchor.rotation.y),
			RAD(anchor.rotation.x),
			RAD(anchor.rotation.z)) *
		Matrix::CreateTranslation(anchor.center);
	const Matrix anchorWorld = targetModel->GetScaledAttachmentTransform(
		offset * nodes[anchor.nodeIndex].worldTransform);
	const Vector3 worldPosition = anchorWorld.Translation();
	const Matrix viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	const Vector4 clipPosition = Vector4::Transform(
		Vector4(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f),
		viewProjection);
	if (clipPosition.w <= eps)
	{
		HideIndicator();
		return;
	}

	const float inverseW = 1.0f / clipPosition.w;
	const float ndcX = clipPosition.x * inverseW;
	const float ndcY = clipPosition.y * inverseW;
	const float ndcZ = clipPosition.z * inverseW;
	if (ndcX < -1.0f || ndcX > 1.0f ||
		ndcY < -1.0f || ndcY > 1.0f ||
		ndcZ < 0.0f || ndcZ > 1.0f)
	{
		HideIndicator();
		return;
	}

	indicator->rect.position = {
		(ndcX + 1.0f) * 0.5f * Game::Graphics::ScreenWidth,
		(1.0f - ndcY) * 0.5f * Game::Graphics::ScreenHeight};
	indicator->SetActive(true);
}

void LockOnComponent::OnDrawGUI()
{
	ImGui::Text("Target: %s", target ? target->GetName().c_str() : "None");
	ImGui::Text("ArrowUI: %s", targetAnchorIndex >= 0 ? "Found" : "Not Found");
	ImGui::DragFloat("Acquire Range", &acquireRange, 0.1f, 0.0f, 1000.0f);
	ImGui::DragFloat("Lost Range", &lostRange, 0.1f, 0.0f, 1000.0f);
	ImGui::DragFloat("Rotation Speed", &rotationSpeed, 0.1f, 0.0f, 30.0f);
	ImGui::Text("Aim Active: %s", aimActive ? "true" : "false");
	ImGui::Text("Rotation Paused: %s", rotationPaused ? "true" : "false");
	ImGui::Text("Rotation Pause: %.2f", rotationPauseTimer);
	if (target && ImGui::Button("Clear Target")) ClearTarget();
}

void LockOnComponent::LockOn(Actor* actor)
{
	if (!actor || actor == owner || actor->IsPendingDestroy()) return;
	target = actor;
	ResolveTargetAnchor();
}

// 攻撃時に取得範囲内で最も近い生存中のEnemyをロックする
bool LockOnComponent::LockOnNearestEnemy()
{
	ActorManager* actorManager = ActorManager::GetActive();
	Transform* ownerTransform = owner ? owner->GetTransform() : nullptr;
	if (!actorManager || !ownerTransform) return false;

	Actor* nearest = nullptr;
	float nearestDistanceSquared = acquireRange * acquireRange;
	for (Actor* actor : actorManager->GetActorsByTag("Enemy"))
	{
		if (!actor || actor == owner || !actor->IsActive() || actor->IsPendingDestroy()) continue;
		const Entity* entity = dynamic_cast<const Entity*>(actor);
		if (entity && entity->IsDead()) continue;

		Vector3 difference = actor->transform.position - ownerTransform->position;
		difference.y = 0.0f;
		const float distanceSquared = difference.LengthSquared();
		if (distanceSquared > nearestDistanceSquared) continue;
		nearestDistanceSquared = distanceSquared;
		nearest = actor;
	}

	if (!nearest) return false;
	LockOn(nearest);
	return true;
}

// 対象から明確に離れる移動入力が入ったら、攻撃の吸い付きを解除する
void LockOnComponent::ReleaseIfMovingAway(const Vector3& worldMoveDirection)
{
	if (!target) return;

	Vector3 moveDirection = worldMoveDirection;
	moveDirection.y = 0.0f;
	if (moveDirection.LengthSquared() <= eps) return;
	moveDirection.Normalize();

	Transform* ownerTransform = owner ? owner->GetTransform() : nullptr;
	if (!ownerTransform) return;
	Vector3 directionToTarget = target->transform.position - ownerTransform->position;
	directionToTarget.y = 0.0f;
	if (directionToTarget.LengthSquared() <= eps) return;
	directionToTarget.Normalize();

	if (moveDirection.Dot(directionToTarget) < releaseMoveDot) ClearTarget();
}

void LockOnComponent::ClearTarget()
{
	target = nullptr;
	targetModel = nullptr;
	targetAnchorIndex = -1;
	HideIndicator();
}

void LockOnComponent::PauseRotation(float duration)
{
	rotationPauseTimer = std::max(rotationPauseTimer, duration);
}

bool LockOnComponent::IsTargetValid() const
{
	if (!ownerEntity || !target || !target->IsActive() || target->IsPendingDestroy()) return false;

	const Entity* targetEntity = dynamic_cast<const Entity*>(target);
	return !targetEntity || !targetEntity->IsDead();
}

void LockOnComponent::EnsureIndicator()
{
	if (indicator) return;

	Scene* scene = SceneManager::Instance().GetCurrentScene();
	if (!scene) return;

	indicator = std::make_shared<SpriteWidget>("Resources/UI/arrow.png");
	indicator->rect.anchor = {0.5f, 0.5f};
	indicator->rect.size = Vector2(583.0f, 392.0f) * 0.1f;
	indicator->rect.angle = -90.0f;
	indicator->SetAffectedByPostProcess(false);
	indicator->SetActive(false);
	scene->RegisterWidget(indicator);
}

void LockOnComponent::ResolveTargetAnchor()
{
	targetModel = nullptr;
	targetAnchorIndex = -1;
	if (!target) return;

	VMDLModelComponent* modelComponent = target->GetComponent<VMDLModelComponent>();
	if (!modelComponent) return;
	targetModel = modelComponent->GetModel();
	if (!targetModel) return;

	const auto& colliders = targetModel->GetVmdlExtensionData().colliders;
	for (int i = 0; i < static_cast<int>(colliders.size()); ++i)
	{
		if (colliders[i].name != "ARROWUI") continue;
		targetAnchorIndex = i;
		return;
	}
}

void LockOnComponent::HideIndicator()
{
	if (indicator) indicator->SetActive(false);
}
