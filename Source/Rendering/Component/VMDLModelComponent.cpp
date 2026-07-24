// VMDLModelComponent.cpp

#include "Rendering/Component/VMDLModelComponent.h"
#include "Animation/Animator.h"
#include "Animation/SpringBone.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Actor/Actor.h"
#include "Physics/Collider/VMDLColliderComponent.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "IconsFontAwesome5.h"

VMDLModelComponent::VMDLModelComponent(
    Object* owner, std::shared_ptr<VMDLModel> model,
    ModelShaderId shaderId,
	VMatRenderParams renderParams)
    : Component(owner), model(model), shaderId(shaderId), renderParams(std::move(renderParams))
{
    // エラー用
    dynamic_cast<Actor*>(owner);

    if (model)
    {
		const auto& placement = model->GetVmdlPlacementData();
		const Matrix placementTransform =
			Matrix::CreateScale(placement.scale) *
			Matrix::CreateTranslation(model->GetVmdlExtensionData().rootOffset);
		model->UpdateTransform(placementTransform);
    }
}

void VMDLModelComponent::OnAwake()
{
	BuildAttachments();
}

void VMDLModelComponent::BuildAttachments()
{
	if (attachmentsBuilt || !model) return;
	attachmentsBuilt = true;

	const auto& data = model->GetVmdlExtensionData();
	attachmentColliders.assign(data.colliders.size(), nullptr);
	for (int colliderIndex = 0; colliderIndex < static_cast<int>(data.colliders.size()); ++colliderIndex)
	{
		const auto& value = data.colliders[colliderIndex];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size())) continue;

		const Matrix offset =
			Matrix::CreateFromYawPitchRoll(RAD(value.rotation.y), RAD(value.rotation.x), RAD(value.rotation.z)) *
			Matrix::CreateTranslation(value.center);

		auto* collider = owner->AddComponent<VMDLColliderComponent>(
			attachmentLayerId,
			model.get(),
			value.nodeIndex,
			value.shape,
			value.size,
			offset,
			nullptr,
			value.trigger);

		collider->SetName(value.name);
		collider->SetActive(model->GetColliderInitialActive(colliderIndex));
		attachmentColliders[colliderIndex] = collider;
	}

	std::vector<SpringBone::SpringCapsule> springColliders;
	for (const auto& value : data.springColliders)
	{
		SpringBone::SpringCapsule collider;
		collider.start = value.offsetPosition;
		collider.end = value.offsetPosition;
		collider.radius = value.radius;
		collider.nodeIndex = value.nodeIndex;
		springColliders.push_back(collider);
	}
	for (const auto& value : data.springs)
	{
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size())) continue;
		auto* spring = owner->AddComponent<SpringBone>(
			attachmentLayerId, model.get(), value.nodeIndex, springColliders, value.stiffness, value.drag);
		spring->SetName(value.name);
	}

	const auto& trailData = model->GetVmdlTrailData();
	if (!buildEmbeddedTrails) return;
	attachmentTrails.assign(trailData.trails.size(), nullptr);
	for (int trailIndex = 0; trailIndex < static_cast<int>(trailData.trails.size()); ++trailIndex)
	{
		const auto& value = trailData.trails[trailIndex];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size())) continue;
		auto* trail = owner->AddComponent<TrailRenderComponent>(
			model.get(), value.nodeIndex, value.rootOffset, value.tipOffset, value.color,
			value.tipRatio, std::max(0.01f, value.lifeTime), std::max(2, value.maxPoints));
		if (!model->GetTrailInitialActive(trailIndex)) trail->StopTrail();
		attachmentTrails[trailIndex] = trail;
	}
}

PhysicsComponent* VMDLModelComponent::GetAttachmentCollider(const std::string& name) const
{
	for (VMDLColliderComponent* collider : attachmentColliders)
	{
		if (collider && collider->CompareName(name)) return collider;
	}
	return nullptr;
}

void VMDLModelComponent::LateUpdate()
{
    Actor* actor = dynamic_cast<Actor*>(owner);

    if (!model)
        return;
	UpdateAnimationControls();

	if (autoUpdateTransform)
	{
		const auto& placement = model->GetVmdlPlacementData();
		const Matrix placementTransform =
			Matrix::CreateScale(placement.scale) *
			Matrix::CreateTranslation(model->GetVmdlExtensionData().rootOffset);
		model->UpdateTransform(placementTransform * actor->transform.matrix);
	}

	for (VMDLColliderComponent* collider : attachmentColliders)
	{
		if (collider && collider->IsActive()) collider->UpdateFromNode();
	}
}

void VMDLModelComponent::UpdateAnimationControls()
{
	if (!model) return;
	if (!animator) animator = owner->GetComponent<Animator>();
	if (!animator || animator->IsDynamicMode()) return;

	const int animationIndex = animator->GetCurrentAnimationIndex();
	if (animationIndex < 0)
	{
		if (animationControlsApplied) RestoreAnimationControls();
		return;
	}

	const float time = animator->GetCurrentAnimationTime();
	for (int i = 0; i < static_cast<int>(attachmentColliders.size()); ++i)
	{
		if (attachmentColliders[i]) attachmentColliders[i]->SetActive(model->EvaluateColliderActive(animationIndex, time, i));
	}
	for (int i = 0; i < static_cast<int>(attachmentTrails.size()); ++i)
	{
		if (!attachmentTrails[i]) continue;
		if (model->EvaluateTrailActive(animationIndex, time, i)) attachmentTrails[i]->StartTrail();
		else attachmentTrails[i]->StopTrail();
	}
	model->ApplyShapeAnimation(animationIndex, time);
	animationControlsApplied = true;
}

void VMDLModelComponent::RestoreAnimationControls()
{
	if (!model) return;
	for (int i = 0; i < static_cast<int>(attachmentColliders.size()); ++i)
	{
		if (attachmentColliders[i]) attachmentColliders[i]->SetActive(model->GetColliderInitialActive(i));
	}
	for (int i = 0; i < static_cast<int>(attachmentTrails.size()); ++i)
	{
		if (!attachmentTrails[i]) continue;
		if (model->GetTrailInitialActive(i)) attachmentTrails[i]->StartTrail();
		else attachmentTrails[i]->StopTrail();
	}
	model->RestoreRuntimeShapeVisibility();
	animationControlsApplied = false;
}

void VMDLModelComponent::Render(const RenderContext& rc)
{
    if (model)
    {
        Game::Graphics::Instance().GetModelRenderer()->Draw(shaderId, model, &renderParams);
    }
}

void VMDLModelComponent::SetMaterialParamsForAllMaterials(const VMatMaterialParams& params)
{
	if (!model) return;
	for (const VMDLModel::Material& material : model->GetMaterials())
		renderParams.materials[material.name] = params;
}

void VMDLModelComponent::DrawGUI()
{
    auto drawVector3 = [](const char* label, const Vector3& value)
    {
        ImGui::Text("%s: %.3f, %.3f, %.3f", label, value.x, value.y, value.z);
    };

    auto drawQuaternion = [](const char* label, const Quaternion& value)
    {
        ImGui::Text("%s: %.3f, %.3f, %.3f, %.3f", label, value.x, value.y, value.z, value.w);
    };

    auto drawMatrixTransform = [&](const char* label, const Matrix& matrix)
    {
        Vector3 scale;
        Vector3 position;
        Quaternion rotation;
        Matrix work = matrix;
        work.Decompose(scale, rotation, position);

        ImGui::SeparatorText(label);
        drawVector3("Position", position);
        drawQuaternion("Rotation", rotation);
        drawVector3("Scale", scale);
    };

    auto drawNodeTooltip = [&](VMDLModel::Node* node, int nodeIndex, const std::string& meshIndices)
    {
        ImGui::BeginTooltip();

        ImGui::Text("Node[%d] %s", nodeIndex, node->name.c_str());
        ImGui::Text("Parent: %d", node->parentIndex);
        ImGui::Text("Children: %zu", node->children.size());
        ImGui::Text("Meshes: %s", meshIndices.empty() ? "None" : meshIndices.c_str());

        ImGui::SeparatorText("Local Node");
        drawVector3("Position", node->position);
        drawQuaternion("Rotation", node->rotation);
        drawVector3("Scale", node->scale);

        drawMatrixTransform("Local Matrix", node->localTransform);
        drawMatrixTransform("Global Matrix", node->globalTransform);
        drawMatrixTransform("World Matrix", node->worldTransform);

        ImGui::EndTooltip();
    };

    // ノードツリーを再帰的に描画する関数
    std::function<void(VMDLModel::Node*)> drawNodeTree = [&](VMDLModel::Node* node)
    {
        // 矢印をクリック、またはノードをダブルクリックで階層を開く
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
            | ImGuiTreeNodeFlags_OpenOnDoubleClick;

        // 子がいない場合は矢印をつけない
        size_t childCount = node->children.size();
        if (childCount == 0)
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        bool isAnyMeshHidden = false;
        std::string meshIndices = "";

        // このノードに関連するメッシュを探す
        for (int i = 0; i < model->GetMeshes().size(); i++)
        {
            const VMDLModel::Mesh& mesh = model->GetMeshes()[i];
            if (mesh.node == node)
            {
                if (!meshIndices.empty()) meshIndices += ",";
                meshIndices += std::to_string(i);

                if (!mesh.isDraw)
                    isAnyMeshHidden = true;
            }
        }

        // ツリーノードを表示
        ImGui::PushStyleColor(ImGuiCol_Text,
            IM_COL32(255, 255, 255, isAnyMeshHidden ? 100 : 255));

        std::string meshStr = meshIndices.empty() ? "" : "{" + meshIndices + "}";
        int nodeIndex = static_cast<int>(node - model->GetNodes().data());

        // ノード名とインデックスに続けて、[x, y, z] 形式でポジションを表示
        bool opened = ImGui::TreeNodeEx(node, nodeFlags,
            "[%d]%s%s [%.2f, %.2f, %.2f]",
            nodeIndex,
            meshStr.c_str(),
            node->name.c_str(),
            node->position.x, node->position.y, node->position.z);

        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() || ImGui::IsItemFocused())
        {
            drawNodeTooltip(node, nodeIndex, meshIndices);
        }

        if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            for (VMDLModel::Mesh& mesh : model->GetMeshes())
            {
                if (mesh.node == node)
                {
                    mesh.isDraw = !mesh.isDraw;
                }
            }
        }

        // 開かれている場合、子階層も同じ処理を行う
        if (opened && childCount > 0)
        {
            for (VMDLModel::Node* child : node->children)
            {
                drawNodeTree(child);
            }
            ImGui::TreePop();
        }
    };

    // すべてのルートノード（親を持たないノード）を起点に描画
    for (VMDLModel::Node& node : model->GetNodes())
    {
        if (node.parent == nullptr)
        {
            drawNodeTree(&node);
        }
    }

    if (ImGui::TreeNode(ICON_FA_PAINT_BRUSH " Materials"))
    {
        for (const VMDLModel::Material& material : model->GetMaterials())
        {
            if (ImGui::TreeNode(material.name.c_str()))
            {
				ImGui::Text("Base Color: %.3f, %.3f, %.3f, %.3f",
					material.baseColor.x, material.baseColor.y, material.baseColor.z, material.baseColor.w);
				ImGui::Text("Emissive Color: %.3f, %.3f, %.3f, %.3f",
					material.emissiveColor.x, material.emissiveColor.y,
					material.emissiveColor.z, material.emissiveColor.w);
				ImGui::Text("Metalness: %.3f", material.metalness);
				ImGui::Text("Roughness: %.3f", material.roughness);
				ImGui::Text("Occlusion: %.3f", material.occlusion);
				ImGui::Text("Occlusion Strength: %.3f", material.occlusionStrength);
				ImGui::Text("Shadow Strength: %.3f", material.shadowStrength);

				const auto it = renderParams.materials.find(material.name);
				if (it != renderParams.materials.end())
				{
					const VMatMaterialParams& params = it->second;
					ImGui::SeparatorText("Instance Overrides");
					if (params.baseColor) ImGui::Text("Base Color: %.3f, %.3f, %.3f, %.3f",
						params.baseColor->x, params.baseColor->y, params.baseColor->z, params.baseColor->w);
					if (params.emissionColor) ImGui::Text("Emission: %.3f, %.3f, %.3f, %.3f",
						params.emissionColor->x, params.emissionColor->y,
						params.emissionColor->z, params.emissionColor->w);
					if (params.metalness) ImGui::Text("Metalness: %.3f", *params.metalness);
					if (params.roughness) ImGui::Text("Roughness: %.3f", *params.roughness);
					if (params.occlusion) ImGui::Text("Occlusion: %.3f", *params.occlusion);
					if (params.occlusionStrength) ImGui::Text("Occlusion Strength: %.3f", *params.occlusionStrength);
					if (params.shadowStrength) ImGui::Text("Shadow Strength: %.3f", *params.shadowStrength);
				}
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}
