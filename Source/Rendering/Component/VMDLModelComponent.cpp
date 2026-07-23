// VMDLModelComponent.cpp

#include "Rendering/Component/VMDLModelComponent.h"
#include "Animation/Animator.h"
#include "Animation/SpringBone.h"
#include "Rendering/Core/Graphics.h"
#include "Gameplay/Actor/Actor.h"
#include "Physics/Collider/BoneBoxCollider.h"
#include "Physics/Collider/BoneCapsuleCollider.h"
#include "Physics/Collider/BoneSphereCollider.h"
#include "Rendering/Component/TrailRenderComponent.h"
#include "IconsFontAwesome5.h"

VMDLModelComponent::VMDLModelComponent(
    Object* owner, std::shared_ptr<VMDLModel> model,
    ModelShaderId shaderId, ShaderParamListWithMaterialName paramsWithMaterial)
    : Component(owner), model(model), shaderId(shaderId), paramsWithMaterial(paramsWithMaterial)
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
	initialMeshVisibility.clear();
	initialMeshVisibility.reserve(model->GetMeshes().size());
	for (const auto& mesh : model->GetMeshes()) initialMeshVisibility.push_back(mesh.isDraw ? 1 : 0);

	const auto& data = model->GetVmdlExtensionData();
	attachmentColliders.assign(data.colliders.size(), nullptr);
	for (int colliderIndex = 0; colliderIndex < static_cast<int>(data.colliders.size()); ++colliderIndex)
	{
		const auto& value = data.colliders[colliderIndex];
		if (value.nodeIndex < 0 || value.nodeIndex >= static_cast<int>(model->GetNodes().size())) continue;

		const Matrix offset =
			Matrix::CreateFromYawPitchRoll(RAD(value.rotation.y), RAD(value.rotation.x), RAD(value.rotation.z)) *
			Matrix::CreateTranslation(value.center);
		PhysicsComponent* collider = nullptr;
		switch (value.shape)
		{
		case 1:
			collider = owner->AddComponent<BoneSphereCollider>(
				attachmentLayerId, model.get(), value.nodeIndex, std::max(0.001f, value.size.x),
				offset, nullptr, value.trigger);
			break;
		case 2:
			collider = owner->AddComponent<BoneCapsuleCollider>(
				attachmentLayerId, model.get(), value.nodeIndex,
				std::max(0.001f, value.size.x), std::max(0.001f, value.size.y),
				offset, nullptr, value.trigger);
			break;
		default:
			collider = owner->AddComponent<BoneBoxCollider>(
				attachmentLayerId, model.get(), value.nodeIndex, value.size,
				offset, nullptr, value.trigger);
			break;
		}
		if (collider)
		{
			collider->SetName(value.name);
			collider->SetActive(model->GetColliderInitialActive(colliderIndex));
			attachmentColliders[colliderIndex] = collider;
		}
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

void VMDLModelComponent::LateUpdate()
{
    Actor* actor = dynamic_cast<Actor*>(owner);

    if (!model)
        return;
	UpdateAnimationControls();
	if (!autoUpdateTransform) return;

	const auto& placement = model->GetVmdlPlacementData();
	const Matrix placementTransform =
		Matrix::CreateScale(placement.scale) *
		Matrix::CreateTranslation(model->GetVmdlExtensionData().rootOffset);
	model->UpdateTransform(placementTransform * actor->transform.matrix);
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
	model->ApplyShapeAnimation(animationIndex, time, initialMeshVisibility);
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
	model->RestoreShapeVisibility(initialMeshVisibility);
	animationControlsApplied = false;
}

void VMDLModelComponent::Render(const RenderContext& rc)
{
    if (model)
    {
        Game::Graphics::Instance().GetModelRenderer()->Draw(shaderId, model, paramsWithMaterial);
    }
}

void VMDLModelComponent::SetShaderParamForAllMaterials(const ShaderParam& param)
{
    ModelRenderer::SetShaderParamForAllMaterials(model.get(), param, paramsWithMaterial);
}

void VMDLModelComponent::SetShaderParamForAllMaterials(const ShaderParamList& paramList)
{
	ModelRenderer::SetShaderParamForAllMaterials(model.get(), paramList, paramsWithMaterial);
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
                // paramsWithMaterial にあれば表示
                auto it = paramsWithMaterial.find(material.name);
                if (it != paramsWithMaterial.end())
                {
                    for (ShaderParam& p : it->second)
                        std::visit(ParamGUIVisitor{p.name.c_str()}, p.value);
                }
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}
