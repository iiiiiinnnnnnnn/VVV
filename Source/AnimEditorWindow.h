// AnimEditorWindow.h

#pragma once

#include "Animator.h"
#include <imgui_node_editor.h>
#include <unordered_map>
#include <string>

#include "Dialog.h"
#include "AnimatorSerializer.h"

namespace ed = ax::NodeEditor;

// -------------------------------------------------------
// ID 割り当て規則
//   Node  ID : layerIndex * 10000 + stateIndex + 1      (1-based)
//   Pin   ID : layerIndex * 100000 + stateIndex * 100 + pinSlot (出力=0, 入力=1..N)
//   Link  ID : layerIndex * 1000000 + fromState * 1000 + transitionIndex
// -------------------------------------------------------

class AnimEditorWindow
{
public:
    AnimEditorWindow(Animator* animator)
        : animator(animator)
    {
        ed::Config cfg;
        cfg.SettingsFile = nullptr;
        cfg.CanvasSizeMode = ax::NodeEditor::CanvasSizeMode::FitVerticalView;
        context = ed::CreateEditor(&cfg);
    }

    ~AnimEditorWindow()
    {
        if (context)
            ed::DestroyEditor(context);
    }

    // Animator::DrawGUI() から毎フレーム呼ぶ
    void Draw(bool* pOpen)
    {
        if (!pOpen || !*pOpen) return;

        // cpp側からロードされた場合、パスを同期
        if (!animator->GetLastPath().empty() &&
            m_currentFilePath[0] == '\0')
        {
            strcpy_s(m_currentFilePath, animator->GetLastPath().c_str());
        }

        ImGui::SetNextWindowSize(ImVec2(1100, 700), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Animator Editor", pOpen,
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::End();
            return;
        }

        // ---- レイヤー タブ -----------------------------------------
        const int layerCount = (int)animator->GetLayerCount();
        if (layerCount == 0)
        {
            ImGui::TextDisabled("No layers.");
            ImGui::End();
            return;
        }

        // Toolbar
        if (ImGui::Button("Save"))
        {
            if (m_currentFilePath[0] == '\0')
            {
                // 未保存なら SaveAs へ
                if (Dialog::SaveFileName(m_currentFilePath, MAX_PATH,
                    "Animator File\0*.animator\0All Files\0*.*\0\0",
                    "Save Animator", "animator") == DialogResult::OK)
                {
                    animator->Save(m_currentFilePath);
                }
            }
            else
            {
                animator->Save(m_currentFilePath);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As"))
        {
            char buf[MAX_PATH] = {};
            strcpy_s(buf, m_currentFilePath);
            if (Dialog::SaveFileName(buf, MAX_PATH,
                "Animator File\0*.animator\0All Files\0*.*\0\0",
                "Save As Animator", "animator") == DialogResult::OK)
            {
                strcpy_s(m_currentFilePath, buf);
                animator->Save(m_currentFilePath);
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
                strcpy_s(m_currentFilePath, buf);
                animator->Load(m_currentFilePath);
                {
                    positionSet.clear();
                    selectedTrans = {};
                    currentLayer = 0;
                }
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled(m_currentFilePath[0] ? m_currentFilePath : "(unsaved)");
        ImGui::Separator();

        // ---- レイヤータブ + 管理ボタン ---------------------------------
        if (ImGui::BeginTabBar("Layers"))
        {
            for (int li = 0; li < layerCount; ++li)
            {
                Animator::AnimatorLayer& layer = animator->GetLayer(li);
                char tabLabel[80];
                sprintf_s(tabLabel, "%s##tab%d", layer.name.c_str(), li);
                bool tabOpen = ImGui::BeginTabItem(tabLabel);
                if (tabOpen)
                {
                    currentLayer = li;
                    DrawLayerEditor(layer, li);
                    ImGui::EndTabItem();
                }
            }

            // + ボタンでレイヤー追加
            if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
                animator->AddLayer("New Layer", Animator::BlendMode::Override, 1.0f, {});

            ImGui::EndTabBar();
        }

        // ---- レイヤー追加モーダル ---------------------------------------
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("AddLayerModal", nullptr,
            ImGuiWindowFlags_NoResize))
        {
            ImGui::Text("Layer Name:");
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("##layername", m_addLayerName, sizeof(m_addLayerName));

            ImGui::Spacing();
            ImGui::Text("Bone Mask (unchecked = all bones):");
            ImGui::Separator();

            const auto& nodes = animator->GetModel()->GetNodes();
            ImGui::BeginChild("BoneList", ImVec2(0, 300), true);
            for (int ni = 0; ni < (int)nodes.size(); ++ni)
            {
                if (ni >= (int)m_maskSelection.size())
                    m_maskSelection.resize(ni + 1, false);
                ImGui::PushID(ni);
                bool bsel = m_maskSelection[ni];
                if (ImGui::Checkbox(nodes[ni].name.c_str(), &bsel))
                    m_maskSelection[ni] = bsel;
                ImGui::PopID();
            }
            ImGui::EndChild();

            ImGui::Spacing();
            if (ImGui::Button("Add", ImVec2(120, 0)))
            {
                Animator::AvatarMask mask;
                for (int ni = 0; ni < (int)m_maskSelection.size(); ++ni)
                    if (m_maskSelection[ni]) mask.nodes.push_back(ni);
                animator->AddLayer(m_addLayerName,
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

private:
    Animator* animator = nullptr;
    ed::EditorContext* context = nullptr;
    int          currentLayer = 0;
    char m_currentFilePath[MAX_PATH] = {};

    // 選択中のリンク (遷移) を覚えておくための情報
    struct SelectedTransition
    {
        int layerIndex = -1;
        int fromStateIndex = -1;
        int transIndex = -1;
    };
    SelectedTransition selectedTrans;

    // 選択中のステート
    struct SelectedState { int layerIndex = -1; int stateIndex = -1; };
    SelectedState selectedState;

    // ノードの初期配置オフセット (初回のみ使う)
    std::unordered_map<int /*nodeId*/, bool> positionSet;

    ImVec2 m_contextMenuPos = { 0.0f, 0.0f };
    ImVec2 m_canvasMousePos = { 0.0f, 0.0f };
    bool   m_pendingNodePlace = false;
    int    m_pendingNodeSi = -1;
    ed::NodeId m_deleteNodeId;
    ed::LinkId m_deleteLinkId;

    float  m_leftPanelWidth = 230.0f;  // 左パネル幅（ドラッグで変更可能）

    // レイヤー追加用マスク選択
    bool                m_addLayerPopupOpen = false;
    char                m_addLayerName[64]  = "New Layer";
    std::vector<bool>   m_maskSelection;      // ボーンごとの選択状態
    int                 m_contextLayerIndex = -1; // 右クリックしたタブのレイヤーIndex

    // -------------------------------------------------------------------
    // ID ヘルパー
    // -------------------------------------------------------------------
    static ed::PinId OutPin(int li, int si)
    {
        return ed::PinId(100000 + li * 10000 + si * 2 + 0);  // ベース100000を追加
    }
    static ed::PinId InPin(int li, int si)
    {
        return ed::PinId(100000 + li * 10000 + si * 2 + 1);  // ベース100000を追加
    }
    static ed::NodeId NodeId(int li, int si)
    {
        return ed::NodeId(1 + li * 1000 + si);  // 1-based
    }
    static ed::LinkId LinkId(int li, int from, int ti)
    {
        return ed::LinkId(500000 + li * 100000 + from * 1000 + ti);  // ベース500000を追加
    }

    // -------------------------------------------------------------------
    // レイヤーエディタ本体
    // -------------------------------------------------------------------
    void DrawLayerEditor(Animator::AnimatorLayer& layer, int li)
    {
        ImGui::BeginChild("LeftPanel", ImVec2(m_leftPanelWidth, 0), true);

        ImGui::PushID(li);
        if (selectedTrans.layerIndex == li)
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
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.3f,0.3f,0.3f,0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f,0.5f,0.5f,0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.6f,0.6f,0.6f,1.0f));
        ImGui::Button("##splitter", ImVec2(4.0f, -1.0f));
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemActive())
        {
            m_leftPanelWidth += ImGui::GetIO().MouseDelta.x;
            m_leftPanelWidth = std::clamp(m_leftPanelWidth, 150.0f, 500.0f);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        ImGui::SameLine();

        ed::SetCurrentEditor(context);
        ed::Begin("NodeEditor", ImVec2(0, 0));
        m_canvasMousePos = ImGui::GetMousePos();
        if (m_deleteNodeId)
        {
            ed::DeleteNode(m_deleteNodeId);
            m_deleteNodeId = ed::NodeId();
        }
        if (m_deleteLinkId)
        {
            ed::DeleteLink(m_deleteLinkId);
            m_deleteLinkId = ed::LinkId();
        }
        DrawNodes(layer, li);
        DrawLinks(layer, li);
        HandleInteractions(layer, li);
        ed::End();
        ed::SetCurrentEditor(nullptr);
    }

    // -------------------------------------------------------------------
    // ノード描画
    // -------------------------------------------------------------------
    void DrawNodes(Animator::AnimatorLayer& layer, int li)
    {
        // 配置待ちノードがあれば今フレームで位置をセット
        if (m_pendingNodePlace && m_pendingNodeSi >= 0 &&
            m_pendingNodeSi < (int)layer.states.size())
        {
            ax::NodeEditor::NodeId nid = NodeId(li, m_pendingNodeSi);
            positionSet[(int)nid.Get()] = true;
            ed::SetNodePosition(nid, m_contextMenuPos);
            m_pendingNodePlace = false;
            m_pendingNodeSi = -1;
        }

        for (int si = 0; si < (int)layer.states.size(); ++si)
        {
            Animator::State& state = layer.states[si];
            ax::NodeEditor::NodeId nid = NodeId(li, si);

            int nidInt = (int)nid.Get();
            if (positionSet.find(nidInt) == positionSet.end())
            {
                positionSet[nidInt] = true;
                ax::NodeEditor::SetNodePosition(nid,
                    ImVec2(200.0f + si * 180.0f, 100.0f + (si % 3) * 130.0f));
            }

            bool isCurrent = (layer.currentStateIndex == si);
            bool isNext = (layer.isTransitioning && layer.nextStateIndex == si);

            if (isCurrent || isNext)
                ax::NodeEditor::PushStyleColor(ax::NodeEditor::StyleColor_NodeBorder,
                    isNext ? ImVec4(0.9f, 0.7f, 0.1f, 1.0f)
                    : ImVec4(0.2f, 0.8f, 0.2f, 1.0f));

            ax::NodeEditor::BeginNode(nid);

            // ノード全体を li,si でスコープ
            ImGui::PushID(li * 10000 + si);

            ImVec4 nameCol = (si == 0)
                ? ImVec4(0.4f, 0.6f, 0.9f, 1.0f)
                : ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, nameCol);
            ImGui::TextUnformatted(state.name.c_str());
            ImGui::PopStyleColor();

            // アニメ名を表示
            const auto& anims = animator->GetModel()->GetAnimations();
            const char* animName = (state.animationIndex >= 0 &&
                state.animationIndex < (int)anims.size())
                ? anims[state.animationIndex].name.c_str() : "(none)";
            ImGui::TextDisabled("%s", animName);

            if (isCurrent)
            {
                const auto& anims = animator->GetModel()->GetAnimations();
                if (state.animationIndex >= 0 &&
                    state.animationIndex < (int)anims.size())
                {
                    float len = anims[state.animationIndex].secondsLength;
                    float prog = (len > 0.0f) ? layer.currentTime / len : 0.0f;
                    char pbId[32];
                    sprintf_s(pbId, "##pb%d_%d", li, si);
                    ImGui::ProgressBar(prog, ImVec2(160.0f, 5.0f), pbId);
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

            if (isCurrent || isNext)
                ax::NodeEditor::PopStyleColor();

            if (ed::IsNodeSelected(nid) && !ImGui::IsMouseDragging(0))
            {
                selectedState = { li, si };
                selectedTrans = {};
            }
        }
    }

    // -------------------------------------------------------------------
    // リンク描画
    // -------------------------------------------------------------------
    void DrawLinks(Animator::AnimatorLayer& layer, int li)
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
                    (selectedTrans.layerIndex == li &&
                        selectedTrans.fromStateIndex == si &&
                        selectedTrans.transIndex == ti);

                ImVec4 col = isSelected
                    ? ImVec4(1.0f, 0.8f, 0.0f, 1.0f)
                    : ImVec4(0.7f, 0.7f, 0.7f, 1.0f);

                ed::Link(LinkId(li, si, ti),
                    OutPin(li, si),
                    InPin(li, tr.toStateIndex),
                    col, 2.0f);
            }
        }
    }

    // -------------------------------------------------------------------
    // インタラクション (リンク作成 / クリック選択 / 削除)
    // -------------------------------------------------------------------
    void HandleInteractions(Animator::AnimatorLayer& layer, int li)
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
                        // 既存チェック
                        bool exists = false;
                        for (const auto& t : layer.states[fromSi].transitions)
                            if (t.toStateIndex == toSi) { exists = true; break; }

                        if (!exists && ed::AcceptNewItem(ImVec4(0.2f, 0.8f, 0.2f, 1), 2.0f))
                        {
                            // 遷移追加
                            animator->AddTransition(li, fromSi, toSi,
                                0.1f, false, 1.0f, 0, false);
                            // 選択状態に
                            selectedTrans = { li, fromSi,
                                (int)layer.states[fromSi].transitions.size() - 1 };
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
                    if (dli == li &&
                        dfrom < (int)layer.states.size() &&
                        dti < (int)layer.states[dfrom].transitions.size())
                    {
                        layer.states[dfrom].transitions.erase(
                            layer.states[dfrom].transitions.begin() + dti);
                        if (selectedTrans.layerIndex == li &&
                            selectedTrans.fromStateIndex == dfrom &&
                            selectedTrans.transIndex == dti)
                            selectedTrans = {};
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
                        // currentState も補正
                        if (layer.currentStateIndex == nv) layer.currentStateIndex = 0;
                        else if (layer.currentStateIndex > nv) --layer.currentStateIndex;
                        positionSet.clear();
                        selectedTrans = {};
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
            m_contextMenuPos = m_canvasMousePos;
        }
        // ノード・リンクのコンテキストメニューは現在未使用

        ed::Suspend();
        if (openContextMenu) ImGui::OpenPopup("BackgroundContextMenu");

        if (ImGui::BeginPopup("BackgroundContextMenu"))
        {
            if (ImGui::MenuItem("+ State"))
            {
                m_pendingNodePlace = true;
                m_pendingNodeSi = animator->AddState(li, "New State", 0, true, 1.0f);
            }
            ImGui::EndPopup();
        }

        // NodeContextMenu / LinkContextMenu は現在メニュー項目なし → 表示しない
        ed::Resume();

        // ---- リンクシングルクリック → 詳細パネルに表示 ---------------
        if (ed::IsBackgroundClicked())
        {
            selectedTrans = {};
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
                selectedTrans = { li, dfrom, dti };
        }
    }

    // -------------------------------------------------------------------
    // パラメータパネル
    // -------------------------------------------------------------------
    void DrawParameterPanel()
    {
        ImGui::TextColored(ImVec4(0.6f, 0.9f, 0.6f, 1.0f), "Parameters");

        const auto& params = animator->GetParameters();
        const auto& triggers = animator->GetTriggers();

        for (const auto& [name, val] : params)
        {
            ImGui::PushID(name.c_str());
            if (std::holds_alternative<float>(val))
            {
                float v = std::get<float>(val);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::DragFloat("##v", &v, 0.01f))
                    animator->SetFloat(name, v);
                ImGui::SameLine(); ImGui::TextUnformatted(name.c_str());
            }
            else if (std::holds_alternative<int>(val))
            {
                int v = std::get<int>(val);
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::DragInt("##v", &v))
                    animator->SetInt(name, v);
                ImGui::SameLine(); ImGui::TextUnformatted(name.c_str());
            }
            else if (std::holds_alternative<bool>(val))
            {
                bool v = std::get<bool>(val);
                if (ImGui::Checkbox("##v", &v))
                    animator->SetBool(name, v);
                ImGui::SameLine(); ImGui::TextUnformatted(name.c_str());
            }
            ImGui::PopID();
        }

        for (const auto& [name, fired] : triggers)
        {
            ImGui::PushID(("trigger_" + name).c_str());
            ImGui::PushStyleColor(ImGuiCol_Button,
                fired ? ImVec4(0.8f, 0.3f, 0.1f, 1.0f)
                : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button(name.c_str(), ImVec2(80, 0)))
                animator->SetTrigger(name);
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }

    // -------------------------------------------------------------------
    // レイヤー設定パネル（常時表示）
    // -------------------------------------------------------------------
    void DrawLayerSettings(Animator::AnimatorLayer& layer, int li)
    {
        ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "Layer Settings");

        // レイヤー名
        char nameBuf[64];
        strncpy_s(nameBuf, layer.name.c_str(), sizeof(nameBuf));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##layername", nameBuf, sizeof(nameBuf)))
            layer.name = nameBuf;

        // Weight
        ImGui::TextDisabled("Weight");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::DragFloat("##weight", &layer.weight, 0.01f, 0.0f, 1.0f, "%.2f");

        // BlendMode
        ImGui::TextDisabled("Blend Mode");
        const char* blendModes[] = { "Override", "Additive" };
        int bm = (int)layer.blendMode;
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##blendmode", &bm, blendModes, 2))
            layer.blendMode = (Animator::BlendMode)bm;

        // ボーンマスク
        ImGui::Spacing();
        ImGui::TextDisabled("Bone Mask");
        const auto& nodes = animator->GetModel()->GetNodes();
        ImGui::BeginChild("BoneMask", ImVec2(0, 120), true);
        for (int ni = 0; ni < (int)nodes.size(); ++ni)
        {
            ImGui::PushID(ni);
            bool inMask = layer.mask.Contains(ni) && !layer.mask.nodes.empty();
            if (ImGui::Checkbox(nodes[ni].name.c_str(), &inMask))
            {
                if (inMask)
                {
                    // 追加
                    if (std::find(layer.mask.nodes.begin(), layer.mask.nodes.end(), ni)
                        == layer.mask.nodes.end())
                        layer.mask.nodes.push_back(ni);
                }
                else
                {
                    // 削除
                    layer.mask.nodes.erase(
                        std::remove(layer.mask.nodes.begin(), layer.mask.nodes.end(), ni),
                        layer.mask.nodes.end());
                }
            }
            ImGui::PopID();
        }
        ImGui::EndChild();

        // レイヤー削除（2つ以上あるとき）
        if (animator->GetLayerCount() > 1)
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Delete Layer", ImVec2(-1, 0)))
                animator->RemoveLayer(li);
            ImGui::PopStyleColor();
        }
    }

    // -------------------------------------------------------------------
    // 選択中ステート詳細パネル
    // -------------------------------------------------------------------
    void DrawStateDetail(Animator::AnimatorLayer& layer, int li)
    {
        int si = selectedState.stateIndex;
        if (si < 0 || si >= (int)layer.states.size()) return;

        Animator::State& state = layer.states[si];
        const auto& anims = animator->GetModel()->GetAnimations();

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "State");
        ImGui::Separator();

        // ステート名
        char nameBuf[128];
        strncpy_s(nameBuf, state.name.c_str(), sizeof(nameBuf));
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##statename", nameBuf, sizeof(nameBuf)))
            state.name = nameBuf;

        ImGui::Spacing();

        // アニメーション選択
        const char* currentAnimName = (state.animationIndex >= 0 &&
            state.animationIndex < (int)anims.size())
            ? anims[state.animationIndex].name.c_str() : "(none)";

        ImGui::Text("Animation");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##anim", currentAnimName))
        {
            for (int ai = 0; ai < (int)anims.size(); ++ai)
            {
                bool sel = (state.animationIndex == ai);
                if (ImGui::Selectable(anims[ai].name.c_str(), sel))
                    state.animationIndex = ai;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::DragFloat("Speed", &state.speed, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::Checkbox("Loop", &state.loop);

        ImGui::Spacing();
        bool isDefault = (layer.defaultStateIndex == si);
        if (isDefault)
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "* Default State");
        else
            if (ImGui::Button("Set as Default"))
                animator->SetDefaultState(li, si);
    }

    // -------------------------------------------------------------------
    // 選択中トランジション詳細パネル
    // -------------------------------------------------------------------
    void DrawTransitionDetail(Animator::AnimatorLayer& layer)
    {
        int si = selectedTrans.fromStateIndex;
        int ti = selectedTrans.transIndex;
        if (si < 0 || si >= (int)layer.states.size()) return;
        if (ti < 0 || ti >= (int)layer.states[si].transitions.size()) return;

        Animator::Transition& tr = layer.states[si].transitions[ti];
        const std::string& fromName = layer.states[si].name;
        const std::string& toName =
            (tr.toStateIndex >= 0 && tr.toStateIndex < (int)layer.states.size())
            ? layer.states[tr.toStateIndex].name : "???";

        ImGui::TextColored(ImVec4(1, 0.8f, 0.2f, 1), "Transition");
        ImGui::Text("%s -> %s", fromName.c_str(), toName.c_str());
        ImGui::Separator();

        ImGui::DragFloat("Blend Duration", &tr.transitionDuration, 0.01f, 0.0f, 5.0f);
        ImGui::Checkbox("Has Exit Time", &tr.hasExitTime);
        if (tr.hasExitTime)
            ImGui::DragFloat("Exit Time", &tr.exitTime, 0.01f, 0.0f, 1.0f);
        ImGui::DragInt("Priority", &tr.priority, 1, -99, 99);
        ImGui::Checkbox("Can Interrupt", &tr.canInterrupt);

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 1, 1), "Conditions");

        const auto& params = animator->GetParameters();
        const auto& triggers = animator->GetTriggers();

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
                        c.paramName = n;
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
            if (c.mode == Animator::ConditionMode::Greater ||
                c.mode == Animator::ConditionMode::Less ||
                c.mode == Animator::ConditionMode::Equals ||
                c.mode == Animator::ConditionMode::NotEquals)
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
            if (!params.empty())   newC.paramName = params.begin()->first;
            else if (!triggers.empty()) {
                newC.paramName = triggers.begin()->first;
                newC.mode = Animator::ConditionMode::Trigger;
            }
            tr.conditions.push_back(newC);
        }
    }

    // -------------------------------------------------------------------
    // ピンID の逆引きヘルパー
    // -------------------------------------------------------------------
    static int DecodeOutPin(int li, ed::PinId pin)
    {
        int v = (int)pin.Get() - 100000 - li * 10000;
        if (v < 0) return -1;
        return (v % 2 == 0) ? v / 2 : -1;
    }
    static int DecodeInPin(int li, ed::PinId pin)
    {
        int v = (int)pin.Get() - 100000 - li * 10000;
        if (v < 0) return -1;
        return (v % 2 == 1) ? v / 2 : -1;
    }
    static void DecodeLinkId(ed::LinkId id, int& li, int& from, int& ti)
    {
        int v = (int)id.Get() - 500000;
        li = v / 100000; v %= 100000;
        from = v / 1000;
        ti = v % 1000;
    }
};