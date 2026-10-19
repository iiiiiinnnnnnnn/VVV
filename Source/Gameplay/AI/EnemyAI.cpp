#include "Gameplay/AI/EnemyAI.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/Actor.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Physics/Core/PhysicsManager.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Physics/Navigation/NavMeshAgent.h"
#include "Rendering/Core/Graphics.h"

#include "imgui.h"

EnemyAI::EnemyAI(Object* owner, const std::string& path)
	: LuaScript(owner)
{
	SetFloat("SearchRange", 15.0f); // 追跡開始距離
	SetFloat("LostRange", 22.5f); // 追跡終了距離
	SetFloat("SightRayLength", 16.5f); // 視線判定の長さ
	SetFloat("SightRayHeight", 0.05f); // 視線判定の高さ
	SetFloat("SightTargetHeight", 1.0f); // 視線判定のターゲットの高さ
	SetFloat("SightHorizontalFov", 90.0f); // 視線判定の水平視野角
	SetFloat("SightVerticalFov", 60.0f); // 視線判定の垂直視野角
	SetBool("HasTarget", false); // ターゲットしているかどうか
	SetBool("IsTargetInSearchRange", false); // ターゲットが追跡開始距離内にいるかどうか
	SetBool("IsTargetInLostRange", false); // ターゲットが追跡終了距離内にいるかどうか
	SetBool("IsTargetInSightAngle", false); // ターゲットが視線判定の角度内にいるかどうか
	SetBool("IsTargetVisible", false); // ターゲットが視線判定の角度内にいて、かつ視線判定の障害物に遮られていないかどうか
	SetBool("HasDestination", false); // ナビメッシュの目的地が設定されているかどうか
	SetBool("IsTurning", false); // ナビメッシュの目的地に向かって回転中かどうか
	SetBool("IsFacingTarget", false); // ターゲットの方向を向いているかどうか
	SetFloat("TurnAngle", 0.0f); // ナビメッシュの目的地に向かって回転中の角度
	SetFloat("TurnThreshold", 15.0f); // ナビメッシュの目的地に向かって回転中の角度の閾値
	SetFloat("TargetDistance", std::numeric_limits<float>::max()); // ターゲットまでの距離
	SetVector3("TargetPosition", Vector3::Zero); // ターゲットの位置
	if (!path.empty()) Load(path);
}

void EnemyAI::OnDisabled()
{
	CallScript("Stop");
	StopMovement();
}

void EnemyAI::OnRender(const RenderContext&)
{
	if (!showDebug || !showSightDebug) return;

	const Actor* ownerActor = dynamic_cast<const Actor*>(owner);
	if (!ownerActor) return;

	// 視線判定のデバッグ描画
	const float searchRange = GetFloat("SearchRange", 20.0f);
	const float lostRange = std::max(
		GetFloat("LostRange", searchRange * 1.5f),
		searchRange);
	const Color white(1.0f, 1.0f, 1.0f, 1.0f);
	const Color lostRangeColor(1.0f, 0.65f, 0.15f, 1.0f);
	if (searchRange > 0.0f)
	{
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			ownerActor->transform.position,
			searchRange,
			white);
	}
	if (lostRange > searchRange)
	{
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			ownerActor->transform.position,
			lostRange,
			lostRangeColor);
	}

	PrimitiveRenderer* primitiveRenderer =
		Game::Graphics::Instance().GetPrimitiveRenderer();
	const float sightLength = GetFloat("SightRayLength", 30.0f);
	const float horizontalFov = std::clamp(
		GetFloat("SightHorizontalFov", 90.0f), 1.0f, 179.0f);
	const float verticalFov = std::clamp(
		GetFloat("SightVerticalFov", 60.0f), 1.0f, 179.0f);
	const float horizontalHalfAngle = RAD(horizontalFov * 0.5f);
	const float verticalHalfAngle = RAD(verticalFov * 0.5f);
	const float halfWidth = tanf(horizontalHalfAngle) * sightLength;
	const float halfHeight = tanf(verticalHalfAngle) * sightLength;
	const Matrix sightRotation = Matrix::CreateFromQuaternion(ownerActor->transform.rotation);

	Vector3 corners[4] = {
		Vector3(-halfWidth, -halfHeight, sightLength),
		Vector3(+halfWidth, -halfHeight, sightLength),
		Vector3(+halfWidth, +halfHeight, sightLength),
		Vector3(-halfWidth, +halfHeight, sightLength)
	};
	for (Vector3& corner : corners)
		corner = sightRayStart + Vector3::TransformNormal(corner, sightRotation);

	for (const Vector3& corner : corners)
		primitiveRenderer->DrawLine(sightRayStart, corner, white, white);
	for (int index = 0; index < 4; ++index)
		primitiveRenderer->DrawLine(corners[index], corners[(index + 1) % 4], white, white);

	primitiveRenderer->DrawLine(sightRayStart, sightRayEnd, white, white);
	if (sightRayHit)
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(sightRayEnd, 0.1f, white);
}

void EnemyAI::SetSearchRange(float value)
{
	const float searchRange =
		std::max(value, 0.0f);
	SetFloat("SearchRange", searchRange);
	if (GetFloat("LostRange", searchRange) < searchRange)
		SetFloat("LostRange", searchRange);
}

void EnemyAI::SetLostRange(float value)
{
	SetFloat(
		"LostRange",
		std::max(
			value,
			GetFloat("SearchRange", 20.0f)));
}

void EnemyAI::LockOn(Actor* actor)
{
	if (!actor || actor == owner) return;
	lockedTarget = actor;
	target = actor;
}

void EnemyAI::SetMovementLocked(bool value)
{
	if (value && !movementLocked) FaceTarget();
	movementLocked = value;
	if (movementLocked) StopMovement();
}

void EnemyAI::MoveToTarget(float speed)
{
	if (movementLocked)
	{
		StopMovement();
		return;
	}

	if (!target || !GetBool("IsTargetInLostRange"))
	{
		StopMovement();
		return;
	}

	if (!navMeshAgent) navMeshAgent = owner->GetComponent<NavMeshAgent>();
	if (!navMeshAgent)
	{
		FaceTarget();
		return;
	}

	navMeshAgent->SetSpeed(speed);
	navMeshAgent->MoveToTarget(target);
	FaceTarget();
}

void EnemyAI::StopMovement()
{
	if (!navMeshAgent) navMeshAgent = owner->GetComponent<NavMeshAgent>();
	if (navMeshAgent) navMeshAgent->Stop();
}

void EnemyAI::FaceTarget()
{
	if (!target || GetBool("RotationPaused")) return;

	Vector3 direction = target->transform.position - owner->GetTransform()->position;
	direction.y = 0.0f;
	if (direction.LengthSquared() <= eps) return;

	const float targetYaw = atan2f(direction.x, direction.z);
	const Quaternion targetRotation = Quaternion::CreateFromYawPitchRoll(targetYaw, 0.0f, 0.0f);
	const float deltaTime = Game::Time::deltaTime;
	const float remainingAngle = expf(-trackingTurnSpeed * deltaTime);
	const float angularSpeed = GetFloat("TurnAngularSpeed");
	const float dot = fabsf(owner->GetTransform()->rotation.Dot(targetRotation));
	const float angle = 2.0f * acosf(std::clamp(dot, 0.0f, 1.0f));
	const float turnAmount = angularSpeed > 0.0f
		? std::min(1.0f, RAD(angularSpeed) * deltaTime / std::max(angle, eps))
		: 1.0f - remainingAngle;
	const Quaternion currentRotation = owner->GetTransform()->rotation;
	const Quaternion nextRotation = Quaternion::Slerp(currentRotation, targetRotation, turnAmount);
	owner->GetTransform()->SetRotation(nextRotation);
}

void EnemyAI::UpdateSensors()
{
	Actor* ownerActor = dynamic_cast<Actor*>(owner);
	ActorManager* actorManager = ActorManager::GetActive();
	if (!navMeshAgent) navMeshAgent = owner->GetComponent<NavMeshAgent>();

	const float searchRange = GetFloat("SearchRange", 20.0f);
	const float defaultLostRange = searchRange * 1.5f;
	const float configuredLostRange = GetFloat("LostRange", defaultLostRange);
	const float lostRange = std::max(configuredLostRange, searchRange);

	// 現在の相手を維持するか、新しい相手を探す。
	const float closestDistance = UpdateTarget(ownerActor, actorManager, searchRange, lostRange);

	const bool hasTarget = target;
	const bool inSearchRange = hasTarget && closestDistance <= searchRange;
	const bool inLostRange = hasTarget && closestDistance <= lostRange;
	const Vector3 targetPosition = hasTarget ? target->transform.position : Vector3::Zero;

	SetBool("HasTarget", hasTarget);
	SetBool("IsTargetInSearchRange", inSearchRange);
	SetBool("IsTargetInLostRange", inLostRange);
	SetFloat("TargetDistance", closestDistance);
	SetVector3("TargetPosition", targetPosition);

	UpdateTurnSensors(hasTarget);
	UpdateSightRay();
	if (target && target == lockedTarget)
	{
		SetBool("IsTargetInSightAngle", true);
		SetBool("IsTargetVisible", true);
	}
}

float EnemyAI::UpdateTarget(
	Actor* ownerActor, ActorManager* actorManager, float searchRange, float lostRange)
{
	float closestDistance = std::numeric_limits<float>::max();
	if (ownerActor && actorManager)
	{
		bool targetIsValid = false;
		for (Actor* actor : actorManager->GetActors())
		{
			if (!actor || actor == ownerActor || actor->IsPendingDestroy()) continue;
			if (actor != target || !actor->IsActive()) continue;

			closestDistance = Vector3::Distance(
				actor->transform.position,
				ownerActor->transform.position);
			targetIsValid = closestDistance <= lostRange;
			break;
		}

		if (!targetIsValid)
		{
			if (lockedTarget == target)
				lockedTarget = nullptr;
			target = nullptr;
			closestDistance = std::numeric_limits<float>::max();
		}

		if (!target)
		{
			for (Actor* actor : actorManager->GetActors())
			{
				if (!actor || actor == ownerActor) continue;
				if (actor->IsPendingDestroy() || !actor->IsActive()) continue;
				if (!actor->CompareTag(targetTag)) continue;

				const Vector3 candidatePosition = actor->transform.position;
				const Vector3 ownerPosition = ownerActor->transform.position;
				const float distance = Vector3::Distance(candidatePosition, ownerPosition);
				if (distance > searchRange || distance >= closestDistance) continue;

				closestDistance = distance;
				target = actor;
			}
		}
	}
	else
	{
		target = nullptr;
		lockedTarget = nullptr;
	}

	return closestDistance;
}

void EnemyAI::UpdateTurnSensors(bool hasTarget)
{
	float turnAngle = navMeshAgent ? navMeshAgent->GetTurnAngle() : 0.0f;
	Vector3 turnDirection = Vector3::Zero;
	if (fabsf(turnAngle) <= eps && navMeshAgent && navMeshAgent->HasDestination())
		turnDirection = navMeshAgent->GetDestination() - owner->GetTransform()->position;
	if (fabsf(turnAngle) <= eps && turnDirection.LengthSquared() <= eps && target)
		turnDirection = target->transform.position - owner->GetTransform()->position;
	turnDirection.y = 0.0f;
	if (fabsf(turnAngle) <= eps && turnDirection.LengthSquared() > eps)
	{
		turnDirection.Normalize();
		const float rightDistance = turnDirection.Dot(owner->GetTransform()->right);
		const float forwardDistance = turnDirection.Dot(owner->GetTransform()->forward);
		turnAngle = atan2f(rightDistance, forwardDistance);
	}
	const float configuredThreshold = GetFloat("TurnThreshold", 15.0f);
	const float thresholdDegrees = std::max(configuredThreshold, 0.0f);
	const float turnThreshold = RAD(thresholdDegrees);
	SetBool("HasDestination", navMeshAgent && navMeshAgent->HasDestination());
	SetBool("IsTurning", fabsf(turnAngle) > turnThreshold);
	Vector3 toTarget = hasTarget && target
		? target->transform.position - owner->GetTransform()->position : Vector3::Zero;
	toTarget.y = 0.0f;
	const float targetAngle = toTarget.LengthSquared() > eps
		? atan2f(toTarget.Dot(owner->GetTransform()->right), toTarget.Dot(owner->GetTransform()->forward)) : 0.0f;
	SetFloat("TargetTurnAngle", DEG(targetAngle));
	SetBool("IsFacingTarget", hasTarget && fabsf(targetAngle) <= turnThreshold);
	SetFloat("TurnAngle", DEG(turnAngle));
}

void EnemyAI::UpdateSightRay()
{
	const Actor* ownerActor = dynamic_cast<const Actor*>(owner);
	if (!ownerActor)
	{
		SetBool("IsTargetInSightAngle", false);
		SetBool("IsTargetVisible", false);
		return;
	}

	const float rayLength = GetFloat("SightRayLength", 30.0f);
	const float rayHeight = GetFloat("SightRayHeight", 1.0f);
	const float targetHeight = GetFloat("SightTargetHeight", 1.0f);
	const float horizontalFov = std::clamp(
		GetFloat("SightHorizontalFov", 90.0f), 1.0f, 179.0f);
	const float verticalFov = std::clamp(
		GetFloat("SightVerticalFov", 60.0f), 1.0f, 179.0f);

	Vector3 forward = ownerActor->transform.forward;
	Vector3 right = ownerActor->transform.right;
	Vector3 up = Vector3::TransformNormal(
		Vector3::Up, Matrix::CreateFromQuaternion(ownerActor->transform.rotation));
	if (forward.LengthSquared() <= eps) forward = Vector3::UnitZ;
	if (right.LengthSquared() <= eps) right = Vector3::UnitX;
	if (up.LengthSquared() <= eps) up = Vector3::UnitY;
	forward.Normalize();
	right.Normalize();
	up.Normalize();

	sightRayStart = ownerActor->transform.position + up * rayHeight;
	sightRayEnd = sightRayStart + forward * rayLength;
	sightRayHit = false;
	if (!target)
	{
		SetBool("IsTargetInSightAngle", false);
		SetBool("IsTargetVisible", false);
		return;
	}

	const Vector3 targetPoint = target->transform.position + Vector3::Up * targetHeight;
	Vector3 toTarget = targetPoint - sightRayStart;
	const float targetDistance = toTarget.Length();
	if (targetDistance <= eps)
	{
		SetBool("IsTargetInSightAngle", true);
		SetBool("IsTargetVisible", true);
		sightRayEnd = targetPoint;
		return;
	}

	const float forwardDistance = toTarget.Dot(forward);
	const float rightDistance = toTarget.Dot(right);
	const float upDistance = toTarget.Dot(up);
	const float horizontalAngle = DEG(atan2f(rightDistance, forwardDistance));
	const float forwardDistanceSquared = forwardDistance * forwardDistance;
	const float rightDistanceSquared = rightDistance * rightDistance;
	const float horizontalDistance = sqrtf(forwardDistanceSquared + rightDistanceSquared);
	const float verticalAngle = DEG(atan2f(upDistance, horizontalDistance));
	const float horizontalHalfFov = horizontalFov * 0.5f;
	const float verticalHalfFov = verticalFov * 0.5f;
	const bool inSightAngle =
		forwardDistance > 0.0f &&
		targetDistance <= rayLength &&
		fabsf(horizontalAngle) <= horizontalHalfFov &&
		fabsf(verticalAngle) <= verticalHalfFov;
	SetBool("IsTargetInSightAngle", inSightAngle);
	if (!inSightAngle)
	{
		SetBool("IsTargetVisible", false);
		return;
	}

	toTarget /= targetDistance;
	sightRayEnd = targetPoint;
	PhysicsManager::PhysicsRaycastHit hit;
	if (PhysicsManager::Instance().Raycast(
		sightRayStart,
		toTarget,
		targetDistance + 0.25f,
		hit,
		Layers::Get("Enemy"),
		ownerActor))
	{
		sightRayHit = true;
		sightRayEnd = hit.position;
		SetBool("IsTargetVisible", hit.layerId == Layers::Get("Player"));
		return;
	}
	SetBool("IsTargetVisible", false);
}

void EnemyAI::OnDrawGUI()
{
	ImGui::Text("Lua state: %s", stateName.c_str());
	ImGui::Text("Target: %s", target ? target->GetName().c_str() : "None");
	ImGui::Text("Locked Target: %s", lockedTarget ? lockedTarget->GetName().c_str() : "None");
	ImGui::Text("Target Distance: %.2f", GetFloat("TargetDistance"));
	ImGui::Text(
		"In Search Range: %s",
		GetBool("IsTargetInSearchRange") ? "true" : "false");
	ImGui::Text(
		"In Lost Range: %s",
		GetBool("IsTargetInLostRange") ? "true" : "false");
	ImGui::Text("In Sight Angle: %s", GetBool("IsTargetInSightAngle") ? "true" : "false");
	ImGui::Text("Target Visible: %s", GetBool("IsTargetVisible") ? "true" : "false");
	ImGui::Text("Has Destination: %s", GetBool("HasDestination") ? "true" : "false");
	ImGui::Text("Turning: %s (%.1f deg)", GetBool("IsTurning") ? "true" : "false", GetFloat("TurnAngle"));
	ImGui::Text(
		"NavMesh: %s",
		navMeshAgent ? navMeshAgent->GetStatusMessage().c_str() : "Not cached yet");
	ImGui::Text("Move Speed: %.2f", navMeshAgent ? navMeshAgent->GetMoveAmount() : 0.0f);
	ImGui::Checkbox("Show Sight Debug", &showSightDebug);

	float searchRange = GetFloat("SearchRange", 20.0f);
	if (ImGui::DragFloat("Search Range", &searchRange, 0.25f, 0.0f, 1000.0f))
		SetSearchRange(searchRange);
	float lostRange = GetFloat(
		"LostRange",
		searchRange * 1.5f);
	if (ImGui::DragFloat(
		"Lost Range",
		&lostRange,
		0.25f,
		searchRange,
		1000.0f))
	{
		SetLostRange(lostRange);
	}
	float sightRayLength = GetFloat("SightRayLength", 30.0f);
	if (ImGui::DragFloat("Sight Ray Length", &sightRayLength, 0.25f, 0.0f, 1000.0f))
		SetFloat("SightRayLength", std::max(sightRayLength, 0.0f));
	float sightRayHeight = GetFloat("SightRayHeight", 1.0f);
	if (ImGui::DragFloat("Sight Ray Height", &sightRayHeight, 0.05f, 0.0f, 100.0f))
		SetFloat("SightRayHeight", std::max(sightRayHeight, 0.0f));
	float sightTargetHeight = GetFloat("SightTargetHeight", 1.0f);
	if (ImGui::DragFloat("Sight Target Height", &sightTargetHeight, 0.05f, 0.0f, 100.0f))
		SetFloat("SightTargetHeight", std::max(sightTargetHeight, 0.0f));
	float horizontalFov = GetFloat("SightHorizontalFov", 90.0f);
	if (ImGui::DragFloat("Sight Horizontal FOV", &horizontalFov, 0.5f, 1.0f, 179.0f))
		SetFloat("SightHorizontalFov", std::clamp(horizontalFov, 1.0f, 179.0f));
	float verticalFov = GetFloat("SightVerticalFov", 60.0f);
	if (ImGui::DragFloat("Sight Vertical FOV", &verticalFov, 0.5f, 1.0f, 179.0f))
		SetFloat("SightVerticalFov", std::clamp(verticalFov, 1.0f, 179.0f));
	ImGui::DragFloat("Tracking Turn Speed", &trackingTurnSpeed, 0.1f, 0.0f, 30.0f);
	float turnThreshold = GetFloat("TurnThreshold", 15.0f);
	if (ImGui::DragFloat("Turn Threshold", &turnThreshold, 0.5f, 0.0f, 180.0f))
		SetFloat("TurnThreshold", std::max(turnThreshold, 0.0f));
}


void EnemyAI::AddAction(const std::string& name, Action enter, Action update, Action late, Action exit)
{
	actions[name] = { std::move(enter), std::move(update), std::move(late), std::move(exit) };
}

bool EnemyAI::GetBool(const std::string& name, bool fallback) const
{
	auto found = bools.find(name);
	return found != bools.end() ? found->second : fallback;
}

float EnemyAI::GetFloat(const std::string& name, float fallback) const
{
	auto found = floats.find(name);
	return found != floats.end() ? found->second : fallback;
}

void EnemyAI::BindEnvironment(sol::environment& environment)
{
	environment["GetBool"] = [this](const std::string& name) { return GetBool(name); };
	environment["GetFloat"] = [this](const std::string& name) { return GetFloat(name); };
	environment["SetBool"] = [this](const std::string& name, bool value) { SetBool(name, value); };
	environment["SetFloat"] = [this](const std::string& name, float value) { SetFloat(name, value); };
	environment["SetState"] = [this](const std::string& name) { stateName = name; };
	environment["Action"] = [this](const std::string& name, int phase)
	{
		auto found = actions.find(name);
		if (found == actions.end() || phase < 1 || phase > 4) return;
		const auto& action = found->second[phase - 1];
		if (action) action();
	};
	environment["GetVector3"] = [this, environmentState = environment.lua_state()](const std::string& name)
	{
		auto found = vectors.find(name);
		const Vector3 value = found != vectors.end() ? found->second : Vector3::Zero;
		sol::state_view state(environmentState);
		return state.create_table_with("x", value.x, "y", value.y, "z", value.z);
	};
	environment["GetTargetsInSearchRange"] = [this, environmentState = environment.lua_state()]()
	{
		sol::state_view state(environmentState);
		sol::table result = state.create_table();
		ActorManager* manager = ActorManager::GetActive();
		if (!manager) return result;
		int index = 1;
		for (Actor* actor : manager->GetActors())
		{
			if (!actor || actor == owner || actor->IsPendingDestroy() || !actor->IsActive()) continue;
			if (!actor->CompareTag(targetTag)) continue;
			const Vector3 position = actor->transform.position;
			const float distance = Vector3::Distance(position, owner->GetTransform()->position);
			if (distance > GetFloat("SearchRange")) continue;
			result[index++] = state.create_table_with("name", actor->GetName(), "distance", distance,
				"position", state.create_table_with("x", position.x, "y", position.y, "z", position.z));
		}
		return result;
	};
	environment["SetTrackingTurnSpeed"] = [this](float speed) { SetTrackingTurnSpeed(speed); };
	environment["MoveToTarget"] = [this](float speed) { MoveToTarget(speed); };
	environment["StopMovement"] = [this]() { StopMovement(); };
	environment["FaceTarget"] = [this]() { FaceTarget(); };
}

void EnemyAI::OnEnabled()
{
	CallScript("Start");
}

void EnemyAI::OnLateUpdate()
{
	CallScript("LateUpdate");
}
