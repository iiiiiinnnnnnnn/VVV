#include "Gameplay/Actor/CrystalPart.h"

#include "Gameplay/Actor/ActorManager.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/VMDL.h"
#include "Rendering/Component/VMDLModelComponent.h"
#include "Resource/ResourceManager.h"
#include <algorithm>
#include <cmath>
#include <random>

CrystalPart::CrystalPart(const Transform& placement, std::shared_ptr<VMDLModel> model)
	: Actor("CrystalPart", "CrystalPart", true, placement), model(std::move(model)), initialScale(placement.scale)
{}

CrystalPart::~CrystalPart()
{
	Uninit();
}

void CrystalPart::OnAwake()
{
	vmdl = AddComponent<VMDL>(model);
	vmdl->Awake();
	vmdl->UpdateTransform(transform.GetMatrix());
}

/// <summary>
/// 
/// </summary>
/// <param name="position"></param>
/// <param name="velocity"></param>
/// <param name="size"></param>
/// <param name="rotationSpeed"></param>
/// <param name="source"></param>
/// <returns></returns>
std::shared_ptr<CrystalPart> CrystalPart::Spawn(const Vector3& position, const Vector3& velocity,
	float size, const Vector3& rotationSpeed, Actor* source)
{
	auto* manager = ActorManager::GetActive();
	if (!manager || !std::isfinite(size) || size <= 0.0f) return {};

	auto resource = ResourceManager::Instance().GetModelResource("CrystalPart");
	if (!resource) return {};

	// モデルはResourceManagerが保持し、ここではサイズだけを再利用する。
	static std::weak_ptr<const VMDLModel> boundsResource;
	static Vector3 center, modelSize;
	if (boundsResource.lock() != resource)
	{
		if (!resource->GetLocalBounds(center, modelSize)) return {};
		boundsResource = resource;
	}

	// GPUバッファを共有し、姿勢とコンポーネントの設定だけを複製する。
	auto model = resource->CloneRuntimeInstance();
	model->GetVmdlExtensionData().rootOffset -= center;
	Transform placement;
	placement.position = position;
	placement.scale = Vector3(size / std::max({modelSize.x, modelSize.y, modelSize.z}));
	static std::mt19937 random(std::random_device{}());
	std::uniform_real_distribution<float> angle(-DirectX::XM_PI, DirectX::XM_PI);
	placement.rotation = Quaternion::CreateFromYawPitchRoll(angle(random), angle(random), angle(random));

	auto fragment = std::make_shared<CrystalPart>(placement, std::move(model));
	manager->Register(fragment);
	fragment->StartMoving(velocity, rotationSpeed, source);
	return fragment;
}

/// <summary>
/// 
/// </summary>
/// <param name="position"></param>
/// <param name="velocity"></param>
/// <param name="count"></param>
/// <param name="size"></param>
/// <param name="outwardSpeed"></param>
/// <param name="radius"></param>
/// <param name="source"></param>
void CrystalPart::Spawn(const Vector3& position, const Vector3& velocity, int count,
	float size, float outwardSpeed, float radius, Actor* source)
{
	for (int i = 0; i < count; ++i)
	{
		const float angle = DirectX::XM_2PI * i / count;
		const Vector3 direction(sinf(angle), 0.0f, cosf(angle));
		Spawn(position + direction * radius, velocity + direction * outwardSpeed,
			size, {0.8f, 1.2f, 0.6f}, source);
	}
}

/// <summary>
/// 
/// </summary>
/// <param name="transform"></param>
/// <param name="center"></param>
/// <param name="size"></param>
/// <param name="velocity"></param>
/// <param name="outwardSpeed"></param>
/// <param name="fragmentSize"></param>
/// <param name="source"></param>
void CrystalPart::Spawn(const Transform& transform, const Vector3& center, const Vector3& size,
	const Vector3& velocity, float outwardSpeed, float fragmentSize, Actor* source)
{
	if (size.x <= 0.0f || size.y <= 0.0f || size.z <= 0.0f) return;
	constexpr float spacing = 1.4f;
	constexpr int maxPiecesPerAxis = 2; // 最大8個
	const Vector3 worldSize = size * Vector3(fabsf(transform.scale.x),
		fabsf(transform.scale.y), fabsf(transform.scale.z));
	const int countX = std::clamp(static_cast<int>(ceilf(worldSize.x / spacing)), 1, maxPiecesPerAxis);
	const int countY = std::clamp(static_cast<int>(ceilf(worldSize.y / spacing)), 1, maxPiecesPerAxis);
	const int countZ = std::clamp(static_cast<int>(ceilf(worldSize.z / spacing)), 1, maxPiecesPerAxis);
	const Vector3 cellSize = size / Vector3(float(countX), float(countY), float(countZ));
	const Matrix world = transform.GetMatrix();
	const Matrix rotation = Matrix::CreateFromQuaternion(transform.rotation);
	for (int y = 0; y < countY; ++y)
	{
		for (int z = 0; z < countZ; ++z)
		{
			for (int x = 0; x < countX; ++x)
			{
				const Vector3 offset((x + 0.5f - countX * 0.5f) * cellSize.x,
					(y + 0.5f - countY * 0.5f) * cellSize.y,
					(z + 0.5f - countZ * 0.5f) * cellSize.z);
				Vector3 direction = Vector3::TransformNormal(Vector3(offset.x, 0.0f, offset.z), rotation);
				if (direction.LengthSquared() < 0.0001f) direction = transform.GetForward();
				direction.Normalize();
				Vector3 impulse = velocity + direction * outwardSpeed;
				impulse.y = velocity.y * (0.8f + 0.4f * (y + 0.5f) / countY);
				Spawn(Vector3::Transform(center + offset, world), impulse, fragmentSize,
					{0.9f, x % 2 ? 1.2f : -1.2f, z % 2 ? 0.6f : -0.6f}, source);
			}
		}
	}
}

void CrystalPart::StartMoving(const Vector3& velocity, const Vector3& rotationSpeed, Actor* source)
{
	if (source) sourceActor = source->weak_from_this();
	vmdl->UpdateTransform(transform.GetMatrix());
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
		vmdl->UpdateTransform(transform.GetMatrix());
		const char* soundName = std::max({initialScale.x, initialScale.y, initialScale.z}) >= 1.0f ?
			"BREAK_LARGE" : "BREAK_SMALL";
		if (!vmdl->GetRenderer()->PlaySoundSource(soundName, &transform.position))
			vmdl->GetRenderer()->PlaySoundSource("BREAK", &transform.position);
		vmdl->GetRenderer()->PlayParticleEmitter("BREAK");
		Destroy();
		return;
	}

	constexpr float shrinkDuration = 1.0f;
	if (lifetimeTimer.Update()) { Destroy(); return; }
	const float remainingTime = lifetimeTimer.GetRemaining();
	if (remainingTime > shrinkDuration) return;
	if (!shrinking)
	{
		for (auto* collider : GetComponents<PhysicsComponent>()) collider->SetActive(false);
		shrinking = true;
	}
	transform.scale = initialScale * (remainingTime / shrinkDuration);
}

void CrystalPart::OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
	const Vector3& point, const Vector3& normal)
{
	if (contactPending || IsPendingDestroy()) return;
	const auto source = sourceActor.lock();
	if (source && other && other->GetOwner() == source.get()) return;
	contactPending = true;
}
