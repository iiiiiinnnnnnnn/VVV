// FootIK.cpp

#include "Animation/FootIK.h"
#include "Gameplay/Actor/Actor.h"
#include "Rendering/Core/Graphics.h"
#include "Application/Time/GameTime.h"
#include "Physics/Core/PhysicsManager.h"

FootIK::FootIK(
	Object* owner,
	LayerId layerId,
	VMDLModel* model,
	const char* thighName,
	const char* calfName,
	const char* footName,
	const char* ballName)
	: PhysicsComponent(owner, layerId), model(model)
{
	chain.root = &model->GetNodes().at(model->GetNodeIndex(thighName));
	chain.mid = &model->GetNodes().at(model->GetNodeIndex(calfName));
	chain.tip = &model->GetNodes().at(model->GetNodeIndex(footName));

	// 接地位置にはつま先を使う。指定がなければ足首を接地点として扱う。
	if (ballName)
	{
		chain.contact = &model->GetNodes().at(model->GetNodeIndex(ballName));
	}
	else
	{
		chain.contact = chain.tip;
	}

	if (!IsDescendantOf(chain.root, chain.mid) ||
		!IsDescendantOf(chain.mid, chain.tip))
	{
		chain.enabled = false;
	}
}

bool FootIK::UpdateGroundTarget(
	float rayUp,
	float rayDown,
	float contactOffset)
{
	if (!chain.enabled)
	{
		return false;
	}

	if (chain.root == nullptr) return false;
	if (chain.mid == nullptr) return false;
	if (chain.tip == nullptr) return false;

	InitializeFromCurrentPose(0.5f);
	SyncPoleWorldPosition();

	Vector3 currentContactPosition = GetContactWorldPosition();

	rayStart = currentContactPosition + Vector3(0, rayUp, 0);
	rayEnd = currentContactPosition - Vector3(0, rayDown, 0);

	Vector3 direction = rayEnd - rayStart;
	float distance = direction.Length();

	if (distance < 0.001f)
	{
		return false;
	}

	direction.Normalize();

	PhysicsManager::PhysicsRaycastHit hit;
	PhysicsManager::PhysicsRaycastHit rawHit;
	lastHitLayerId = InvalidLayerId;
	lastRawHitLayerId = InvalidLayerId;
	hasRawGroundHit = false;
	lastHitNormalY = 0.0f;

	// IK対象レイヤーだけを先に調べる。失敗時の全レイヤーRaycastはデバッグ情報の記録にだけ使う。
	if (!PhysicsManager::Instance().Raycast(
		rayStart,
		direction,
		distance,
		hit,
		layerId,
		dynamic_cast<Actor*>(owner)))
	{
		hasRawGroundHit = PhysicsManager::Instance().Raycast(
			rayStart,
			direction,
			distance,
			rawHit,
			-1,
			dynamic_cast<Actor*>(owner));
		lastRawHitLayerId = hasRawGroundHit ? rawHit.layerId : InvalidLayerId;
		KeepPreviousGroundTarget();
		return false;
	}

	lastHitLayerId = hit.layerId;
	lastHitNormalY = hit.normal.y;

	if (hit.normal.y < 0.35f)
	{
		KeepPreviousGroundTarget();
		return false;
	}

	Vector3 targetContactPosition =
		hit.position + hit.normal * contactOffset;
	float targetGroundOffsetY =
		targetContactPosition.y - currentContactPosition.y;
	float targetWeight = 1.0f;

	// 下方向は脚が伸び切らない範囲へ制限し、制限量に応じてIKウェイトも下げる。
	// 上方向も同様に補正量を抑え、急な段差で足が跳ね上がるのを防ぐ。
	if (targetGroundOffsetY < -0.001f)
	{
		if (liftOnly || downwardWeight <= 0.001f)
		{
			ResetGroundState();
			return false;
		}

		float limitedGroundOffsetY = targetGroundOffsetY;
		if (limitedGroundOffsetY < -maxDownCorrection)
		{
			limitedGroundOffsetY = -maxDownCorrection;
		}

		targetWeight =
			(targetGroundOffsetY != 0.0f)
			? limitedGroundOffsetY / targetGroundOffsetY
			: 0.0f;
		targetWeight *= downwardWeight;
		targetWeight = std::clamp(targetWeight, 0.0f, 1.0f);
		targetGroundOffsetY *= targetWeight;
	}
	else if (targetGroundOffsetY > maxUpCorrection)
	{
		targetWeight =
			(targetGroundOffsetY != 0.0f)
			? maxUpCorrection / targetGroundOffsetY
			: 0.0f;
		targetWeight = std::clamp(targetWeight, 0.0f, 1.0f);
		targetGroundOffsetY *= targetWeight;
	}

	SetTargetFromContact(
		hit.position,
		hit.normal,
		contactOffset
	);

	if (targetWeight < 0.999f)
	{
		const Vector3 currentTipPosition = GetScaledNodeWorldPosition(*chain.tip);
		chain.targetPosition = Vector3::Lerp(currentTipPosition, chain.targetPosition, targetWeight);
	}

	SetSmoothedTarget(
		chain.targetPosition,
		targetGroundOffsetY);

	hasGroundContact = true;
	lostGroundFrameCount = 0;
	return true;
}

void FootIK::Render(const RenderContext& rc)
{
	if (!showDebug) return;

	if (!chain.enabled) return;
	if (chain.root == nullptr) return;
	if (chain.mid == nullptr) return;
	if (chain.tip == nullptr) return;

	{
		Vector3 targetPosition = chain.targetPosition;
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			targetPosition,
			0.05f,
			{1, 1, 0, 1});
	}
	{
		Vector3 polePosition = chain.polePosition;
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			polePosition,
			0.05f,
			{0, 1, 1, 1});
	}
	{
		Game::Graphics::Instance().GetPrimitiveRenderer()->DrawLine(
			rayStart,
			rayEnd,
			{1, 0, 0, 1}, {1, 0, 0, 1});
	}
}

void FootIK::DrawGUI()
{
	if (ImGui::DragFloat3("Pole Position", &chain.polePosition.x, 0.01f))
	{
		SyncPoleLocalPosition();
	}
}

void FootIK::InitializeFromCurrentPose(float poleDistance)
{
	if (chain.poleInitialized) return;
	if (chain.root == nullptr) return;
	if (chain.mid == nullptr) return;
	if (chain.tip == nullptr) return;

	Matrix rootWorldTransform = GetScaledNodeWorldTransform(*chain.root);
	Matrix midWorldTransform = GetScaledNodeWorldTransform(*chain.mid);
	Matrix tipWorldTransform = GetScaledNodeWorldTransform(*chain.tip);

	Vector3 rootPosition = rootWorldTransform.Translation();
	Vector3 midPosition  = midWorldTransform.Translation();
	Vector3 tipPosition  = tipWorldTransform.Translation();

	Vector3 rootToTip = tipPosition - rootPosition;
	Vector3 rootToMid = midPosition - rootPosition;

	Vector3 poleDirection = Vector3::Zero;

	if (rootToTip.Length() > 0.001f)
	{
		Vector3 rootToTipDirection = rootToTip;
		rootToTipDirection.Normalize();

		Vector3 projectedMidPosition =
			rootPosition +
			rootToTipDirection * rootToMid.Dot(rootToTipDirection);

		poleDirection = midPosition - projectedMidPosition;
	}

	if (poleDirection.Length() < 0.001f)
	{
		poleDirection = Vector3(0, 0, 1);
	}

	poleDirection.Normalize();

	const Vector3 scaledPoleOffset = model
		? model->GetScaledAttachmentVector(Vector3(poleDistance, poleLiftY, 0.0f))
		: Vector3(poleDistance, poleLiftY, 0.0f);
	chain.polePosition = midPosition + poleDirection * scaledPoleOffset.x + Vector3::Up * scaledPoleOffset.y;
	chain.targetPosition = tipPosition;
	chain.poleInitialized = true;
	SyncPoleLocalPosition();
}

void FootIK::SetTarget(const Vector3& targetPosition)
{
	chain.targetPosition = targetPosition;
}

void FootIK::SetTargetFromContact(
	const Vector3& contactPosition,
	const Vector3& contactNormal,
	float contactOffset)
{
	if (chain.tip == nullptr) return;

	VMDLModel::Node* contactNode =
		chain.contact != nullptr
		? chain.contact
		: chain.tip;

	Matrix tipWorldTransform = GetScaledNodeWorldTransform(*chain.tip);
	Matrix contactWorldTransform = GetScaledNodeWorldTransform(*contactNode);

	Vector3 tipPosition = tipWorldTransform.Translation();
	Vector3 currentContactPosition = contactWorldTransform.Translation();

	Vector3 contactToTipOffset = tipPosition - currentContactPosition;

	chain.targetPosition =
		contactPosition +
		contactNormal * contactOffset +
		contactToTipOffset;
}

void FootIK::SetPoleWorldPosition(const Vector3& poleWorldPosition)
{
	chain.polePosition = poleWorldPosition;
	chain.poleInitialized = true;
	SyncPoleLocalPosition();
}

void FootIK::SetIKEnabled(bool enabled)
{
	if (chain.enabled == enabled) return;

	chain.enabled = enabled;
	if (!chain.enabled) ResetGroundState();
}

void FootIK::ResetGroundState()
{
	hasGroundContact = false;
	groundOffsetY = 0.0f;
	smoothedGroundOffsetY = 0.0f;
	hasSmoothedTarget = false;
	lostGroundFrameCount = 0;

	if (chain.tip)
	{
		chain.targetPosition = GetScaledNodeWorldPosition(*chain.tip);
		smoothedTargetPosition = chain.targetPosition;
	}
}

void FootIK::KeepPreviousGroundTarget()
{
	lostGroundFrameCount++;
	if (lostGroundFrameCount <= maxLostGroundFrames && hasSmoothedTarget)
	{
		chain.targetPosition = smoothedTargetPosition;
		groundOffsetY = smoothedGroundOffsetY;
		hasGroundContact = true;
		return;
	}

	hasGroundContact = false;
	groundOffsetY = 0.0f;
	chain.targetPosition = GetContactWorldPosition();
	smoothedTargetPosition = chain.targetPosition;
	hasSmoothedTarget = false;
}

void FootIK::SetSmoothedTarget(const Vector3& targetPosition, float targetGroundOffsetY)
{
	if (!hasSmoothedTarget || Vector3::Distance(smoothedTargetPosition, targetPosition) > 0.75f)
	{
		smoothedTargetPosition = targetPosition;
		smoothedGroundOffsetY = targetGroundOffsetY;
		hasSmoothedTarget = true;
	}
	else
	{
		float targetRate = 1.0f - expf(-targetSmoothSpeed * Game::Time::deltaTime);
		float offsetRate = 1.0f - expf(-groundOffsetSmoothSpeed * Game::Time::deltaTime);

		targetRate = std::clamp(targetRate, 0.0f, 1.0f);
		offsetRate = std::clamp(offsetRate, 0.0f, 1.0f);

		smoothedTargetPosition = Vector3::Lerp(smoothedTargetPosition, targetPosition, targetRate);
		smoothedGroundOffsetY += (targetGroundOffsetY - smoothedGroundOffsetY) * offsetRate;
	}

	chain.targetPosition = smoothedTargetPosition;
	groundOffsetY = smoothedGroundOffsetY;
}

void FootIK::SyncPoleWorldPosition()
{
	if (!model) return;
	if (!chain.poleInitialized) return;

	chain.polePosition = Vector3::Transform(
		chain.poleLocalPosition,
		GetScaledModelOwnerWorldTransform());
}

void FootIK::SyncPoleLocalPosition()
{
	if (!model) return;

	Matrix inverseModelWorldTransform = GetScaledModelOwnerWorldTransform().Invert();
	chain.poleLocalPosition = Vector3::Transform(
		chain.polePosition,
		inverseModelWorldTransform);
}


Matrix FootIK::GetModelOwnerWorldTransform() const
{
	if (!model) return Matrix::Identity;

	for (const VMDLModel::Node& node : model->GetNodes())
	{
		if (node.parent) continue;

		Matrix rootGlobalTransform = node.globalTransform;
		Matrix rootWorldTransform = node.worldTransform;
		return rootGlobalTransform.Invert() * rootWorldTransform;
	}

	return Matrix::Identity;
}

Matrix FootIK::GetScaledModelOwnerWorldTransform() const
{
	const Matrix ownerWorldTransform = GetModelOwnerWorldTransform();
	return model ? model->GetScaledAttachmentTransform(ownerWorldTransform) : ownerWorldTransform;
}

Matrix FootIK::GetScaledNodeWorldTransform(const VMDLModel::Node& node) const
{
	const Matrix nodeWorldTransform = node.worldTransform;
	return model ? model->GetScaledAttachmentTransform(nodeWorldTransform) : nodeWorldTransform;
}

Vector3 FootIK::GetScaledNodeWorldPosition(const VMDLModel::Node& node) const
{
	return GetScaledNodeWorldTransform(node).Translation();
}

Vector3 FootIK::GetPoleWorldPosition() const
{
	return chain.polePosition;
}

Vector3 FootIK::GetTargetPosition() const
{
	return chain.targetPosition;
}

Vector3 FootIK::GetContactWorldPosition() const
{
	if (chain.contact != nullptr)
	{
		return GetScaledNodeWorldPosition(*chain.contact);
	}

	if (chain.tip != nullptr)
	{
		return GetScaledNodeWorldPosition(*chain.tip);
	}

	return Vector3::Zero;
}

void FootIK::SolveIK(const DirectX::XMFLOAT4X4& modelWorldTransform)
{
	if (!chain.enabled) return;

	const Matrix ownerWorldTransform = GetModelOwnerWorldTransform();

	float targetWeight = hasGroundContact ? ikWeight : 0.0f;
	float blendRate = 1.0f - expf(-ikBlendSpeed * Game::Time::deltaTime);
	blendRate = std::clamp(blendRate, 0.0f, 1.0f);
	chain.weight += (targetWeight - chain.weight) * blendRate;
	if (chain.weight <= 0.001f) return;
	if (chain.root == nullptr) return;
	if (chain.mid == nullptr) return;
	if (chain.tip == nullptr) return;
	if (!chain.poleInitialized) return;

	VMDLModel::Node& rootBone = *chain.root;
	VMDLModel::Node& midBone = *chain.mid;
	VMDLModel::Node& tipBone = *chain.tip;

	Quaternion originalRootRotation = rootBone.rotation;
	Quaternion originalMidRotation = midBone.rotation;
	auto restoreOriginalPose = [&]()
	{
		rootBone.rotation = originalRootRotation;
		midBone.rotation = originalMidRotation;
		UpdateWorldTransforms(rootBone, ownerWorldTransform);
	};

	rootBone.rotation *= rootRotationOffset;
	rootBone.rotation.Normalize();
	UpdateWorldTransforms(rootBone, ownerWorldTransform);

	Matrix RootWorldTransform = GetScaledNodeWorldTransform(rootBone);
	Matrix MidWorldTransform = GetScaledNodeWorldTransform(midBone);
	Matrix TipWorldTransform = GetScaledNodeWorldTransform(tipBone);

	Vector3 RootWorldPosition = RootWorldTransform.Translation();
	Vector3 MidWorldPosition = MidWorldTransform.Translation();
	Vector3 TipWorldPosition = TipWorldTransform.Translation();

	Vector3 TargetWorldPosition = chain.targetPosition;
	Vector3 PoleWorldPosition = chain.polePosition;

	Vector3 RootMidVec = MidWorldPosition - RootWorldPosition;
	Vector3 RootTargetVec = TargetWorldPosition - RootWorldPosition;
	Vector3 MidTipVec = TipWorldPosition - MidWorldPosition;

	float rootTargetLength = RootTargetVec.Length();
	float rootMidLength = RootMidVec.Length();
	float midTipLength = MidTipVec.Length();

	if (rootTargetLength < 0.001f)
	{
		restoreOriginalPose();
		return;
	}
	if (rootMidLength < 0.001f)
	{
		restoreOriginalPose();
		return;
	}
	if (midTipLength < 0.001f)
	{
		restoreOriginalPose();
		return;
	}

	Vector3 RootTargetDirection = RootTargetVec;
	RootTargetDirection.Normalize();

	Vector3 RootMidDirection = RootMidVec;
	RootMidDirection.Normalize();

	RotateBone(rootBone, RootMidDirection, RootTargetDirection);

	if (rootTargetLength < rootMidLength + midTipLength)
	{
		float s = (rootMidLength + midTipLength + rootTargetLength) * 0.5f;
		float areaValue = s * (s - rootMidLength) * (s - midTipLength) * (s - rootTargetLength);

		if (areaValue > 0.0f)
		{
			float S = std::sqrt(areaValue);

			float bottom = rootMidLength;
			float height = S * 2.0f / bottom;

			float sinAngle = height / rootTargetLength;
			sinAngle = std::clamp(sinAngle, -1.0f, 1.0f);

			float angle = std::asin(sinAngle);

			if (angle > FLT_EPSILON)
			{
				Vector3 RootPoleVec = PoleWorldPosition - RootWorldPosition;

				if (RootPoleVec.Length() > 0.001f)
				{
					Vector3 RootPoleDirection = RootPoleVec;
					RootPoleDirection.Normalize();

					RootPoleDirection =
						RootPoleDirection -
						RootTargetDirection * RootPoleDirection.Dot(RootTargetDirection);

					if (RootPoleDirection.Length() > 0.001f)
					{
						RootPoleDirection.Normalize();

						Vector3 axis = RootTargetDirection.Cross(RootPoleDirection);

						if (axis.Length() > 0.001f)
						{
							axis.Normalize();

							if (rootBone.parent != nullptr)
							{
								Matrix parentWorldTransform = rootBone.parent->worldTransform;
								Matrix inverseParentWorldTransform = parentWorldTransform.Invert();

								axis = Vector3::TransformNormal(axis, inverseParentWorldTransform);
								axis.Normalize();
							}

							Quaternion quat = Quaternion::CreateFromAxisAngle(axis, angle);

							rootBone.rotation *= quat;
							rootBone.rotation.Normalize();
						}
					}
				}
			}
		}
	}

	UpdateWorldTransforms(rootBone, ownerWorldTransform);

	MidWorldTransform = GetScaledNodeWorldTransform(midBone);
	TipWorldTransform = GetScaledNodeWorldTransform(tipBone);

	MidWorldPosition = MidWorldTransform.Translation();
	TipWorldPosition = TipWorldTransform.Translation();

	MidTipVec = TipWorldPosition - MidWorldPosition;
	Vector3 MidTargetVec = TargetWorldPosition - MidWorldPosition;

	RotateBone(midBone, MidTipVec, MidTargetVec);

	UpdateWorldTransforms(midBone, ownerWorldTransform);

	Quaternion solvedRootRotation = rootBone.rotation;
	Quaternion solvedMidRotation = midBone.rotation;

	rootBone.rotation = Quaternion::Slerp(originalRootRotation, solvedRootRotation, chain.weight);
	rootBone.rotation.Normalize();
	midBone.rotation = Quaternion::Slerp(originalMidRotation, solvedMidRotation, chain.weight);
	midBone.rotation.Normalize();

	UpdateWorldTransforms(rootBone, ownerWorldTransform);
}
