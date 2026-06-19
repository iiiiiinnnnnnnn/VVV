// Prop.cpp

#include "Prop.h"
#include "ResourceManager.h"

Prop::Prop(const std::filesystem::path& path, const Transform& transform, bool isDynamic, int meshColliderConvex, int animationIndex, const std::string& tag, bool isActive, int layer)
	: Actor(path.stem().string().c_str(), tag, isActive, layer)
{
	std::shared_ptr<Model> model = ResourceManager::Instance().LoadModel(path.string());
	Build(model, transform, isDynamic, meshColliderConvex, animationIndex, tag, isActive, layer);
}

Prop::Prop(std::shared_ptr<Model> model, const Transform& transform, bool isDynamic, int meshColliderConvex, int animationIndex, const std::string& tag, bool isActive, int layer)
{
	Build(model, transform, isDynamic, meshColliderConvex, animationIndex, tag, isActive, layer);
}

void Prop::Build(std::shared_ptr<Model> model, const Transform& transform, bool isDynamic, int meshColliderConvex, int animationIndex, const std::string& tag, bool isActive, int layer)
{
	this->transform = transform;
	this->transform.Update();

	// モデルレンダラー生成
	AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);

	// アニメーター生成
	if (animationIndex > 0)
	{
		Animator* anim = AddComponent<Animator>();
		anim->AddLayer("Base Layer");
		anim->AddState(0, "Animation", animationIndex);
	}

	if (meshColliderConvex > 0)
	{
		model->UpdateTransform(Matrix::CreateScale(this->transform.scale));

		if (isDynamic)
		{
			// 動的オブジェクトならRigidbodyとMeshColliderを追加
			RigidbodyDynamic* rb = AddComponent<RigidbodyDynamic>();
			AddComponent<MeshCollider>(rb, model.get(), true, meshColliderConvex, PhysicsManager::Instance().GetPhysics()->createMaterial(0.5f, 0.5f, 0));
		}
		else
		{
			// 静的オブジェクトならRigidbodyStaticとMeshColliderを追加
			RigidbodyStatic* rb = AddComponent<RigidbodyStatic>();
			AddComponent<MeshCollider>(rb, model.get(), true, meshColliderConvex, PhysicsManager::Instance().GetPhysics()->createMaterial(0.5f, 0.5f, 0));
		}

		model->UpdateTransform(this->transform.matrix);
	}
}
