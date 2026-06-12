// ModelRenderComponent.cpp

#include "ModelRenderComponent.h"
#include <Graphics.h>
#include "Actor.h"

ModelRenderComponent::ModelRenderComponent(
    Object* owner, std::shared_ptr<Model> model,
    ModelShaderId shaderId, ShaderParamListWithMaterialName paramsWithMaterial)
    : Component(owner), model(model), shaderId(shaderId), paramsWithMaterial(paramsWithMaterial)
{
    // エラー用
    Component::GetOwnerAsActor();

    if (model)
    {
        model->UpdateTransform(Matrix::Identity);
    }
}

void ModelRenderComponent::LateUpdate()
{
    Actor* actor = dynamic_cast<Actor*>(owner);

    if (!model)
        return;

    // アクターの変換行列でモデルを更新
    model->UpdateTransform(actor->transform.matrix);
}

void ModelRenderComponent::Render(const RenderContext& rc)
{
    if (model)
    {
        Game::Graphics::Instance().GetModelRenderer()->Draw(shaderId, model, paramsWithMaterial);
    }
}

void ModelRenderComponent::DrawGUI()
{
    if (ImGui::TreeNode("ModelRenderComponent"))
    {
        if (model)
        {
            // ノードツリーを再帰的に描画する関数
            std::function<void(Model::Node*)> drawNodeTree = [&](Model::Node* node)
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
                    const Model::Mesh& mesh = model->GetMeshes()[i];
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

                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    for (Model::Mesh& mesh : model->GetMeshes())
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
                    for (Model::Node* child : node->children)
                    {
                        drawNodeTree(child);
                    }
                    ImGui::TreePop();
                }
            };

            // すべてのルートノード（親を持たないノード）を起点に描画
            for (Model::Node& node : model->GetNodes())
            {
                if (node.parent == nullptr)
                {
                    drawNodeTree(&node);
                }
            }

            if (ImGui::TreeNode("Materials"))
            {
                for (const Model::Material& material : model->GetMaterials())
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
        ImGui::TreePop();
    }
}