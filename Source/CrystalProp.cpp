// CrystalProp.cpp

#include "CrystalProp.h"

#include "Graphics.h"
#include "MeshCollider.h"
#include "NavMeshActor.h"
#include "NavMeshObstacle.h"
#include "ModelRenderer.h"
#include "ResourceManager.h"
#include "Rigidbody.h"

CrystalProp::CrystalProp(const StageLoader::CrystalData& crystalData)
	: Actor("CrystalProp", "CrystalProp", true)
{
	ApplyStageData(crystalData);
	AddComponent<NavMeshObstacle>();
	if (NavMeshActor* navMeshActor = NavMeshActor::GetActive())
		navMeshActor->RequestBuild();
}

Matrix CrystalProp::MakeColliderMatrix(const Matrix& world, Vector3& scale)
{
	Quaternion rotation;
	Vector3 position;
	Matrix copy = world;
	copy.Decompose(scale, rotation, position);
	return Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
}

Matrix CrystalProp::MakeMatrix(const Transform& transform)
{
	return Matrix::CreateScale(transform.scale) *
		Matrix::CreateFromQuaternion(transform.rotation) *
		Matrix::CreateTranslation(transform.position);
}

void CrystalProp::SetColliderActive(Instance& instance, bool active)
{
	if (!instance.rigidbody) return;
	if (!instance.rigidbody->GetRigidActor()) return;
	instance.rigidbody->GetRigidActor()->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !active);
}

void CrystalProp::SyncCollider(Instance& instance, const Matrix& world)
{
	if (!instance.model) return;

	Vector3 scale;
	Matrix colliderMatrix = MakeColliderMatrix(world, scale);

	if (!instance.rigidbody)
	{
		instance.rigidbody = AddComponent<RigidbodyStatic>(colliderMatrix);
		instance.meshCollider = AddComponent<MeshCollider>(Layers::Get("Prop"), instance.rigidbody, instance.model.get(), scale);
	}
	else
	{
		instance.rigidbody->GetRigidActor()->setGlobalPose(Conv::ToPxTransform(colliderMatrix));
		if (instance.meshCollider) instance.meshCollider->SetLocalScale(scale);
	}

	SetColliderActive(instance, true);
}

void CrystalProp::ApplyStageData(const StageLoader::CrystalData& crystalData)
{
	parentTransform = crystalData.parentTransform;

	if (modelPath != crystalData.modelPath)
	{
		for (Instance& instance : instances)
		{
			SetColliderActive(instance, false);
		}

		modelPath = crystalData.modelPath;
		instances.clear();
	}

	for (size_t i = crystalData.transforms.size(); i < instances.size(); ++i)
	{
		SetColliderActive(instances[i], false);
	}

	instances.resize(crystalData.transforms.size());

	for (size_t i = 0; i < crystalData.transforms.size(); ++i)
	{
		Instance& instance = instances[i];
		instance.transform = crystalData.transforms[i];

		if (!instance.model)
		{
			instance.model = ResourceManager::Instance().LoadModel(modelPath);
		}

		ModelRenderer::SetShaderParamForAllMaterials(
			instance.model.get(),
			crystalData.MakePBRParams(),
			shaderParams);
	}
}

bool CrystalProp::GetNavMeshBounds(Vector3& center, Vector3& size) const
{
	Vector3 minPosition(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 maxPosition(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	bool hasBounds = false;

	for (const Instance& instance : instances)
	{
		Vector3 instanceCenter;
		Vector3 instanceSize;
		if (instance.meshCollider)
		{
			if (!instance.meshCollider->GetBounds(instanceCenter, instanceSize)) continue;
		}
		else
		{
			if (!instance.model) continue;

			Vector3 instanceScale;
			Matrix world =
				MakeMatrix(instance.transform) *
				MakeMatrix(parentTransform);
			Matrix colliderMatrix = MakeColliderMatrix(world, instanceScale);
			Matrix actorTransform = colliderMatrix;

			Vector3 minVertex(FLT_MAX, FLT_MAX, FLT_MAX);
			Vector3 maxVertex(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			bool hasVertex = false;

			for (const Model::Mesh& mesh : instance.model->GetMeshes())
			{
				if (!mesh.isDraw) continue;
				if (!mesh.node) continue;

				Matrix localVertexTransform =
					mesh.node->globalTransform *
					Matrix::CreateScale(instanceScale);

				for (const Model::Vertex& vertex : mesh.vertices)
				{
					Vector3 position = Vector3::Transform(vertex.position, localVertexTransform);
					position = Vector3::Transform(position, actorTransform);

					if (position.x < minVertex.x) minVertex.x = position.x;
					if (position.y < minVertex.y) minVertex.y = position.y;
					if (position.z < minVertex.z) minVertex.z = position.z;

					if (position.x > maxVertex.x) maxVertex.x = position.x;
					if (position.y > maxVertex.y) maxVertex.y = position.y;
					if (position.z > maxVertex.z) maxVertex.z = position.z;

					hasVertex = true;
				}
			}

			if (!hasVertex) continue;

			instanceCenter = (minVertex + maxVertex) * 0.5f;
			instanceSize = maxVertex - minVertex;
		}

		Vector3 halfSize = instanceSize * 0.5f;
		Vector3 instanceMin = instanceCenter - halfSize;
		Vector3 instanceMax = instanceCenter + halfSize;

		if (instanceMin.x < minPosition.x) minPosition.x = instanceMin.x;
		if (instanceMin.y < minPosition.y) minPosition.y = instanceMin.y;
		if (instanceMin.z < minPosition.z) minPosition.z = instanceMin.z;

		if (instanceMax.x > maxPosition.x) maxPosition.x = instanceMax.x;
		if (instanceMax.y > maxPosition.y) maxPosition.y = instanceMax.y;
		if (instanceMax.z > maxPosition.z) maxPosition.z = instanceMax.z;

		hasBounds = true;
	}

	if (!hasBounds) return false;

	center = (minPosition + maxPosition) * 0.5f;
	size = maxPosition - minPosition;
	return true;
}

void CrystalProp::Update()
{
	Actor::Update();
	parentTransform.Update();

	for (Instance& instance : instances)
	{
		instance.transform.Update();
		Matrix world = instance.transform.matrix * parentTransform.matrix;
		if (instance.model) instance.model->UpdateTransform(world);
		SyncCollider(instance, world);
	}
}

void CrystalProp::Render(const RenderContext& rc)
{
	for (const Instance& instance : instances)
	{
		if (!instance.model) continue;
		Game::Graphics::Instance().GetModelRenderer()->Draw(ModelShaderId::PBR, instance.model, shaderParams);
	}
}
