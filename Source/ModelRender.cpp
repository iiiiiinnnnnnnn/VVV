// ModelRender.cpp

#include "ModelRender.h"
#include <Graphics.h>

ModelRender::ModelRender(Actor* owner, std::shared_ptr<Model> model, ModelShaderId shaderId) : Component(owner) {
    SetModel(model);
    SetModelShaderId(shaderId);
}

void ModelRender::LateUpdate(float elapsedTime)
{
    if (appendNode) {
        // worldTransformをDecomposeして正規化する
        Matrix world = appendNode->worldTransform;
        Vector3 scale, position;
        Quaternion rotation;
        world.Decompose(scale, rotation, position);

        // スケールを正規化（符号も修正）
        Matrix normalizedWorld = Matrix::CreateFromQuaternion(rotation)
            * Matrix::CreateTranslation(position);

        Matrix finalWorld = owner->transform.matrix * normalizedWorld;
        model->UpdateTransform(finalWorld);
    }
    else {
        model->UpdateTransform(owner->transform.matrix);
    }
}

void ModelRender::Render(const RenderContext& rc, float elapsedTime)
{
    Graphics::Instance().GetModelRenderer()->Draw(shaderId, model);
}

void ModelRender::DrawGUI(float elapsedTime)
{
    if (ImGui::TreeNode("ModelRender"))
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

                    int nodeIndex = static_cast<int>(node - model->GetNodes().data());
                    bool opened = ImGui::TreeNodeEx(node, nodeFlags,
                        ("[" + std::to_string(nodeIndex) + "]"
                            + (meshIndices.empty() ? "" : "{" + meshIndices + "}")
                            + node->name).c_str());

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

            // 再帰的にノードを描画
            drawNodeTree(model->GetRootNode());
        }
        ImGui::TreePop();
    }
}
