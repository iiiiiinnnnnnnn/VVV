// AnimatorSerialization.cpp

#include "Animation/Animator.h"

#include <algorithm>
#include <fstream>
#include <variant>

#include "nlohmann/json.hpp"

using json = nlohmann::json;
bool Animator::Serialize(const std::string& path) const
    {
        json root;
        root["animationMode"] = IsDynamicMode() ? "Dynamic" : "VMDLModel";

        // Parameters
        json jParams = json::array();
        for (const auto& kv : GetParameters())
        {
            json p;
            p["name"] = kv.first;
            if (std::holds_alternative<float>(kv.second))
            {
                p["type"] = "float";
                p["value"] = std::get<float>(kv.second);
            }
            else if (std::holds_alternative<int>(kv.second))
            {
                p["type"] = "int";
                p["value"] = std::get<int>(kv.second);
            }
            else if (std::holds_alternative<bool>(kv.second))
            {
                p["type"] = "bool";
                p["value"] = std::get<bool>(kv.second);
            }
            jParams.push_back(p);
        }

        // Triggers
        json jTriggers = json::array();
        for (const auto& kv : GetTriggers())
            jTriggers.push_back(kv.first);

        root["parameters"] = jParams;
        root["triggers"]   = jTriggers;

        // Layers
        json jLayers = json::array();
        for (int li = 0; li < GetLayerCount(); ++li)
        {
            const Animator::AnimatorLayer& layer =
                GetLayer(li);

            json jLayer;
            jLayer["name"]      = layer.name;
            jLayer["weight"]    = layer.weight;
            jLayer["blendMode"] = (int)layer.blendMode;
            jLayer["mask"]      = layer.mask.nodes;
            jLayer["defaultState"] = layer.defaultStateIndex;
            jLayer["hasAnyStateEditorPosition"] = layer.hasAnyStateEditorPosition;
            jLayer["anyStateEditorPosX"] = layer.anyStateEditorPosX;
            jLayer["anyStateEditorPosY"] = layer.anyStateEditorPosY;

            // States
            json jStates = json::array();
            for (const auto& state : layer.states)
            {
                json jState;
                jState["name"]           = state.name;
                jState["animationIndex"] = state.animationIndex;
                jState["dynamicClipPath"] = state.dynamicClipPath;
                jState["speed"]          = state.speed;
                jState["loop"]           = state.loop;
                jState["blockAnyStateTransitions"] = state.blockAnyStateTransitions;
                jState["hasEditorPosition"] = state.hasEditorPosition;
                jState["editorPosX"] = state.editorPosX;
                jState["editorPosY"] = state.editorPosY;

                // Transitions
                json jTrans = json::array();
                for (const auto& tr : state.transitions)
                {
                    json jTr;
                    jTr["toStateIndex"]       = tr.toStateIndex;
                    jTr["excludedFromStateIndices"] = tr.excludedFromStateIndices;
                    jTr["exitTime"]           = tr.exitTime;
                    jTr["transitionDuration"] = tr.transitionDuration;
                    jTr["hasExitTime"]        = tr.hasExitTime;
                    jTr["isAny"]              = tr.isAny;
                    jTr["sourceProgressMin"] = tr.sourceProgressMin;
                    jTr["sourceProgressMax"] = tr.sourceProgressMax;
                    jTr["priority"]           = tr.priority;
                    jTr["canInterrupt"]       = tr.canInterrupt;

                    json jConds = json::array();
                    for (const auto& c : tr.conditions)
                    {
                        json jC;
                        jC["paramName"] = c.paramName;
                        jC["mode"]      = (int)c.mode;
                        if (std::holds_alternative<float>(c.threshold))
                            jC["threshold"] = std::get<float>(c.threshold);
                        else if (std::holds_alternative<int>(c.threshold))
                            jC["threshold"] = std::get<int>(c.threshold);
                        else
                            jC["threshold"] = std::get<bool>(c.threshold);
                        jConds.push_back(jC);
                    }
                    jTr["conditions"] = jConds;
                    jTrans.push_back(jTr);
                }
                jState["transitions"] = jTrans;

                json jCallbacks = json::array();
				// 実行関数そのものは保存できないため、再バインドに必要なラベルと発火区間だけ保存する。
                for (const auto& cb : state.callbacks)
                {
                    json jCb;
                    jCb["label"]        = cb.label;
                    jCb["enterTimePer"] = cb.enterTimePer;
                    jCb["exitTimePer"]  = cb.exitTimePer;
                    jCallbacks.push_back(jCb);
                }
                jState["callbacks"] = jCallbacks;

                jStates.push_back(jState);
            }
            jLayer["states"] = jStates;

            json jAnyTrans = json::array();
            for (const auto& tr : layer.anyStateTransitions)
            {
                json jTr;
                jTr["toStateIndex"]       = tr.toStateIndex;
                jTr["excludedFromStateIndices"] = tr.excludedFromStateIndices;
                jTr["exitTime"]           = tr.exitTime;
                jTr["transitionDuration"] = tr.transitionDuration;
                jTr["hasExitTime"]        = tr.hasExitTime;
                jTr["isAny"]              = tr.isAny;
                jTr["sourceProgressMin"] = tr.sourceProgressMin;
                jTr["sourceProgressMax"] = tr.sourceProgressMax;
                jTr["priority"]           = tr.priority;
                jTr["canInterrupt"]       = tr.canInterrupt;

                json jConds = json::array();
                for (const auto& c : tr.conditions)
                {
                    json jC;
                    jC["paramName"] = c.paramName;
                    jC["mode"]      = (int)c.mode;
                    if (std::holds_alternative<float>(c.threshold))
                        jC["threshold"] = std::get<float>(c.threshold);
                    else if (std::holds_alternative<int>(c.threshold))
                        jC["threshold"] = std::get<int>(c.threshold);
                    else
                        jC["threshold"] = std::get<bool>(c.threshold);
                    jConds.push_back(jC);
                }
                jTr["conditions"] = jConds;
                jAnyTrans.push_back(jTr);
            }
            jLayer["anyStateTransitions"] = jAnyTrans;

            jLayers.push_back(jLayer);
        }
        root["layers"] = jLayers;

        std::ofstream ofs(path);
        if (!ofs) return false;
        ofs << root.dump(4);
        return true;
    }

    // -------------------------------------------------------
    // -------------------------------------------------------
void Animator::Deserialize(const std::string& path)
    {
        std::ifstream ifs(path);
        _ASSERT_EXPR(ifs, "Failed to open file: " + path);

        json root;
        try { root = json::parse(ifs); }
        catch (...) { _ASSERT_EXPR(false, "Failed to parse animator: " + path); }

		const std::string expectedMode = IsDynamicMode() ? "Dynamic" : "VMDLModel";
		const std::string fileMode = root.value("animationMode", expectedMode);
		const bool legacyVmdlMode = !IsDynamicMode() && fileMode == "Model";
		if (fileMode != expectedMode && !legacyVmdlMode)
        {
            _ASSERT_EXPR(false, L"Animator mode does not match the loaded file.");
            return;
        }

		// 既存データと混ざるとインデックス参照が壊れるため、読み込み前に全状態を破棄する。
        ClearAll();

        for (const auto& p : root["parameters"])
        {
            std::string name = p["name"];
            std::string type = p["type"];
            if (type == "float")      AddFloat(name, p["value"].get<float>());
            else if (type == "int")   AddInt(name, p["value"].get<int>());
            else if (type == "bool")  AddBool(name, p["value"].get<bool>());
        }

        for (const auto& t : root["triggers"])
            AddTrigger(t.get<std::string>());

        for (const auto& jLayer : root["layers"])
        {
            Animator::AvatarMask mask;
            mask.nodes = jLayer.value("mask", std::vector<int>{});

            int li = AddLayer(
                jLayer["name"].get<std::string>(),
                (Animator::BlendMode)jLayer["blendMode"].get<int>(),
                jLayer["weight"].get<float>(),
                mask);
            GetLayer(li).hasAnyStateEditorPosition = jLayer.value("hasAnyStateEditorPosition", false);
            GetLayer(li).anyStateEditorPosX = jLayer.value("anyStateEditorPosX", 0.0f);
            GetLayer(li).anyStateEditorPosY = jLayer.value("anyStateEditorPosY", 0.0f);

            for (const auto& jState : jLayer["states"])
            {
                int si = -1;
                if (IsDynamicMode())
                {
                    si = AddDynamicState(
                        li,
                        jState["name"].get<std::string>(),
                        jState.value("dynamicClipPath", std::string()),
                        jState["loop"].get<bool>(),
                        jState["speed"].get<float>());
                }
                else
                {
                    si = AddState(
                        li,
                        jState["name"].get<std::string>(),
                        jState.value("animationIndex", -1),
                        jState["loop"].get<bool>(),
                        jState["speed"].get<float>());
                }
                GetLayer(li).states[si].blockAnyStateTransitions =
                    jState.value("blockAnyStateTransitions", false);
                GetLayer(li).states[si].hasEditorPosition =
                    jState.value("hasEditorPosition", false);
                GetLayer(li).states[si].editorPosX =
                    jState.value("editorPosX", 0.0f);
                GetLayer(li).states[si].editorPosY =
                    jState.value("editorPosY", 0.0f);

                auto& stateRef = GetLayer(li).states[si];
                for (const auto& jTr : jState["transitions"])
                {
                    Animator::Transition tr;
                    tr.toStateIndex       = jTr["toStateIndex"].get<int>();
                    tr.excludedFromStateIndices =
                        jTr.value("excludedFromStateIndices", std::vector<int>{});
                    tr.transitionDuration = jTr["transitionDuration"].get<float>();
                    tr.hasExitTime        = jTr["hasExitTime"].get<bool>();
                    tr.isAny              = jTr.value("isAny", true);
                    tr.sourceProgressMin = jTr.contains("sourceProgressMin") ? jTr["sourceProgressMin"].get<float>() : jTr.value("sourceProgressThreshold", 0.0f);
                    tr.sourceProgressMax = jTr.value("sourceProgressMax", 1.0f);
                    tr.exitTime           = jTr["exitTime"].get<float>();
                    tr.priority           = jTr["priority"].get<int>();
                    tr.canInterrupt       = jTr["canInterrupt"].get<bool>();

                    for (const auto& jC : jTr["conditions"])
                    {
                        Animator::Condition c;
                        c.paramName = jC["paramName"].get<std::string>();
                        c.mode      = (Animator::ConditionMode)jC["mode"].get<int>();

                        if (jC["threshold"].is_number_float())
                            c.threshold = jC["threshold"].get<float>();
                        else if (jC["threshold"].is_number_integer())
                            c.threshold = jC["threshold"].get<int>();
                        else
                            c.threshold = jC["threshold"].get<bool>();

                        tr.conditions.push_back(c);
                    }
                    stateRef.transitions.push_back(tr);
                }
                std::sort(stateRef.transitions.begin(), stateRef.transitions.end(),
                          [](const Animator::Transition& a, const Animator::Transition& b)
                { return a.priority > b.priority; });

                if (jState.contains("callbacks"))
                {
					// ここでは識別情報だけ復元する。onEnter/onExitは読み込み後にBindCallbacksで設定する。
                    for (const auto& jCb : jState["callbacks"])
                    {
                        stateRef.AddCallback(
                            jCb["label"].get<std::string>(),
                            jCb["enterTimePer"].get<float>(),
                            jCb["exitTimePer"].get<float>());
                    }
                }
            }

            int defState = jLayer["defaultState"].get<int>();
            if (defState >= 0)
                SetDefaultState(li, defState);

            if (jLayer.contains("anyStateTransitions"))
            {
                auto& anyTrans = GetAnyStateTransitions_Mutable(li);
                for (const auto& jTr : jLayer["anyStateTransitions"])
                {
                    Animator::Transition tr;
                    tr.toStateIndex       = jTr["toStateIndex"].get<int>();
                    tr.excludedFromStateIndices =
                        jTr.value("excludedFromStateIndices", std::vector<int>{});
                    tr.transitionDuration = jTr["transitionDuration"].get<float>();
                    tr.hasExitTime        = jTr["hasExitTime"].get<bool>();
                    tr.isAny              = jTr.value("isAny", true);
                    tr.sourceProgressMin = jTr.contains("sourceProgressMin") ? jTr["sourceProgressMin"].get<float>() : jTr.value("sourceProgressThreshold", 0.0f);
                    tr.sourceProgressMax = jTr.value("sourceProgressMax", 1.0f);
                    tr.exitTime           = jTr["exitTime"].get<float>();
                    tr.priority           = jTr["priority"].get<int>();
                    tr.canInterrupt       = jTr["canInterrupt"].get<bool>();

                    for (const auto& jC : jTr["conditions"])
                    {
                        Animator::Condition c;
                        c.paramName = jC["paramName"].get<std::string>();
                        c.mode      = (Animator::ConditionMode)jC["mode"].get<int>();

                        if (jC["threshold"].is_number_float())
                            c.threshold = jC["threshold"].get<float>();
                        else if (jC["threshold"].is_number_integer())
                            c.threshold = jC["threshold"].get<int>();
                        else
                            c.threshold = jC["threshold"].get<bool>();

                        tr.conditions.push_back(c);
                    }
                    anyTrans.push_back(tr);
                }
                std::sort(anyTrans.begin(), anyTrans.end(),
                          [](const Animator::Transition& a, const Animator::Transition& b)
                { return a.priority > b.priority; });
            }
        }
    }
