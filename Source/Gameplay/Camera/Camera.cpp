#include "Gameplay/Camera/Camera.h"

#include "Core/Object/Object.h"
#include "Core/Object/Transform.h"
#include "Physics/Core/PhysicsComponent.h"
#include "Physics/Core/PhysicsManager.h"

#include <cmath>

// コンストラクタ
Camera::Camera(Object* owner, int priority)
	: Component(owner), priority(priority), normalizedPriority(priority)
{
	// カメラ設定
	SetPerspectiveFov(
		DirectX::XMConvertToRadians(45),	// 画角
		1280.0f / 720.0f,					// 画面アスペクト比
		0.1f,								// ニアクリップ
		1000.0f								// ファークリップ
	);
	SetLookAt(
		{ 0, 0, -5 },		// 視点
		{ 0, 0, 0 },		// 注視点
		{ 0, 1, 0 }			// 上ベクトル
	);
}

// 指定方向を向く
void Camera::SetLookAt(const Vector3& eye, const Vector3& focus, const Vector3& up)
{
	// 視点、注視点、上方向からビュー行列を作成
	Matrix View = DirectX::XMMatrixLookAtLH(eye, focus, up);
	view = View;

	// ビューを逆行列化し、ワールド行列に戻す
	Matrix World = DirectX::XMMatrixInverse(nullptr, View);
	Matrix world;
	world = World;

	// カメラの方向を取り出す
	this->right.x = world._11;
	this->right.y = world._12;
	this->right.z = world._13;

	this->up.x = world._21;
	this->up.y = world._22;
	this->up.z = world._23;

	this->front.x = world._31;
	this->front.y = world._32;
	this->front.z = world._33;

	// 視点、注視点を保存
	this->eye = eye;
	this->focus = focus;
	focusDistance = Vector3::Distance(eye, focus);

	Transform* transform = owner->GetComponent<Transform>();
	if (!transform) return;

	transform->SetPosition(eye);
	transform->SetRotation(Quaternion::CreateFromRotationMatrix(world));
	transformPosition = transform->position;
	transformRotation = transform->rotation;
}

void Camera::SyncTransform() const
{
	const auto* transform = owner->GetComponent<Transform>();
	if (!transform) return;
	if (transform->position == transformPosition && transform->rotation == transformRotation) return;

	const Matrix rotation = Matrix::CreateFromQuaternion(transform->rotation);
	right = {rotation._11, rotation._12, rotation._13};
	up = {rotation._21, rotation._22, rotation._23};
	front = {rotation._31, rotation._32, rotation._33};
	eye = transform->position;
	focus = eye + front * focusDistance;
	view = DirectX::XMMatrixLookAtLH(eye, focus, up);
	transformPosition = transform->position;
	transformRotation = transform->rotation;
}

// パースペクティブ設定
void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
	// 画角、画面比率、クリップ距離からプロジェクション行列を作成
	Matrix Projection = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	projection = Projection;
}

bool Camera::IsInView(const Vector3& position, float radius) const
{
	if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
		!std::isfinite(position.z) || !std::isfinite(radius) || radius < 0.0f) return false;

	const Matrix vp = GetView() * GetProjection();
	// DirectXのクリップ範囲は -w <= x,y <= w、0 <= z <= w。
	const Vector4 planes[] = {
		{vp._14 + vp._11, vp._24 + vp._21, vp._34 + vp._31, vp._44 + vp._41},
		{vp._14 - vp._11, vp._24 - vp._21, vp._34 - vp._31, vp._44 - vp._41},
		{vp._14 + vp._12, vp._24 + vp._22, vp._34 + vp._32, vp._44 + vp._42},
		{vp._14 - vp._12, vp._24 - vp._22, vp._34 - vp._32, vp._44 - vp._42},
		{vp._13, vp._23, vp._33, vp._43},
		{vp._14 - vp._13, vp._24 - vp._23, vp._34 - vp._33, vp._44 - vp._43}
	};
	for (const Vector4& plane : planes)
	{
		const Vector3 normal(plane.x, plane.y, plane.z);
		if (normal.Dot(position) + plane.w < -radius * normal.Length()) return false;
	}
	return true;
}

bool Camera::IsVisible(const Object& target, const Vector3& centerOffset, float radius) const
{
	const Transform* transform = target.GetTransform();
	if (!transform) return false;

	const Vector3 center = Vector3::Transform(centerOffset, transform->GetMatrix());
	const Vector3& scale = transform->scale;
	radius *= std::max({fabsf(scale.x), fabsf(scale.y), fabsf(scale.z)});
	if (!IsInView(center, radius)) return false;

	// 中心が隠れていても、体の別の部分が見えれば視認扱いにする。
	const Vector3 offsets[] = {Vector3::Zero, Vector3::UnitX, -Vector3::UnitX,
		Vector3::UnitY, -Vector3::UnitY, Vector3::UnitZ, -Vector3::UnitZ};
	for (const Vector3& offset : offsets)
	{
		if (IsVisible(center + offset * radius * 0.7f, &target)) return true;
	}
	return false;
}

bool Camera::IsVisible(const Vector3& position, const Object* target) const
{
	if (!IsInView(position)) return false;
	Vector3 direction = position - GetEye();
	const float distance = direction.Length();
	if (distance <= 0.001f) return true;
	direction /= distance;

	struct VisibilityFilter : physx::PxQueryFilterCallback
	{
		const Object* target = nullptr;
		const Object* cameraOwner = nullptr;
		LayerId propLayer = Layers::Get("Prop");

		physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&,
			const physx::PxShape* shape, const physx::PxRigidActor* actor,
			physx::PxHitFlags&) override
		{
			if (!shape || !actor || (shape->getFlags() & physx::PxShapeFlag::eTRIGGER_SHAPE))
				return physx::PxQueryHitType::eNONE;
			const auto* collider = static_cast<const PhysicsComponent*>(shape->userData);
			if (collider && (!PhysicsComponent::IsLive(collider) || !collider->IsActive()))
				return physx::PxQueryHitType::eNONE;
			const Object* object = collider ? collider->GetOwner() : static_cast<const Object*>(actor->userData);
			if (object && (object == target || object == cameraOwner ||
				!object->IsActive() || object->IsPendingDestroy())) return physx::PxQueryHitType::eNONE;
			if (actor->is<physx::PxRigidStatic>() || (collider && collider->GetLayerId() == propLayer) ||
				(object && object->CompareTag("Prop"))) return physx::PxQueryHitType::eBLOCK;
			return physx::PxQueryHitType::eNONE;
		}

		physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&, const physx::PxQueryHit&,
			const physx::PxShape*, const physx::PxRigidActor*) override
		{
			return physx::PxQueryHitType::eBLOCK;
		}
	} filter;
	filter.target = target;
	filter.cameraOwner = owner;
	physx::PxScene* scene = PhysicsManager::Instance().GetSceneContext().GetScene();
	if (!scene) return false;
	physx::PxRaycastBuffer hit;
	physx::PxQueryFilterData query;
	query.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
	const Vector3& origin = GetEye();
	return !scene->raycast(physx::PxVec3(origin.x, origin.y, origin.z),
		physx::PxVec3(direction.x, direction.y, direction.z), distance, hit,
		physx::PxHitFlag::eDEFAULT | physx::PxHitFlag::eMESH_BOTH_SIDES, query, &filter);
}
