#include "Gameplay/Actor/Crystal.h"

#include "Gameplay/Scene/CameraEffectController.h"
#include "Gameplay/Scene/TimeScaleController.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Rendering/Component/VMDL.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Application/Time/GameTime.h"
#include "Resource/ResourceManager.h"
#include <cmath>
#include <cfloat>
#include <random>
#include "Physics/Collider/VMDLColliderComponent.h"

// Base

BaseCrystal::BaseCrystal(const Transform& placement, const std::string& name, const std::string& modelPath)
	: Entity(name, "Crystal", true, placement)
{
	transform.Update();
	loadedModel = ResourceManager::Instance().LoadModel(modelPath);
}

BaseCrystal::BaseCrystal(const Transform& placement, const std::string& name, std::shared_ptr<VMDLModel> model)
	: Entity(name, "Crystal", true, placement)
{
	transform.Update();
	loadedModel = std::move(model);
}

void BaseCrystal::OnAwake()
{
	vmdl = AddComponent<VMDL>(loadedModel);
	vmdl->UpdateTransform(transform.matrix);
}

bool BaseCrystal::PlayBreakSound()
{
	float largestScale = std::max(fabsf(transform.scale.x), fabsf(transform.scale.y));
	largestScale = std::max(largestScale, fabsf(transform.scale.z));

	const bool isLarge = largestScale >= 1.0f;
	const char* sourceName = isLarge ? "BREAK_LARGE" : "BREAK_SMALL";
	if (vmdl->GetRenderer()->PlaySoundSource(sourceName, &transform.position)) return true;
	return vmdl->GetRenderer()->PlaySoundSource("BREAK", &transform.position);
}

bool BaseCrystal::PlayBreakParticle()
{
	return vmdl->GetRenderer()->PlayParticleEmitter("BREAK");
}

// Crystal Part

// 破壊時に破片を生成するため、モデルのバウンディングボックスを計算する。
bool GetCrystalModelBounds(const VMDLModel& model, Vector3& center, Vector3& size)
{
	Vector3 minimum(FLT_MAX), maximum(-FLT_MAX);
	bool found = false;
	const Matrix placement = Matrix::CreateScale(model.GetModelScale()) *
		Matrix::CreateTranslation(model.GetVmdlExtensionData().rootOffset);
	for (const auto& mesh : model.GetMeshes())
	{
		if (!mesh.isDraw) continue;
		const Matrix world = (mesh.node ? mesh.node->globalTransform : Matrix::Identity) * placement;
		for (const auto& vertex : mesh.vertices)
		{
			const Vector3 point = Vector3::Transform(vertex.position, world);
			minimum = Vector3::Min(minimum, point);
			maximum = Vector3::Max(maximum, point);
			found = true;
		}
	}
	if (!found) return false;
	center = (minimum + maximum) * 0.5f;
	size = maximum - minimum;
	return size.x > 0.0001f && size.y > 0.0001f && size.z > 0.0001f;
}

CrystalPart::CrystalPart(const Transform& placement, std::shared_ptr<VMDLModel> model)
	: BaseCrystal(placement, "CrystalPart", model), initialScale(placement.scale)
{
}

void CrystalPart::StartMoving(const Vector3& velocity, const Vector3& rotationSpeed)
{
	vmdl->UpdateTransform(transform.matrix);
	for (auto* collider : GetComponents<VMDLColliderComponent>())
	{
		collider->UpdateFromNode();
		if (!collider->StartDynamicMotion(velocity, rotationSpeed)) continue;
		body = collider;
		break;
	}
}

void CrystalPart::OnUpdate()
{
	if (body) body->SyncOwnerTransform();
	if (contactPending)
	{
		vmdl->UpdateTransform(transform.matrix);
		PlayBreakSound();
		PlayBreakParticle();
		Destroy();
		return;
	}

	constexpr float lifetime = 6.0f;
	constexpr float shrinkDuration = 1.0f;
	elapsedTime += Game::Time::deltaTime;
	if (elapsedTime >= lifetime) { Destroy(); return; }
	const float remainingTime = lifetime - elapsedTime;
	if (remainingTime > shrinkDuration) return;
	if (!shrinking)
	{
		for (auto* collider : GetComponents<PhysicsComponent>()) collider->SetActive(false);
		shrinking = true;
	}
	transform.scale = initialScale * (remainingTime / shrinkDuration);
	transform.Update();
}

void CrystalPart::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
	const Vector3& point, const Vector3& normal)
{
	if (contactPending || IsPendingDestroy()) return;
	contactPending = true;
}

// Crystal

Crystal::Crystal(const Transform& transform, const std::string& modelPath) : BaseCrystal(transform, "Crystal", modelPath)
{
	this->transform.Update();

	float largestScale = std::max(fabsf(this->transform.scale.x), fabsf(this->transform.scale.y));
	largestScale = std::max(largestScale, fabsf(this->transform.scale.z));

	constexpr float baseScale = 0.5f;
	constexpr float lifePerScale = 87.0f;

	maxLife = ceilf((largestScale - baseScale) * lifePerScale);
	maxLife = std::clamp(maxLife, 1.0f, 100.0f);
	life = maxLife;

	// サイズの走査と描画用モデルの準備は、破壊時ではなく生成時に済ませる。
	auto source = ResourceManager::Instance().LoadModel("Resources/Model/CrystalPart.vmdl");
	Vector3 partCenter;
	if (!source || !GetCrystalModelBounds(*source, partCenter, partSize)) return;
	if (!GetCrystalModelBounds(*loadedModel, crystalCenter, crystalSize)) return;
	partModel = source;
	partModel->GetVmdlExtensionData().rootOffset =
		source->GetVmdlExtensionData().rootOffset - partCenter;
}

void Crystal::OnUpdate()
{
	if (deathCleanupPending)
	{
		SpawnFragments();
		Destroy();
		return;
	}

	Entity::OnUpdate();
}

void Crystal::OnDamaged(const DamageData& damageData)
{
	vmdl->GetRenderer()->PlaySoundSource("ATTACK", &transform.position);
	TimeScaleController::Request(0.06f);
	CameraEffectController::Request(0.1f, 0.06f);
}

void Crystal::OnDead(const DamageData& damageData)
{
	if (deathCleanupPending) return;

	life = 0.0f;
	deathCleanupPending = true;
	for (PhysicsComponent* collider : GetComponents<PhysicsComponent>())
		if (collider) collider->SetActive(false);
	PlayBreakSound();
	PlayBreakParticle();
}

void Crystal::SpawnFragments()
{
	ActorManager* manager = ActorManager::GetActive();
	if (!manager || !partModel) return;

	// 破片を周りに飛ばす処理
	constexpr float fragmentSize = 0.6f;
	constexpr float fragmentSpacing = 1.4f;
	constexpr int maxPiecesPerAxis = 2; // 最大8個
	constexpr float outwardSpeed = 4.0f;
	constexpr float upwardSpeed = 3.0f;
	const Vector3 worldSize = crystalSize * Vector3(fabsf(transform.scale.x),
		fabsf(transform.scale.y), fabsf(transform.scale.z));
	const int countX = std::clamp(static_cast<int>(ceilf(worldSize.x / fragmentSpacing)), 1, maxPiecesPerAxis);
	const int countY = std::clamp(static_cast<int>(ceilf(worldSize.y / fragmentSpacing)), 1, maxPiecesPerAxis);
	const int countZ = std::clamp(static_cast<int>(ceilf(worldSize.z / fragmentSpacing)), 1, maxPiecesPerAxis);
	const Vector3 cellSize = crystalSize / Vector3(float(countX), float(countY), float(countZ));
	const float partScale = fragmentSize / std::max({partSize.x, partSize.y, partSize.z});
	static std::mt19937 random(std::random_device{}());
	std::uniform_real_distribution<float> angle(-DirectX::XM_PI, DirectX::XM_PI);

	for (int y = 0; y < countY; ++y)
	{
		for (int z = 0; z < countZ; ++z)
		{
			for (int x = 0; x < countX; ++x)
			{
				const Vector3 localOffset(
					(x + 0.5f - countX * 0.5f) * cellSize.x,
					(y + 0.5f - countY * 0.5f) * cellSize.y,
					(z + 0.5f - countZ * 0.5f) * cellSize.z);
				Transform placement;
				placement.position =
					Vector3::Transform(crystalCenter + localOffset, transform.matrix);
				placement.rotation = Quaternion::CreateFromYawPitchRoll(
					angle(random), angle(random), angle(random));
				placement.scale = Vector3(partScale);
				placement.Update();

				Vector3 direction =
					Vector3::TransformNormal(Vector3(localOffset.x, 0.0f, localOffset.z),
						Matrix::CreateFromQuaternion(transform.rotation));
				if (direction.LengthSquared() < 0.0001f) direction = transform.forward;
				direction.Normalize();
				Vector3 velocity = direction * outwardSpeed;
				velocity.y = upwardSpeed * (0.8f + 0.4f * (y + 0.5f) / countY);
				const Vector3 rotationSpeed(0.9f, x % 2 ? 1.2f : -1.2f, z % 2 ? 0.6f : -0.6f);

				// GPUの頂点・インデックスバッファを共有し、姿勢だけを別々に持つ。
				auto model = partModel->CloneRuntimeInstance();
				auto fragment = std::make_shared<CrystalPart>(placement, model);
				manager->Register(fragment);
				fragment->StartMoving(velocity, rotationSpeed);
			}
		}
	}
}
