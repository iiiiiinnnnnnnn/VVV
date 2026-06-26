// FootIK.cpp

#include "FootIK.h"
#include "Actor.h"
#include "IconsFontAwesome5.h"
#include "Graphics.h"

FootIK::FootIK(
	Object* owner,
	Model* model,
	const char* thighName,
	const char* calfName,
	const char* footName,
	const char* ballName)
	: Component(owner), model(model)
{
	// FootIKのチェーンを作る
	chain.root = &model->GetNodes().at(model->GetNodeIndex(thighName));
	chain.mid = &model->GetNodes().at(model->GetNodeIndex(calfName));
	chain.tip = &model->GetNodes().at(model->GetNodeIndex(footName));

	// ball(接地)がないならfootを接地として使う
	if (ballName)
	{
		chain.contact = &model->GetNodes().at(model->GetNodeIndex(ballName));
	}
	else
	{
		chain.contact = chain.tip;
	}
}

void FootIK::LateUpdate()
{
	InitializeFromCurrentPose(0.5f);
	SyncPoleWorldPosition();
	if (chain.tip) chain.targetPosition = Matrix(chain.tip->worldTransform).Translation();

	// Raycast例
	{
		rayStart = GetContactWorldPosition() + Vector3(0, 1.0f, 0);
		rayEnd = GetContactWorldPosition() - Vector3(0, 3.0f, 0);

		Vector3 direction = rayEnd - rayStart;
		float distance = direction.Length();

		if (distance < 0.001f) return;
		direction.Normalize();

		PhysicsManager::PhysicsRaycastHit hit;
		if (PhysicsManager::Instance().Raycast(
			rayStart,
			direction,
			distance,
			hit,
			Layer::FootIK
			))
		{
			SetTargetFromContact(hit.position, hit.normal, 0.01f);
		}
	}

	SolveIK(model->GetWorldTransform());
}

void FootIK::Render(const RenderContext& rc)
{
	if (!isActive) return;
	if (!chain.enabled) return;
	if (chain.weight <= 0.0f) return;
	if (chain.root == nullptr) return;
	if (chain.mid == nullptr) return;
	if (chain.tip == nullptr) return;

	// IKターゲット位置を描画
	{
		Vector3 targetPosition = chain.targetPosition;
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			targetPosition,
			0.05f,
			{1, 1, 0, 1});
	}
	// Pole位置を描画
	{
		Vector3 polePosition = chain.polePosition;
		Game::Graphics::Instance().GetShapeRenderer()->DrawSphere(
			polePosition,
			0.05f,
			{0, 1, 1, 1});
	}
	// Ray描画
	{
		Game::Graphics::Instance().GetPrimitiveRenderer()->DrawLine(
			rayStart,
			rayEnd,
			{1, 0, 0, 1}, {1, 0, 0, 1});
	}
}

void FootIK::DrawGUI()
{
	if (ImGui::TreeNode(ICON_FA_BONE " FootIK"))
	{
		if (ImGui::DragFloat3("Pole Position", &chain.polePosition.x, 0.01f))
		{
			SyncPoleLocalPosition();
		}
		ImGui::TreePop();
	}
}

void FootIK::InitializeFromCurrentPose(float poleDistance)
{
	if (chain.poleInitialized) return;
	if (chain.root == nullptr) return;
	if (chain.mid == nullptr) return;
	if (chain.tip == nullptr) return;

	Matrix rootWorldTransform = chain.root->worldTransform;
	Matrix midWorldTransform  = chain.mid->worldTransform;
	Matrix tipWorldTransform  = chain.tip->worldTransform;

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

	chain.polePosition = midPosition + poleDirection * poleDistance;
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

	Model::Node* contactNode =
		chain.contact != nullptr
		? chain.contact
		: chain.tip;

	Matrix tipWorldTransform = chain.tip->worldTransform;
	Matrix contactWorldTransform = contactNode->worldTransform;

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
	chain.enabled = enabled;
}

void FootIK::SyncPoleWorldPosition()
{
	if (!model) return;
	if (!chain.poleInitialized) return;

	chain.polePosition = Vector3::Transform(
		chain.poleLocalPosition,
		model->GetWorldTransform());
}

void FootIK::SyncPoleLocalPosition()
{
	if (!model) return;

	Matrix inverseModelWorldTransform = model->GetWorldTransform().Invert();
	chain.poleLocalPosition = Vector3::Transform(
		chain.polePosition,
		inverseModelWorldTransform);
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
		Matrix world = chain.contact->worldTransform;
		return world.Translation();
	}

	if (chain.tip != nullptr)
	{
		Matrix world = chain.tip->worldTransform;
		return world.Translation();
	}

	return Vector3::Zero;
}

void FootIK::SolveIK(const DirectX::XMFLOAT4X4& modelWorldTransform)
{
	if (!chain.enabled) return;
	if (chain.weight <= 0.0f) return;
	if (chain.root == nullptr) return;
	if (chain.mid == nullptr) return;
	if (chain.tip == nullptr) return;
	if (!chain.poleInitialized) return;

	Model::Node& rootBone = *chain.root;
	Model::Node& midBone = *chain.mid;
	Model::Node& tipBone = *chain.tip;

	Matrix RootWorldTransform = rootBone.worldTransform;
	Matrix MidWorldTransform = midBone.worldTransform;
	Matrix TipWorldTransform = tipBone.worldTransform;

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

	if (rootTargetLength < 0.001f) return;
	if (rootMidLength < 0.001f) return;
	if (midTipLength < 0.001f) return;

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

	UpdateWorldTransforms(rootBone, modelWorldTransform);

	MidWorldTransform = midBone.worldTransform;
	TipWorldTransform = tipBone.worldTransform;

	MidWorldPosition = MidWorldTransform.Translation();
	TipWorldPosition = TipWorldTransform.Translation();

	MidTipVec = TipWorldPosition - MidWorldPosition;
	Vector3 MidTargetVec = TargetWorldPosition - MidWorldPosition;

	RotateBone(midBone, MidTipVec, MidTargetVec);

	UpdateWorldTransforms(midBone, modelWorldTransform);
}
