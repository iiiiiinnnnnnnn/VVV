// AnimatorEditor.cpp

#include "Animation/Animator.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <utility>

#include "Application/Tools/Dialog.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;
void Animator::DrawEditor(bool* pOpen)
    {
        if (!pOpen || !*pOpen) return;

        // cpp側からロードされた場合、パスを同期
        if (!GetLastPath().empty() &&
            currentAnimatorPath[0] == '\0')
        {
            strcpy_s(currentAnimatorPath, GetLastPath().c_str());
        }

        ImGui::SetNextWindowSize(ImVec2(1100, 700), ImGuiCond_FirstUseEver);
        const char* windowTitle = IsDynamicMode()
            ? "Animator (Dynamic)"
            : "Animator (Model)";
        if (!ImGui::Begin(windowTitle, pOpen,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::End();
            return;
        }

        // ---- レイヤー タブ -----------------------------------------
        const int layerCount = (int)GetLayerCount();
        if (layerCount == 0)
        {
            ImGui::TextDisabled("No layers.");
            if (ImGui::Button("+ Add Layer"))
                AddLayer("Base Layer", Animator::BlendMode::Override, 1.0f, {});
            ImGui::End();
            return;
        }

        // Toolbar
        if (ImGui::Button("Save"))
        {
            if (currentAnimatorPath[0] == '\0')
            {
                // 未保存なら SaveAs へ
                if (Dialog::SaveFileName(currentAnimatorPath, MAX_PATH,
                    "Animator File\0*.animator\0All Files\0*.*\0\0",
                    "Save Animator", "animator") == DialogResult::OK)
                {
                    Save(currentAnimatorPath);
                }
            }
            else
            {
                Save(currentAnimatorPath);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As"))
        {
            char buf[MAX_PATH] = {};
            strcpy_s(buf, currentAnimatorPath);
            if (Dialog::SaveFileName(buf, MAX_PATH,
                "Animator File\0*.animator\0All Files\0*.*\0\0",
                "Save As Animator", "animator") == DialogResult::OK)
            {
                strcpy_s(currentAnimatorPath, buf);
                Save(currentAnimatorPath);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            char buf[MAX_PATH] = {};
            if (Dialog::OpenFileName(buf, MAX_PATH,
                "Animator File\0*.animator\0All Files\0*.*\0\0",
                "Open Animator") == DialogResult::OK)
            {
                strcpy_s(currentAnimatorPath, buf);
                Load(currentAnimatorPath);
                {
                    editorPositionSet.clear();
                    selectedTransition = {};
                    currentEditorLayer = 0;
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled(currentAnimatorPath[0] ? currentAnimatorPath : "(unsaved)");
        ImGui::SameLine();
        ImGui::TextColored(
            IsDynamicMode()
                ? ImVec4(0.4f, 0.85f, 1.0f, 1.0f)
                : ImVec4(0.6f, 1.0f, 0.6f, 1.0f),
            IsDynamicMode() ? "Dynamic Mode" : "Model Mode");
        ImGui::Separator();

        // ---- レイヤータブ + 管理ボタン ---------------------------------
        // 左にあるレイヤーほど優先度が高い（Updateの評価順）
        // < > ボタンで隣のレイヤーと入れ替え
        if (ImGui::BeginTabBar("Layers"))
        {
            for (int li = 0; li < layerCount; ++li)
            {
                Animator::AnimatorLayer& layer = GetLayer(li);
                char tabLabel[80];
                sprintf_s(tabLabel, "%s##tab%d", layer.name.c_str(), li);
                bool tabOpen = ImGui::BeginTabItem(tabLabel);
                if (tabOpen)
                {
                    currentEditorLayer = li;

                    // 並び替えボタン（タブ内に表示）
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 1));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.6f));

                    bool canLeft  = (li > 0);
                    bool canRight = (li < layerCount - 1);

                    if (!canLeft)  ImGui::BeginDisabled();
                    if (ImGui::SmallButton("<"))
                    {
                        SwapLayers(li, li - 1);
                        // selectedTransition / selectedState の layerIndex を追従
                        if (selectedTransition.layerIndex == li)       selectedTransition.layerIndex = li - 1;
                        else if (selectedTransition.layerIndex == li - 1) selectedTransition.layerIndex = li;
                        if (selectedState.layerIndex == li)       selectedState.layerIndex = li - 1;
                        else if (selectedState.layerIndex == li - 1) selectedState.layerIndex = li;
                    }
                    if (!canLeft)  ImGui::EndDisabled();

                    ImGui::SameLine();

                    if (!canRight) ImGui::BeginDisabled();
                    if (ImGui::SmallButton(">"))
                    {
                        SwapLayers(li, li + 1);
                        if (selectedTransition.layerIndex == li)       selectedTransition.layerIndex = li + 1;
                        else if (selectedTransition.layerIndex == li + 1) selectedTransition.layerIndex = li;
                        if (selectedState.layerIndex == li)       selectedState.layerIndex = li + 1;
                        else if (selectedState.layerIndex == li + 1) selectedState.layerIndex = li;
                    }
                    if (!canRight) ImGui::EndDisabled();

                    ImGui::PopStyleColor();
                    ImGui::PopStyleVar();
                    ImGui::SameLine();
                    ImGui::TextDisabled("Layer %d (left = higher priority)", li);
                    ImGui::Separator();

                    DrawLayerEditor(layer, li);
                    ImGui::EndTabItem();
                }
            }

            // + ボタンでレイヤー追加
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
                AddLayer("New Layer", Animator::BlendMode::Override, 1.0f, {});

            ImGui::EndTabBar();
        }

        // ---- レイヤー追加モーダル ---------------------------------------
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("AddLayerModal", nullptr,
            ImGuiWindowFlags_NoResize))
        {
            ImGui::Text("Layer Name:");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##layername", addLayerName, sizeof(addLayerName));

            ImGui::Spacing();
            if (!IsDynamicMode())
            {
                ImGui::Text("Bone Mask (unchecked = all bones):");
                ImGui::Separator();

                const auto& nodes = GetModel()->GetNodes();
                ImGui::BeginChild("BoneList", ImVec2(0, 300), true);
                for (int ni = 0; ni < (int)nodes.size(); ++ni)
                {
                    if (ni >= (int)maskSelection.size())
                        maskSelection.resize(ni + 1, false);
                    ImGui::PushID(ni);
                    bool bsel = maskSelection[ni];
                    if (ImGui::Checkbox(nodes[ni].name.c_str(), &bsel))
                        maskSelection[ni] = bsel;
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            else
            {
                ImGui::TextDisabled("Dynamic layers animate Widget and ShaderParam values.");
                ImGui::Dummy(ImVec2(0.0f, 300.0f));
            }

            ImGui::Spacing();
            if (ImGui::Button("Add", ImVec2(120, 0)))
            {
                Animator::AvatarMask mask;
                if (!IsDynamicMode())
                {
                    for (int ni = 0; ni < (int)maskSelection.size(); ++ni)
                        if (maskSelection[ni]) mask.nodes.push_back(ni);
                }
                AddLayer(addLayerName,
                                   Animator::BlendMode::Override, 1.0f, mask);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::End();
    }


ed::PinId Animator::OutPin(int li, int si)
    {
        return ed::PinId(100000 + li * 10000 + si * 2 + 0);  // ベース100000を追加
    }
ed::PinId Animator::InPin(int li, int si)
    {
        return ed::PinId(100000 + li * 10000 + si * 2 + 1);  // ベース100000を追加
    }
ed::NodeId Animator::NodeId(int li, int si)
    {
        return ed::NodeId(1 + li * 1000 + si);  // 1-based
    }
ed::LinkId Animator::LinkId(int li, int from, int ti)
    {
        constexpr int LinkBase = 500000;
        constexpr int LinkLayerStride = 2000000;
        constexpr int LinkFromStride = 1000;

        return ed::LinkId(
            LinkBase +
            li * LinkLayerStride +
            from * LinkFromStride +
            ti);
    }

    // -------------------------------------------------------------------
    // レイヤーエディタ本体
    // -------------------------------------------------------------------
void Animator::DrawLayerEditor(Animator::AnimatorLayer& layer, int li)
    {
        suppressNodeEditorInteractions = false;

        ImGui::BeginChild("LeftPanel", ImVec2(editorLeftPanelWidth, 0), true);

        ImGui::PushID(li);
        if (selectedTransition.layerIndex == li)
            DrawTransitionDetail(layer);
        else if (selectedState.layerIndex == li)
            DrawStateDetail(layer, li);
        else
        {
            DrawLayerSettings(layer, li);
            ImGui::Separator();
            DrawParameterPanel();
        }
        ImGui::PopID();

        ImGui::EndChild();

        // スプリッタ（ドラッグで左パネル幅を変更）
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Button("##splitter", ImVec2(4.0f, -1.0f));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemActive())
        {
            editorLeftPanelWidth += ImGui::GetIO().MouseDelta.x;
            editorLeftPanelWidth = std::clamp(editorLeftPanelWidth, 150.0f, 500.0f);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::SameLine();

        ed::SetCurrentEditor(editorContext);
        ed::Begin("NodeEditor", ImVec2(0, 0));
        editorCanvasMousePosition = ImGui::GetMousePos();
        if (deleteNodeId)
        {
            ed::DeleteNode(deleteNodeId);
            deleteNodeId = ed::NodeId();
        }
        if (deleteLinkId)
        {
            ed::DeleteLink(deleteLinkId);
            deleteLinkId = ed::LinkId();
        }
        DrawNodes(layer, li);
        DrawLinks(layer, li);
        if (!suppressNodeEditorInteractions)
        {
            HandleInteractions(layer, li);
        }
        ed::End();
        ed::SetCurrentEditor(nullptr);
    }

    // -------------------------------------------------------------------
    // ノード描画
    // -------------------------------------------------------------------
void Animator::DrawNodes(Animator::AnimatorLayer& layer, int li)
    {
        // 配置待ちノードがあれば今フレームで位置をセット
        if (pendingNodePlacement && pendingNodeStateIndex >= 0 &&
            pendingNodeStateIndex < (int)layer.states.size())
        {
            ax::NodeEditor::NodeId nid = NodeId(li, pendingNodeStateIndex);
            editorPositionSet[(int)nid.Get()] = true;
            ed::SetNodePosition(nid, pendingNodePosition);
            layer.states[pendingNodeStateIndex].hasEditorPosition = true;
            layer.states[pendingNodeStateIndex].editorPosX = pendingNodePosition.x;
            layer.states[pendingNodeStateIndex].editorPosY = pendingNodePosition.y;
            pendingNodePlacement = false;
            pendingNodeStateIndex = -1;
        }

        // ---- AnyState ノードを先に表示 ----
        {
            ax::NodeEditor::NodeId nid = NodeId(li, ANY_STATE_INDEX);
            int nidInt = (int)nid.Get();
            if (editorPositionSet.find(nidInt) == editorPositionSet.end())
            {
                editorPositionSet[nidInt] = true;
                if (layer.hasAnyStateEditorPosition)
                {
                    ax::NodeEditor::SetNodePosition(nid,
                                                    ImVec2(layer.anyStateEditorPosX, layer.anyStateEditorPosY));
                }
                else
                {
                    ax::NodeEditor::SetNodePosition(nid, ImVec2(60.0f, 100.0f));
                    layer.hasAnyStateEditorPosition = true;
                    layer.anyStateEditorPosX = 60.0f;
                    layer.anyStateEditorPosY = 100.0f;
                }
            }

            // AnyState を目立たせる色
            ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBorder,
                                           ImVec4(0.8f, 0.4f, 0.1f, 1.0f));

            ax::NodeEditor::BeginNode(nid);
            ImGui::PushID(li * 10000 + ANY_STATE_INDEX);

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
            ImGui::TextUnformatted("AnyState");
            ImGui::PopStyleColor();

            ImGui::TextDisabled("From any state");

            // 出力ピンのみ
            ax::NodeEditor::BeginPin(OutPin(li, ANY_STATE_INDEX), ax::NodeEditor::PinKind::Output);
            ImGui::PushID((int)OutPin(li, ANY_STATE_INDEX).Get());
            ImGui::TextUnformatted("A");
            ImGui::PopID();
            ax::NodeEditor::EndPin();

            ImGui::PopID();
            ax::NodeEditor::EndNode();

            ax::NodeEditor::PopStyleColor();

            // AnyState ノード選択時の挙動：左パネルで AnyState トランジション一覧が見えるよう
            if (ed::IsNodeSelected(nid) && !ImGui::IsMouseDragging(0))
            {
                // 選択状態は左パネルの AnyState トランジション一覧で扱う。
                selectedState = {};
                selectedTransition = {};
            }

            ImVec2 anyPos = ed::GetNodePosition(nid);
            layer.hasAnyStateEditorPosition = true;
            layer.anyStateEditorPosX = anyPos.x;
            layer.anyStateEditorPosY = anyPos.y;
        }

        for (int si = 0; si < (int)layer.states.size(); ++si)
        {
            Animator::State& state = layer.states[si];
            ax::NodeEditor::NodeId nid = NodeId(li, si);

            int nidInt = (int)nid.Get();
            if (editorPositionSet.find(nidInt) == editorPositionSet.end())
            {
                editorPositionSet[nidInt] = true;
                if (state.hasEditorPosition)
                {
                    ax::NodeEditor::SetNodePosition(nid, ImVec2(state.editorPosX, state.editorPosY));
                }
                else
                {
                    const ImVec2 defaultPos(200.0f + si * 180.0f, 100.0f + (si % 3) * 130.0f);
                    ax::NodeEditor::SetNodePosition(nid, defaultPos);
                    state.hasEditorPosition = true;
                    state.editorPosX = defaultPos.x;
                    state.editorPosY = defaultPos.y;
                }
            }

            bool isCurrent = (layer.currentStateIndex == si);
            bool isNext = (layer.isTransitioning && layer.nextStateIndex == si);

            if (isCurrent)
            {
                ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.06f, 0.28f, 0.10f, 0.96f));
                ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.2f, 0.85f, 0.3f, 1.0f));
                ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 5.0f);
            }
            else if (isNext)
            {
                ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
            }

            ax::NodeEditor::BeginNode(nid);

            // ノード全体を li,si でスコープ
            ImGui::PushID(li * 10000 + si);

            ImGui::TextUnformatted(state.name.c_str());
            if (isCurrent)
                ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.35f, 1.0f), ">> RUNNING <<");

            const std::string animationName =
                GetStateAnimationName(state);
            ImGui::TextDisabled("%s", animationName.c_str());

            if (isCurrent)
            {
                const float length = GetStateLength(state);
                if (length > 0.0f)
                {
                    const float progress = std::clamp(
                        layer.currentTime / length,
                        0.0f,
                        1.0f);
                    char progressId[32];
                    sprintf_s(progressId, "##pb%d_%d", li, si);
                    ImGui::ProgressBar(
                        progress,
                        ImVec2(160.0f, 5.0f),
                        progressId);
                }
            }

            // 入力ピン
            ax::NodeEditor::BeginPin(InPin(li, si), ax::NodeEditor::PinKind::Input);
            ImGui::PushID((int)InPin(li, si).Get());
            ImGui::TextUnformatted(">");
            ImGui::PopID();
            ax::NodeEditor::EndPin();

            ImGui::SameLine(140.0f);

            // 出力ピン
            ax::NodeEditor::BeginPin(OutPin(li, si), ax::NodeEditor::PinKind::Output);
            ImGui::PushID((int)OutPin(li, si).Get());
            ImGui::TextUnformatted("o");
            ImGui::PopID();
            ax::NodeEditor::EndPin();

            ImGui::PopID();
            ax::NodeEditor::EndNode();

            if (isCurrent)
            {
                ed::PopStyleVar();
                ed::PopStyleColor(2);
            }
            else if (isNext)
            {
                ed::PopStyleColor();
            }

            if (ed::IsNodeSelected(nid) && !ImGui::IsMouseDragging(0))
            {
                selectedState = { li, si };
                selectedTransition = {};
            }

            ImVec2 pos = ed::GetNodePosition(nid);
            state.hasEditorPosition = true;
            state.editorPosX = pos.x;
            state.editorPosY = pos.y;
        }
    }

    // -------------------------------------------------------------------
    // リンク描画
    // -------------------------------------------------------------------
void Animator::DrawLinks(Animator::AnimatorLayer& layer, int li)
    {
        for (int si = 0; si < (int)layer.states.size(); ++si)
        {
            const Animator::State& state = layer.states[si];
            for (int ti = 0; ti < (int)state.transitions.size(); ++ti)
            {
                const Animator::Transition& tr = state.transitions[ti];
                if (tr.toStateIndex < 0 ||
                    tr.toStateIndex >= (int)layer.states.size()) continue;

                // 選択中は色を変える
                bool isSelected =
                    (selectedTransition.layerIndex == li &&
                     selectedTransition.fromStateIndex == si &&
                     selectedTransition.transIndex == ti);

                ImVec4 col = isSelected
                    ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f)
                    : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

                ed::Link(LinkId(li, si, ti),
                         OutPin(li, si),
                         InPin(li, tr.toStateIndex),
                         col, 2.0f);
            }
        }

        // AnyState からの遷移を描画
        for (int ti = 0; ti < (int)layer.anyStateTransitions.size(); ++ti)
        {
            const Animator::Transition& tr = layer.anyStateTransitions[ti];
            if (tr.toStateIndex < 0 ||
                tr.toStateIndex >= (int)layer.states.size()) continue;

            bool isSelected =
                (selectedTransition.layerIndex == li &&
                 selectedTransition.fromStateIndex == ANY_STATE_INDEX &&
                 selectedTransition.transIndex == ti);

            ImVec4 col = isSelected
                ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f)
                : ImVec4(0.9f, 0.6f, 0.3f, 1.0f); // AnyState 色

            ed::Link(LinkId(li, ANY_STATE_INDEX, ti),
                     OutPin(li, ANY_STATE_INDEX),
                     InPin(li, tr.toStateIndex),
                     col, 2.0f);
        }
    }

    // -------------------------------------------------------------------
    // インタラクション (リンク作成 / クリック選択 / 削除)
    // -------------------------------------------------------------------
void Animator::HandleInteractions(Animator::AnimatorLayer& layer, int li)
    {
        // ---- 新規リンク (ドラッグ&ドロップ) --------------------------
        if (ed::BeginCreate())
        {
            ed::PinId startPin, endPin;
            if (ed::QueryNewLink(&startPin, &endPin))
            {
                if (startPin && endPin && startPin != endPin)
                {
                    // ピンIDから stateIndex を逆引き
                    int fromSi = DecodeOutPin(li, startPin);
                    int toSi = DecodeInPin(li, endPin);

                    if (fromSi >= 0 && toSi >= 0 && fromSi != toSi)
                    {
                        // AnyState からの遷移か通常遷移かで処理を分ける
                        bool exists = false;
                        if (fromSi == ANY_STATE_INDEX)
                        {
                            for (const auto& t : layer.anyStateTransitions)
                                if (t.toStateIndex == toSi) { exists = true; break; }

                            if (!exists && ed::AcceptNewItem(ImVec4(0.2f, 0.8f, 0.2f, 1), 2.0f))
                            {
                                // AnyState 遷移追加
                                AddAnyStateTransition(li, toSi,
                                                                0.1f, false, 1.0f, 0, false);
                                selectedTransition = { li, ANY_STATE_INDEX,
                                    (int)layer.anyStateTransitions.size() - 1 };
                            }
                        }
                        else
                        {
                            for (const auto& t : layer.states[fromSi].transitions)
                                if (t.toStateIndex == toSi) { exists = true; break; }

                            if (!exists && ed::AcceptNewItem(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), 2.0f))
                            {
                                // 通常遷移追加
                                AddTransition(li, fromSi, toSi,
                                                        0.1f, false, 1.0f, 0, false);
                                selectedTransition = { li, fromSi,
                                    (int)layer.states[fromSi].transitions.size() - 1 };
                            }
                        }
                    }
                    else
                    {
                        ed::RejectNewItem(ImVec4(1, 0, 0, 1), 2.0f);
                    }
                }
            }
        }
        ed::EndCreate();

        // ---- 削除 -----------------------------------------------------
        if (ed::BeginDelete())
        {
            ed::LinkId delLink;
            while (ed::QueryDeletedLink(&delLink))
            {
                if (ed::AcceptDeletedItem())
                {
                    int dli, dfrom, dti;
                    DecodeLinkId(delLink, dli, dfrom, dti);
                    if (dli == li)
                    {
                        if (dfrom == ANY_STATE_INDEX)
                        {
                            if (dti < (int)layer.anyStateTransitions.size())
                            {
                                layer.anyStateTransitions.erase(
                                    layer.anyStateTransitions.begin() + dti);
                                if (selectedTransition.layerIndex == li &&
                                    selectedTransition.fromStateIndex == dfrom &&
                                    selectedTransition.transIndex == dti)
                                    selectedTransition = {};
                            }
                        }
                        else if (dfrom < (int)layer.states.size() &&
                                 dti < (int)layer.states[dfrom].transitions.size())
                        {
                            layer.states[dfrom].transitions.erase(
                                layer.states[dfrom].transitions.begin() + dti);
                            if (selectedTransition.layerIndex == li &&
                                selectedTransition.fromStateIndex == dfrom &&
                                selectedTransition.transIndex == dti)
                                selectedTransition = {};
                        }
                    }
                }
            }

            ed::NodeId delNode;
            while (ed::QueryDeletedNode(&delNode))
            {
                if (ed::AcceptDeletedItem())
                {
                    // NodeId から stateIndex を逆算
                    int nv = (int)delNode.Get() - 1 - li * 1000;
                    if (nv >= 0 && nv < (int)layer.states.size())
                    {
                        // このステートへの/からの遷移を全削除
                        layer.states.erase(layer.states.begin() + nv);
                        for (auto& s : layer.states)
                        {
                            s.transitions.erase(
                                std::remove_if(s.transitions.begin(), s.transitions.end(),
                                [nv](const Animator::Transition& t) {
                                return t.toStateIndex == nv;
                            }),
                                s.transitions.end());
                            // toStateIndex の番号を詰める
                            for (auto& t : s.transitions)
                                if (t.toStateIndex > nv) --t.toStateIndex;
                        }
                        // AnyState 側の toStateIndex も詰める
                        layer.anyStateTransitions.erase(
                            std::remove_if(layer.anyStateTransitions.begin(), layer.anyStateTransitions.end(),
                            [nv](const Animator::Transition& t) {
                            return t.toStateIndex == nv;
                        }),
                            layer.anyStateTransitions.end());
                        for (auto& t : layer.anyStateTransitions)
                            if (t.toStateIndex > nv) --t.toStateIndex;

                        // currentState も補正
                        if (layer.currentStateIndex == nv) layer.currentStateIndex = 0;
                        else if (layer.currentStateIndex > nv) --layer.currentStateIndex;
                        editorPositionSet.clear();
                        selectedTransition = {};
                    }
                }
            }
        }
        ed::EndDelete();

        bool openContextMenu = false;
        ed::NodeId contextNodeId;  // 将来のノードメニュー用

        if (ed::ShowBackgroundContextMenu())
        {
            openContextMenu = true;
            editorContextMenuPosition = editorCanvasMousePosition;
        }
        // ノード・リンクのコンテキストメニューは現在未使用

        ed::Suspend();
        if (openContextMenu) ImGui::OpenPopup("BackgroundContextMenu");

        if (ImGui::BeginPopup("BackgroundContextMenu"))
        {
            if (ImGui::MenuItem("+ State"))
            {
                pendingNodePlacement = true;
                pendingNodeStateIndex = IsDynamicMode()
                    ? AddDynamicState(li, "New State", "", true, 1.0f)
                    : AddState(li, "New State", 0, true, 1.0f);
                pendingNodePosition = editorContextMenuPosition;
            }
            ImGui::EndPopup();
        }

        // NodeContextMenu / LinkContextMenu は現在メニュー項目なし → 表示しない
        ed::Resume();

        // ---- リンクシングルクリック → 詳細パネルに表示 ---------------
        if (ed::IsBackgroundClicked())
        {
            selectedTransition = {};
            selectedState = {};
        }

        // ダブルクリックしたリンクを選択
        ed::LinkId clickedLink = ed::GetDoubleClickedLink();
        if (clickedLink ||
            (ImGui::IsMouseClicked(0) &&
            ed::GetHoveredLink().Get() != 0 &&
            (clickedLink = ed::GetHoveredLink(), true)))
        {
            int dli, dfrom, dti;
            DecodeLinkId(clickedLink, dli, dfrom, dti);
            if (dli == li)
                selectedTransition = { li, dfrom, dti };
        }
    }

    // -------------------------------------------------------------------
    // パラメータパネル
    // -------------------------------------------------------------------
void Animator::DrawParameterPanel()
    {
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "Parameters");

        // --- 追加UI ---
        static char newParamName[64] = "";
        static int  newParamType = 0; // 0=Float 1=Int 2=Bool 3=Trigger
        const char* typeLabels[] = { "Float", "Int", "Bool", "Trigger" };

        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputText("##newname", newParamName, sizeof(newParamName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::Combo("##newtype", &newParamType, typeLabels, 4);
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Add") && newParamName[0] != '\0')
        {
            std::string n(newParamName);
            switch (newParamType)
            {
                case 0: AddFloat(n);   break;
                case 1: AddInt(n);     break;
                case 2: AddBool(n);    break;
                case 3: AddTrigger(n); break;
            }
            newParamName[0] = '\0';
        }

        ImGui::Separator();

        // --- パラメータ一覧（削除ボタン付き）---
        const auto& params   = GetParameters();
        const auto& triggers = GetTriggers();

        std::string toDelete;

        for (const auto& [name, val] : params)
        {
            ImGui::PushID(name.c_str());

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("x")) toDelete = name;
            ImGui::PopStyleColor();
            ImGui::SameLine();

            if (std::holds_alternative<float>(val))
            {
                float v = std::get<float>(val);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::DragFloat("##v", &v, 0.01f))
                    SetFloat(name, v);
                ImGui::SameLine();
                ImGui::TextDisabled("[F]"); ImGui::SameLine();
                ImGui::TextUnformatted(name.c_str());
            }
            else if (std::holds_alternative<int>(val))
            {
                int v = std::get<int>(val);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::DragInt("##v", &v))
                    SetInt(name, v);
                ImGui::SameLine();
                ImGui::TextDisabled("[I]"); ImGui::SameLine();
                ImGui::TextUnformatted(name.c_str());
            }
            else if (std::holds_alternative<bool>(val))
            {
                bool v = std::get<bool>(val);
                if (ImGui::Checkbox("##v", &v))
                    SetBool(name, v);
                ImGui::SameLine();
                ImGui::TextDisabled("[B]"); ImGui::SameLine();
                ImGui::TextUnformatted(name.c_str());
            }
            ImGui::PopID();
        }

        if (!toDelete.empty())
            GetParameters_Mutable().erase(toDelete);

        std::string toDeleteTrigger;

        for (const auto& [name, fired] : triggers)
        {
            ImGui::PushID(("trigger_" + name).c_str());

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
            if (ImGui::SmallButton("x")) toDeleteTrigger = name;
            ImGui::PopStyleColor();
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button,
                                  fired ? ImVec4(0.8f, 0.3f, 0.1f, 1.0f)
                                  : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(name.c_str(), ImVec2(80, 0)))
                SetTrigger(name);
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextDisabled("[T]");

            ImGui::PopID();
        }

        if (!toDeleteTrigger.empty())
            GetTriggers_Mutable().erase(toDeleteTrigger);
    }

    // -------------------------------------------------------------------
    // レイヤー設定パネル（常時表示）
    // -------------------------------------------------------------------
void Animator::DrawLayerSettings(Animator::AnimatorLayer& layer, int li)
    {
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Layer Settings");

        // レイヤー名
        char nameBuf[64];
        strncpy_s(nameBuf, layer.name.c_str(), sizeof(nameBuf));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##layername", nameBuf, sizeof(nameBuf)))
            layer.name = nameBuf;

        if (!IsDynamicMode())
        {
            ImGui::TextDisabled("Weight");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::DragFloat("##weight", &layer.weight, 0.01f, 0.0f, 1.0f, "%.2f");

            ImGui::TextDisabled("Blend Mode");
            const char* blendModes[] = { "Override", "Additive" };
            int blendMode = static_cast<int>(layer.blendMode);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##blendmode", &blendMode, blendModes, 2))
                layer.blendMode = static_cast<Animator::BlendMode>(blendMode);
        }
        else
        {
            ImGui::TextDisabled("Dynamic layers are applied in layer order.");
        }

        ImGui::Spacing();
        if (!IsDynamicMode())
        {
            ImGui::TextDisabled("Bone Mask");
            const auto& nodes = GetModel()->GetNodes();
            ImGui::BeginChild("BoneMask", ImVec2(0, 120), true);
            for (int ni = 0; ni < (int)nodes.size(); ++ni)
            {
                ImGui::PushID(ni);
                bool inMask = layer.mask.Contains(ni) && !layer.mask.nodes.empty();
                if (ImGui::Checkbox(nodes[ni].name.c_str(), &inMask))
                {
                    if (inMask)
                    {
                        if (std::find(layer.mask.nodes.begin(), layer.mask.nodes.end(), ni)
                            == layer.mask.nodes.end())
                        {
                            layer.mask.nodes.push_back(ni);
                        }
                    }
                    else
                    {
                        layer.mask.nodes.erase(
                            std::remove(layer.mask.nodes.begin(), layer.mask.nodes.end(), ni),
                            layer.mask.nodes.end());
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        else
        {
            ImGui::TextDisabled(
                "Dynamic Mode: later layers overwrite the same Widget/ShaderParam target.");
        }

        // ---- AnyState トランジション一覧（編集可能） ----
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1), "AnyState Transitions");

        int anyDragFrom = -1;
        int anyDragTo = -1;
        int anyDeleteIndex = -1;

        const ImGuiTableFlags anyTransitionTableFlags =
            ImGuiTableFlags_SizingStretchProp |
            ImGuiTableFlags_NoSavedSettings |
            ImGuiTableFlags_BordersInnerV;

        if (ImGui::BeginTable(
            "AnyStateTransitionsTable",
            3,
            anyTransitionTableFlags))
        {
            const float rowHeight = ImGui::GetFrameHeight();

            ImGui::TableSetupColumn(
                "##AnyStateDragHandle",
                ImGuiTableColumnFlags_WidthFixed,
                rowHeight);
            ImGui::TableSetupColumn(
                "Transition",
                ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(
                "##AnyStateDelete",
                ImGuiTableColumnFlags_WidthFixed,
                rowHeight);

            for (int ti = 0;
                 ti < static_cast<int>(layer.anyStateTransitions.size());
                 ++ti)
            {
                const Animator::Transition& transition =
                    layer.anyStateTransitions[ti];

                const std::string& toName =
                    transition.toStateIndex >= 0 &&
                    transition.toStateIndex < static_cast<int>(layer.states.size())
                    ? layer.states[transition.toStateIndex].name
                    : "???";

                ImGui::PushID(1000 + ti);
                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
                ImGui::Button("::##handle", ImVec2(rowHeight, rowHeight));
                ImGui::PopStyleColor(2);

                if (ImGui::BeginDragDropSource(
                    ImGuiDragDropFlags_SourceNoPreviewTooltip))
                {
                    ImGui::SetDragDropPayload(
                        "ANYTRANS_REORDER",
                        &ti,
                        sizeof(int));
                    ImGui::EndDragDropSource();
                }

                ImGui::TableSetColumnIndex(1);

                const bool isSelected =
                    selectedTransition.layerIndex == li &&
                    selectedTransition.fromStateIndex == ANY_STATE_INDEX &&
                    selectedTransition.transIndex == ti;

                char label[128];
                snprintf(
                    label,
                    sizeof(label),
                    "%d. -> %s",
                    ti + 1,
                    toName.c_str());

                const ImVec2 itemPos = ImGui::GetCursorScreenPos();
                const float itemWidth = ImGui::GetContentRegionAvail().x;
                ImGui::InvisibleButton(
                    "##AnyStateTransitionSelect",
                    ImVec2(itemWidth, rowHeight));

                const bool isHovered = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked())
                {
                    selectedTransition = { li, ANY_STATE_INDEX, ti };
                    selectedState = {};
                }

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (isSelected || isHovered)
                {
                    const ImU32 bgColor = ImGui::GetColorU32(
                        isSelected ? ImGuiCol_Header : ImGuiCol_HeaderHovered);
                    drawList->AddRectFilled(
                        itemPos,
                        ImVec2(itemPos.x + itemWidth, itemPos.y + rowHeight),
                        bgColor);
                }

                const ImU32 textColor = ImGui::GetColorU32(
                    isSelected
                        ? ImVec4(1.0f, 0.8f, 0.2f, 1.0f)
                        : ImGui::GetStyleColorVec4(ImGuiCol_Text));
                const float textY =
                    itemPos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f;

                drawList->PushClipRect(
                    itemPos,
                    ImVec2(itemPos.x + itemWidth, itemPos.y + rowHeight),
                    true);
                drawList->AddText(
                    ImVec2(itemPos.x + 4.0f, textY),
                    textColor,
                    label);
                drawList->PopClipRect();

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("ANYTRANS_REORDER"))
                    {
                        anyDragFrom = *static_cast<const int*>(payload->Data);
                        anyDragTo = ti;
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::PushStyleColor(
                    ImGuiCol_Button,
                    ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(
                    ImGuiCol_ButtonHovered,
                    ImVec4(0.75f, 0.20f, 0.20f, 1.0f));

                if (ImGui::Button("x", ImVec2(rowHeight, rowHeight)))
                {
                    anyDeleteIndex = ti;
                    suppressNodeEditorInteractions = true;
                }

                ImGui::PopStyleColor(2);
                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        if (anyDeleteIndex >= 0 &&
            anyDeleteIndex < static_cast<int>(layer.anyStateTransitions.size()))
        {
            layer.anyStateTransitions.erase(
                layer.anyStateTransitions.begin() + anyDeleteIndex);

            if (selectedTransition.layerIndex == li &&
                selectedTransition.fromStateIndex == ANY_STATE_INDEX)
            {
                if (selectedTransition.transIndex == anyDeleteIndex)
                {
                    selectedTransition = {};
                }
                else if (selectedTransition.transIndex > anyDeleteIndex)
                {
                    --selectedTransition.transIndex;
                }
            }

            for (int i = 0;
                 i < static_cast<int>(layer.anyStateTransitions.size());
                 ++i)
            {
                layer.anyStateTransitions[i].priority =
                    static_cast<int>(layer.anyStateTransitions.size()) - 1 - i;
            }

            anyDragFrom = -1;
            anyDragTo = -1;
        }

        if (anyDragFrom >= 0 &&
            anyDragTo >= 0 &&
            anyDragFrom != anyDragTo &&
            anyDragFrom < static_cast<int>(layer.anyStateTransitions.size()) &&
            anyDragTo < static_cast<int>(layer.anyStateTransitions.size()))
        {
            Animator::Transition moved =
                layer.anyStateTransitions[anyDragFrom];

            layer.anyStateTransitions.erase(
                layer.anyStateTransitions.begin() + anyDragFrom);

            int insertAt = anyDragTo;
            if (insertAt > static_cast<int>(layer.anyStateTransitions.size()))
            {
                insertAt = static_cast<int>(layer.anyStateTransitions.size());
            }

            layer.anyStateTransitions.insert(
                layer.anyStateTransitions.begin() + insertAt,
                moved);

            for (int i = 0;
                 i < static_cast<int>(layer.anyStateTransitions.size());
                 ++i)
            {
                layer.anyStateTransitions[i].priority =
                    static_cast<int>(layer.anyStateTransitions.size()) - 1 - i;
            }

            if (selectedTransition.layerIndex == li &&
                selectedTransition.fromStateIndex == ANY_STATE_INDEX)
            {
                const int selectedIndex = selectedTransition.transIndex;

                if (selectedIndex == anyDragFrom)
                {
                    selectedTransition.transIndex = insertAt;
                }
                else if (anyDragFrom < anyDragTo &&
                         selectedIndex > anyDragFrom &&
                         selectedIndex <= anyDragTo)
                {
                    selectedTransition.transIndex = selectedIndex - 1;
                }
                else if (anyDragFrom > anyDragTo &&
                         selectedIndex >= anyDragTo &&
                         selectedIndex < anyDragFrom)
                {
                    selectedTransition.transIndex = selectedIndex + 1;
                }
            }
        }

        // 追加ボタン（ターゲットを選んで追加）
        ImGui::Spacing();
        if (ImGui::Button("+ AnyState Transition", ImVec2(160, 0)))
        {
            if (!layer.states.empty())
            {
                // デフォルトで最初のステートへ遷移を追加
                AddAnyStateTransition(li, 0, 0.1f, false, 1.0f, 0, false);
            }
        }

        // レイヤー削除（2つ以上あるとき）
        if (GetLayerCount() > 1)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Delete Layer", ImVec2(-1, 0)))
                RemoveLayer(li);
            ImGui::PopStyleColor();
        }
    }

    // -------------------------------------------------------------------
    // Foot IK レンジ編集
    // -------------------------------------------------------------------
void Animator::DrawFootIKRangeEditor(Animator::State& state)
    {
        if (IsDynamicMode()) return;

        std::shared_ptr<Model> model = GetModel();
        const float animationLength = GetStateLength(state);
        const std::string animationName = GetStateAnimationName(state);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Foot IK Ranges");
        ImGui::TextDisabled("Animation: %s  Length: %.3f sec",
            animationName.c_str(),
            animationLength);

        if (ImGui::Button("+ Add Foot IK Range", ImVec2(-1.0f, 0.0f)))
        {
            Animator::FootIKRange range;
            range.name = "FootIK";
            range.targetName = "All";
            range.startRatio = 0.0f;
            range.endRatio = 1.0f;
            range.weight = 1.0f;
            range.fadeInRatio = 0.03f;
            range.fadeOutRatio = 0.03f;
            state.footIKRanges.push_back(range);
        }

        int removeIndex = -1;
        for (int rangeIndex = 0;
            rangeIndex < static_cast<int>(state.footIKRanges.size());
            ++rangeIndex)
        {
            Animator::FootIKRange& range = state.footIKRanges[rangeIndex];
            ImGui::PushID(rangeIndex);

            const float startSeconds = range.startRatio * animationLength;
            const float endSeconds = range.endRatio * animationLength;

            char header[160];
            sprintf_s(
                header,
                "%d: %s  target=%s  %.2f-%.2f (%.2fs-%.2fs)  w=%.2f",
                rangeIndex,
                range.name.c_str(),
                range.targetName.c_str(),
                range.startRatio,
                range.endRatio,
                startSeconds,
                endSeconds,
                range.weight);

            const bool nodeOpen = ImGui::TreeNode("[ TREE ]");
            ImGui::SameLine();
            ImGui::Text("%s", header);

            if (nodeOpen)
            {
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::InputText("Name", &range.name);

                const char* currentTargetName = range.targetName.empty() ? "All" : range.targetName.c_str();
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("Target", currentTargetName))
                {
                    const bool allSelected = range.targetName == "All" || range.targetName.empty();
                    if (ImGui::Selectable("All", allSelected))
                    {
                        range.targetName = "All";
                    }
                    if (allSelected) ImGui::SetItemDefaultFocus();

                    if (model)
                    {
                        const auto& nodes = model->GetNodes();
                        for (int nodeIndex = 0; nodeIndex < static_cast<int>(nodes.size()); ++nodeIndex)
                        {
                            const std::string& nodeName = nodes[nodeIndex].name;
                            if (nodeName.empty()) continue;

                            const bool selected = range.targetName == nodeName;
                            if (ImGui::Selectable(nodeName.c_str(), selected))
                            {
                                range.targetName = nodeName;
                            }
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                float ratioRange[2] =
                {
                    range.startRatio,
                    range.endRatio
                };

                if (ImGui::DragFloat2(
                    "Ratio",
                    ratioRange,
                    0.001f,
                    0.0f,
                    1.0f,
                    "%.3f"))
                {
                    range.startRatio = ratioRange[0];
                    range.endRatio = ratioRange[1];

                    if (range.endRatio < range.startRatio)
                    {
                        const float temp = range.startRatio;
                        range.startRatio = range.endRatio;
                        range.endRatio = temp;
                    }
                }

                ImGui::TextDisabled(
                    "Seconds: %.3f - %.3f",
                    range.startRatio * animationLength,
                    range.endRatio * animationLength);

                ImGui::SliderFloat("Weight", &range.weight, 0.0f, 1.0f, "%.2f");
                ImGui::DragFloat("Fade In Ratio", &range.fadeInRatio, 0.001f, 0.0f, 1.0f, "%.3f");
                ImGui::DragFloat("Fade Out Ratio", &range.fadeOutRatio, 0.001f, 0.0f, 1.0f, "%.3f");

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
                if (ImGui::Button("Delete Range", ImVec2(-1.0f, 0.0f)))
                    removeIndex = rangeIndex;
                ImGui::PopStyleColor();

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (removeIndex >= 0)
        {
            state.footIKRanges.erase(
                state.footIKRanges.begin() + removeIndex);
        }

    }

void Animator::DrawAnimationKeyEditor(Animator::State& state)
    {
        if (IsDynamicMode()) return;

        std::shared_ptr<Model> model = GetModel();
        if (!model) return;
        if (state.animationIndex < 0) return;
        if (state.animationIndex >= static_cast<int>(model->GetAnimations().size())) return;

        Model::Animation& animation =
            model->GetAnimations()[state.animationIndex];
        if (animation.nodeAnims.empty()) return;

        const auto& nodes = model->GetNodes();
        if (keyEditorNodeIndex < 0) keyEditorNodeIndex = 0;
        if (keyEditorNodeIndex >= static_cast<int>(nodes.size()))
            keyEditorNodeIndex = static_cast<int>(nodes.size()) - 1;
        if (keyEditorNodeIndex >= static_cast<int>(animation.nodeAnims.size()))
            keyEditorNodeIndex = static_cast<int>(animation.nodeAnims.size()) - 1;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "Animation Keys");

        const char* currentNodeName =
            (keyEditorNodeIndex >= 0 &&
             keyEditorNodeIndex < static_cast<int>(nodes.size()))
            ? nodes[keyEditorNodeIndex].name.c_str()
            : "(none)";

        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("Node", currentNodeName))
        {
            int count = static_cast<int>(nodes.size());
            if (count > static_cast<int>(animation.nodeAnims.size()))
                count = static_cast<int>(animation.nodeAnims.size());
            for (int nodeIndex = 0; nodeIndex < count; ++nodeIndex)
            {
                const bool selected = keyEditorNodeIndex == nodeIndex;
                if (ImGui::Selectable(nodes[nodeIndex].name.c_str(), selected))
                    keyEditorNodeIndex = nodeIndex;
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (keyEditorNodeIndex < 0 ||
            keyEditorNodeIndex >= static_cast<int>(animation.nodeAnims.size()))
        {
            return;
        }

        Model::NodeAnim& nodeAnim = animation.nodeAnims[keyEditorNodeIndex];
        const Model::Node& node = nodes[keyEditorNodeIndex];

        DrawVectorKeys(
            "Position Keys",
            nodeAnim.positionKeyframes,
            animation.secondsLength,
            node.position);
        DrawQuaternionKeys(
            "Rotation Keys",
            nodeAnim.rotationKeyframes,
            animation.secondsLength,
            node.rotation);
        DrawVectorKeys(
            "Scale Keys",
            nodeAnim.scaleKeyframes,
            animation.secondsLength,
            node.scale);

        if (ImGui::Button("Save VMDL##keys", ImVec2(-1.0f, 0.0f)))
        {
            model->SaveVmdl();
        }
    }

void Animator::DrawVectorKeys(
        const char* label,
        std::vector<Model::VectorKeyframe>& keys,
        float animationLength,
        const Vector3& defaultValue)
    {
        if (!ImGui::TreeNode(label)) return;

        int removeIndex = -1;
        bool sortKeys = false;
        for (int keyIndex = 0; keyIndex < static_cast<int>(keys.size()); ++keyIndex)
        {
            Model::VectorKeyframe& key = keys[keyIndex];
            ImGui::PushID(keyIndex);

            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::DragFloat("Sec", &key.seconds, 0.01f, 0.0f, animationLength, "%.3f"))
                sortKeys = true;

            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            ImGui::DragFloat3("Value", &key.value.x, 0.01f, -1000.0f, 1000.0f, "%.3f");

            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                removeIndex = keyIndex;

            ImGui::PopID();
        }

        if (removeIndex >= 0)
            keys.erase(keys.begin() + removeIndex);

        if (ImGui::Button("+ Add Key", ImVec2(-1.0f, 0.0f)))
        {
            Model::VectorKeyframe key;
            key.seconds = 0.0f;
            key.value = defaultValue;
            keys.push_back(key);
            sortKeys = true;
        }

        if (sortKeys)
        {
            std::sort(
                keys.begin(),
                keys.end(),
                [](const Model::VectorKeyframe& a, const Model::VectorKeyframe& b)
                {
                    return a.seconds < b.seconds;
                });
        }

        ImGui::TreePop();
    }

void Animator::DrawQuaternionKeys(
        const char* label,
        std::vector<Model::QuaternionKeyframe>& keys,
        float animationLength,
        const Quaternion& defaultValue)
    {
        if (!ImGui::TreeNode(label)) return;

        int removeIndex = -1;
        bool sortKeys = false;
        for (int keyIndex = 0; keyIndex < static_cast<int>(keys.size()); ++keyIndex)
        {
            Model::QuaternionKeyframe& key = keys[keyIndex];
            ImGui::PushID(keyIndex);

            ImGui::SetNextItemWidth(90.0f);
            if (ImGui::DragFloat("Sec", &key.seconds, 0.01f, 0.0f, animationLength, "%.3f"))
                sortKeys = true;

            ImGui::SameLine();
            ImGui::SetNextItemWidth(170.0f);
            if (ImGui::DragFloat4("Value", &key.value.x, 0.001f, -1.0f, 1.0f, "%.3f"))
                key.value.Normalize();

            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
                removeIndex = keyIndex;

            ImGui::PopID();
        }

        if (removeIndex >= 0)
            keys.erase(keys.begin() + removeIndex);

        if (ImGui::Button("+ Add Key", ImVec2(-1.0f, 0.0f)))
        {
            Model::QuaternionKeyframe key;
            key.seconds = 0.0f;
            key.value = defaultValue;
            keys.push_back(key);
            sortKeys = true;
        }

        if (sortKeys)
        {
            std::sort(
                keys.begin(),
                keys.end(),
                [](const Model::QuaternionKeyframe& a, const Model::QuaternionKeyframe& b)
                {
                    return a.seconds < b.seconds;
                });
        }

        ImGui::TreePop();
    }

    // -------------------------------------------------------------------
    // 選択中ステート詳細パネル
    // -------------------------------------------------------------------
void Animator::DrawStateDetail(Animator::AnimatorLayer& layer, int li)
    {
        int si = selectedState.stateIndex;
        if (si < 0 || si >= (int)layer.states.size()) return;

        Animator::State& state = layer.states[si];

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "State");
        ImGui::Separator();

        // ステート名
        char nameBuf[128];
        strncpy_s(nameBuf, state.name.c_str(), sizeof(nameBuf));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##statename", nameBuf, sizeof(nameBuf)))
            state.name = nameBuf;

        ImGui::Spacing();

        if (IsDynamicMode())
        {
            ImGui::Text("Dynamic Animation Clip");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##dynamicClipPath", &state.dynamicClipPath);

            if (ImGui::Button("Browse .danim", ImVec2(-1.0f, 0.0f)))
            {
                char path[MAX_PATH] = {};
                if (!state.dynamicClipPath.empty())
                    strcpy_s(path, state.dynamicClipPath.c_str());

                if (Dialog::OpenFileName(
                    path,
                    MAX_PATH,
                    "Dynamic Animation Clip\0*.danim\0All Files\0*.*\0\0",
                    "Open Dynamic Animation Clip") == DialogResult::OK)
                {
                    SetDynamicClipPath(li, si, path);
                }
            }

            if (ImGui::Button("Reload Clip", ImVec2(-1.0f, 0.0f)))
                ReloadDynamicClips();

            const float clipLength = GetStateLength(state);
            if (state.dynamicClipPath.empty())
                ImGui::TextDisabled("No .danim assigned.");
            else if (clipLength <= 0.0f)
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Failed to load clip.");
            else
                ImGui::TextDisabled("Length: %.3f sec", clipLength);
        }
        else
        {
            const auto& animations = GetModel()->GetAnimations();
            const char* currentAnimationName =
                (state.animationIndex >= 0 &&
                 state.animationIndex < static_cast<int>(animations.size()))
                ? animations[state.animationIndex].name.c_str()
                : "(none)";

            ImGui::Text("Animation");
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::BeginCombo("##anim", currentAnimationName))
            {
                for (int animationIndex = 0;
                     animationIndex < static_cast<int>(animations.size());
                     ++animationIndex)
                {
                    const bool selected =
                        state.animationIndex == animationIndex;
                    if (ImGui::Selectable(
                        animations[animationIndex].name.c_str(),
                        selected))
                    {
                        state.animationIndex = animationIndex;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            DrawFootIKRangeEditor(state);
        }

        ImGui::Spacing();
        ImGui::DragFloat("Speed", &state.speed, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::Checkbox("Loop", &state.loop);
        ImGui::Checkbox("Block AnyState Transition", &state.blockAnyStateTransitions);

        ImGui::Spacing();
        bool isDefault = (layer.defaultStateIndex == si);
        if (isDefault)
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "* Default State");
        else
            if (ImGui::Button("Set as Default"))
                SetDefaultState(li, si);

        // ---- このステートから出るトランジションの優先順位（ドラッグで並び替え）----
        if (!state.transitions.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Transition Order");

            int dragFrom = -1, dragTo = -1;
            for (int ti = 0; ti < (int)state.transitions.size(); ++ti)
            {
                const Animator::Transition& tr = state.transitions[ti];
                const std::string& toName =
                    (tr.toStateIndex >= 0 && tr.toStateIndex < (int)layer.states.size())
                    ? layer.states[tr.toStateIndex].name : "???";

                ImGui::PushID(ti);

                // ドラッグハンドル（ :: アイコン風）
                ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
                ImGui::Button("::##hdl");
                ImGui::PopStyleColor(2);
                if (ImGui::BeginDragDropSource(0))
                {
                    ImGui::SetDragDropPayload("TRANS_REORDER", &ti, sizeof(int));
                    ImGui::Text("-> %s", toName.c_str());
                    ImGui::EndDragDropSource();
                }
                ImGui::SameLine();

                // 選択中なら色を変える
                bool isSelected = (selectedTransition.layerIndex == li &&
                                   selectedTransition.fromStateIndex == si &&
                                   selectedTransition.transIndex == ti);
                if (isSelected)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));

                // 行全体をドラッグソースにする
                char label[128];
                snprintf(label, sizeof(label), "%d. -> %s", ti + 1, toName.c_str());
                ImGui::Selectable(label, isSelected, ImGuiSelectableFlags_None, ImVec2(0, 0));

                if (isSelected)
                    ImGui::PopStyleColor();

                // クリックで選択
                if (ImGui::IsItemClicked())
                {
                    selectedTransition = { li, si, ti };
                    selectedState = {};
                }

                // ドロップターゲット
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("TRANS_REORDER"))
                    {
                        dragFrom = *(const int*)payload->Data;
                        dragTo = ti;
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::PopID();
            }

            // ドラッグ&ドロップ後に並び替え実行
            if (dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo)
            {
                // 選択中トランジションのインデックスを追跡
                int selectedTi = (selectedTransition.layerIndex == li &&
                                  selectedTransition.fromStateIndex == si)
                    ? selectedTransition.transIndex : -1;

                Animator::Transition moved = state.transitions[dragFrom];
                state.transitions.erase(state.transitions.begin() + dragFrom);
                int insertAt = (dragTo > dragFrom) ? dragTo : dragTo;
                state.transitions.insert(state.transitions.begin() + insertAt, moved);

                // priority値を配列順に振り直す（内部整合性のため）
                for (int i = 0; i < (int)state.transitions.size(); ++i)
                    state.transitions[i].priority = (int)state.transitions.size() - 1 - i;

                // selectedTransition インデックスを新しい位置に追従
                if (selectedTi >= 0)
                {
                    if (selectedTi == dragFrom)
                        selectedTransition.transIndex = insertAt;
                    else if (dragFrom < dragTo && selectedTi > dragFrom && selectedTi <= dragTo)
                        selectedTransition.transIndex = selectedTi - 1;
                    else if (dragFrom > dragTo && selectedTi >= dragTo && selectedTi < dragFrom)
                        selectedTransition.transIndex = selectedTi + 1;
                }
            }
        }

        // ---- Callbacks ----
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Callbacks");

        auto& callbacks = state.callbacks;
        int removeIdx = -1;

        for (int ci = 0; ci < (int)callbacks.size(); ++ci)
        {
            auto& cb = callbacks[ci];
            ImGui::PushID(ci);

            // ラベル
            char labelBuf[64];
            strncpy_s(labelBuf, cb.label.c_str(), sizeof(labelBuf));
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText("Label##cblabel", labelBuf, sizeof(labelBuf)))
                cb.label = labelBuf;

            ImGui::SameLine();

            // 区間スライダー
            float range[2] = { cb.enterTimePer, cb.exitTimePer };
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::DragFloat2("Range##cbrange", range, 0.01f, 0.0f, 1.0f, "%.2f"))
            {
                cb.enterTimePer = range[0];
                cb.exitTimePer  = std::max(range[1], range[0] + 0.01f);
            }

            ImGui::SameLine();

            // 削除ボタン
            if (ImGui::SmallButton("x"))
                removeIdx = ci;

            // 関数バインド状況を表示
            ImGui::Indent();
            ImGui::TextDisabled("onEnter: %s", cb.onEnter ? "bound" : "(none)");
            ImGui::SameLine();
            ImGui::TextDisabled("onExit: %s",  cb.onExit  ? "bound" : "(none)");
            ImGui::Unindent();

            ImGui::PopID();
        }

        if (removeIdx >= 0)
            callbacks.erase(callbacks.begin() + removeIdx);

        if (ImGui::Button("+ Add Callback"))
            state.callbacks.push_back({ "(unnamed)", 0.0f, 1.0f, nullptr, nullptr });
    }

    // -------------------------------------------------------------------
    // 選択中トランジション詳細パネル
    // -------------------------------------------------------------------
void Animator::DrawTransitionDetail(Animator::AnimatorLayer& layer)
    {
        int si = selectedTransition.fromStateIndex;
        int ti = selectedTransition.transIndex;
        // AnyState の場合は layer.anyStateTransitions を参照する
        if (si == ANY_STATE_INDEX)
        {
            if (ti < 0 || ti >= (int)layer.anyStateTransitions.size()) return;
        }
        else
        {
            if (si < 0 || si >= (int)layer.states.size()) return;
            if (ti < 0 || ti >= (int)layer.states[si].transitions.size()) return;
        }

        Animator::Transition& tr = (si == ANY_STATE_INDEX)
            ? layer.anyStateTransitions[ti]
            : layer.states[si].transitions[ti];

        const std::string& fromName = (si == ANY_STATE_INDEX) ? "AnyState" : layer.states[si].name;
        const std::string& toName =
            (tr.toStateIndex >= 0 && tr.toStateIndex < (int)layer.states.size())
            ? layer.states[tr.toStateIndex].name : "???";

        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Transition");
        ImGui::Text("%s -> %s", fromName.c_str(), toName.c_str());

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.18f, 0.18f, 1.0f));
        if (ImGui::Button("Delete Transition", ImVec2(-1.0f, 0.0f)))
        {
            suppressNodeEditorInteractions = true;

            if (si == ANY_STATE_INDEX)
            {
                layer.anyStateTransitions.erase(
                    layer.anyStateTransitions.begin() + ti);
            }
            else
            {
                layer.states[si].transitions.erase(
                    layer.states[si].transitions.begin() + ti);
            }

            selectedTransition = {};
            ImGui::PopStyleColor();
            return;
        }
        ImGui::PopStyleColor();

        ImGui::Separator();

        ImGui::DragFloat("Blend Duration", &tr.transitionDuration, 0.01f, 0.0f, 5.0f);
        ImGui::Checkbox("Has Exit Time", &tr.hasExitTime);
        if (tr.hasExitTime)
            ImGui::DragFloat("Exit Time", &tr.exitTime, 0.01f, 0.0f, 1.0f);
        ImGui::Checkbox("Is Any", &tr.isAny);
        if (!tr.isAny)
        {
            ImGui::DragFloat("From Progress", &tr.sourceProgressMin, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("To Progress",   &tr.sourceProgressMax, 0.01f, 0.0f, 1.0f, "%.2f");
            if (tr.sourceProgressMin > tr.sourceProgressMax) tr.sourceProgressMax = tr.sourceProgressMin;
        }
        // Priority は ステート詳細の "Transition Order" リストで並び替えして変更する
        ImGui::TextDisabled("Priority: %d  (order in State detail)", tr.priority);
        ImGui::Checkbox("Can Interrupt", &tr.canInterrupt);

        if (si == ANY_STATE_INDEX)
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.4f, 1.0f), "Do Not Transition From");
            ImGui::BeginChild("ExcludedAnyStateSources", ImVec2(0, 120), true);
            for (int st = 0; st < (int)layer.states.size(); ++st)
            {
                const std::string& stName = layer.states[st].name;
                bool excluded = (std::find(tr.excludedFromStateIndices.begin(),
                                 tr.excludedFromStateIndices.end(), st) != tr.excludedFromStateIndices.end());
                if (ImGui::Checkbox(stName.c_str(), &excluded))
                {
                    if (excluded)
                    {
                        if (std::find(tr.excludedFromStateIndices.begin(),
                            tr.excludedFromStateIndices.end(), st) == tr.excludedFromStateIndices.end())
                        {
                            tr.excludedFromStateIndices.push_back(st);
                        }
                    }
                    else
                    {
                        tr.excludedFromStateIndices.erase(
                            std::remove(tr.excludedFromStateIndices.begin(),
                            tr.excludedFromStateIndices.end(), st),
                            tr.excludedFromStateIndices.end());
                    }
                }
            }
            ImGui::EndChild();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1, 1), "Conditions");

        const auto& params = GetParameters();
        const auto& triggers = GetTriggers();

        // 条件一覧
        for (int ci = 0; ci < (int)tr.conditions.size(); ++ci)
        {
            Animator::Condition& c = tr.conditions[ci];
            ImGui::PushID(ci);

            // パラメータ名コンボ
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::BeginCombo("##param", c.paramName.c_str()))
            {
                for (const auto& [n, v] : params)
                    if (ImGui::Selectable(n.c_str(), c.paramName == n))
                    {
                        c.paramName = n;
                        if (std::holds_alternative<bool>(v))
                            c.mode = Animator::ConditionMode::IsTrue;
                        else
                            c.mode = Animator::ConditionMode::Greater;
                    }
                for (const auto& [n, v] : triggers)
                    if (ImGui::Selectable(n.c_str(), c.paramName == n))
                    {
                        c.paramName = n;
                        c.mode = Animator::ConditionMode::Trigger;
                    }
                ImGui::EndCombo();
            }
            ImGui::SameLine();

            // モードコンボ
            {
                auto pit = params.find(c.paramName);
                bool isTrigger = (triggers.find(c.paramName) != triggers.end());
                bool isBool = (pit != params.end() && std::holds_alternative<bool>(pit->second));
                bool isNumeric = (pit != params.end() && !isBool);

                struct ModeEntry { const char* label; Animator::ConditionMode mode; };
                std::vector<ModeEntry> validModes;
                if (isNumeric)
                {
                    validModes = {
                        {">",  Animator::ConditionMode::Greater},
                        {"<",  Animator::ConditionMode::Less},
                        {"==", Animator::ConditionMode::Equals},
                        {"!=", Animator::ConditionMode::NotEquals},
                    };
                }
                else if (isBool)
                {
                    validModes = {
                        {"True",  Animator::ConditionMode::IsTrue},
                        {"False", Animator::ConditionMode::IsFalse},
                    };
                }
                else if (isTrigger)
                {
                    validModes = { {"Trigger", Animator::ConditionMode::Trigger} };
                }

                int modeIdx = 0;
                for (int m = 0; m < (int)validModes.size(); ++m)
                    if (validModes[m].mode == c.mode) { modeIdx = m; break; }

                std::vector<const char*> modeLabels;
                for (const auto& e : validModes) modeLabels.push_back(e.label);

                ImGui::SetNextItemWidth(60.0f);
                if (!modeLabels.empty() &&
                    ImGui::Combo("##mode", &modeIdx, modeLabels.data(), (int)modeLabels.size()))
                {
                    c.mode = validModes[modeIdx].mode;
                }
            }

            // しきい値 (Trigger/Bool 以外)
            auto pit2 = params.find(c.paramName);
            bool isBoolParam = (pit2 != params.end() && std::holds_alternative<bool>(pit2->second));
            bool isTriggerParam = (triggers.find(c.paramName) != triggers.end());

            if (!isBoolParam && !isTriggerParam &&
                (c.mode == Animator::ConditionMode::Greater ||
                c.mode == Animator::ConditionMode::Less ||
                c.mode == Animator::ConditionMode::Equals ||
                c.mode == Animator::ConditionMode::NotEquals))
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(60.0f);

                auto pit = params.find(c.paramName);
                if (pit != params.end() && std::holds_alternative<float>(pit->second))
                {
                    float thr = std::holds_alternative<float>(c.threshold)
                        ? std::get<float>(c.threshold) : 0.0f;
                    if (ImGui::DragFloat("##thr", &thr, 0.01f))
                        c.threshold = thr;
                }
                else
                {
                    int thr = std::holds_alternative<int>(c.threshold)
                        ? std::get<int>(c.threshold) : 0;
                    if (ImGui::DragInt("##thr", &thr))
                        c.threshold = thr;
                }
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("^") && ci > 0)
                std::swap(tr.conditions[ci], tr.conditions[ci - 1]);
            ImGui::SameLine();
            if (ImGui::SmallButton("v") && ci < (int)tr.conditions.size() - 1)
                std::swap(tr.conditions[ci], tr.conditions[ci + 1]);
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                tr.conditions.erase(tr.conditions.begin() + ci);
                --ci;
            }

            ImGui::PopID();
        }

        // 条件追加ボタン
        if (ImGui::Button("+ Condition"))
        {
            Animator::Condition newC;
            if (!params.empty())
            {
                newC.paramName = params.begin()->first;
                const auto& val = params.begin()->second;

                // パラメータの型に合わせてmodeとthresholdの初期値を設定
                if (std::holds_alternative<float>(val))
                {
                    newC.mode = Animator::ConditionMode::Greater;
                    newC.threshold = 0.0f;
                }
                else if (std::holds_alternative<int>(val))
                {
                    newC.mode = Animator::ConditionMode::Greater;
                    newC.threshold = 0;
                }
                else if (std::holds_alternative<bool>(val))
                {
                    newC.mode = Animator::ConditionMode::IsTrue;
                }
            }
            else if (!triggers.empty())
            {
                newC.paramName = triggers.begin()->first;
                newC.mode = Animator::ConditionMode::Trigger;
            }
            tr.conditions.push_back(newC);
        }


    }

    // -------------------------------------------------------------------
    // ピンID の逆引きヘルパー
    // -------------------------------------------------------------------
int Animator::DecodeOutPin(int li, ed::PinId pin)
    {
        int v = (int)pin.Get() - 100000 - li * 10000;
        if (v < 0) return -1;
        return (v % 2 == 0) ? v / 2 : -1;
    }
int Animator::DecodeInPin(int li, ed::PinId pin)
    {
        int v = (int)pin.Get() - 100000 - li * 10000;
        if (v < 0) return -1;
        return (v % 2 == 1) ? v / 2 : -1;
    }
void Animator::DecodeLinkId(ed::LinkId id, int& li, int& from, int& ti)
    {
        constexpr int LinkBase = 500000;
        constexpr int LinkLayerStride = 2000000;
        constexpr int LinkFromStride = 1000;

        int value = static_cast<int>(id.Get()) - LinkBase;
        li = value / LinkLayerStride;
        value %= LinkLayerStride;
        from = value / LinkFromStride;
        ti = value % LinkFromStride;
    }
