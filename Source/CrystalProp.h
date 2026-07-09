// CrystalProp.h

#pragma once

#include <memory>
#include <vector>

#include "Actor.h"
#include "Model.h"
#include "ShaderParam.h"
#include "StageLoader.h"

class MeshCollider;
class Rigidbody;

class CrystalProp : public Actor
{
public:
	CrystalProp(const StageLoader::CrystalData& crystalData);
	~CrystalProp() override = default;

	void ApplyStageData(const StageLoader::CrystalData& crystalData);
	bool GetNavMeshBounds(Vector3& center, Vector3& size) const;
	void Update() override;
	void Render(const RenderContext& rc) override;

private:
	struct Instance
	{
		Transform transform = {};
		std::shared_ptr<Model> model = nullptr;
		Rigidbody* rigidbody = nullptr;
		MeshCollider* meshCollider = nullptr;
	};

	static Matrix MakeColliderMatrix(const Matrix& world, Vector3& scale);
	static Matrix MakeMatrix(const Transform& transform);
	void SetColliderActive(Instance& instance, bool active);
	void SyncCollider(Instance& instance, const Matrix& world);

	std::vector<Instance> instances = {};
	ShaderParamListWithMaterialName shaderParams = {};
	std::string modelPath = "";
	Transform parentTransform = {};
};
