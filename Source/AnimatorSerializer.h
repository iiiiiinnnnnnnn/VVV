// AnimatorSerializer.h

#pragma once
#include <algorithm>
#include <functional>
#include <variant>
#include <vector>

#include "Animator.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <string>

using json = nlohmann::json;

class AnimatorSerializer
{
public:
    // -------------------------------------------------------
    // Save
    // -------------------------------------------------------
    static bool Save(const Animator& anim, const std::string& path)
    {
        json root;
        root["animationMode"] = anim.IsDynamicMode() ? "Dynamic" : "Model";

        // Parameters
        json jParams = json::array();
        for (const auto& kv : anim.GetParameters())
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
        for (const auto& kv : anim.GetTriggers())
            jTriggers.push_back(kv.first);

        root["parameters"] = jParams;
        root["triggers"]   = jTriggers;

        // Layers
        json jLayers = json::array();
        for (int li = 0; li < anim.GetLayerCount(); ++li)
        {
            const Animator::AnimatorLayer& layer =
                const_cast<Animator&>(anim).GetLayer(li);

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

                json jFootIKRanges = json::array();
                for (const auto& range : state.footIKRanges)
                {
                    json jRange;
                    jRange["name"] = range.name;
                    jRange["targetName"] = range.targetName;
                    jRange["startRatio"] = range.startRatio;
                    jRange["endRatio"] = range.endRatio;
                    jRange["weight"] = range.weight;
                    jRange["fadeInRatio"] = range.fadeInRatio;
                    jRange["fadeOutRatio"] = range.fadeOutRatio;
                    jFootIKRanges.push_back(jRange);
                }
                jState["footIKRanges"] = jFootIKRanges;
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

                    // Conditions
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

                // Callbacks (label / enterTimePer / exitTimePer のみ保存。std::function は保存不可)
                json jCallbacks = json::array();
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

            // AnyState Transitions
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
    // Load  (既存データを全てクリアして上書き)
    // -------------------------------------------------------
    static void Load(Animator& anim, const std::string& path)
    {
        std::ifstream ifs(path);
        _ASSERT_EXPR(ifs, "Failed to open file: " + path);

        json root;
        try { root = json::parse(ifs); }
        catch (...) { _ASSERT_EXPR(false, "Failed to parse animator: " + path); }

        const std::string expectedMode = anim.IsDynamicMode() ? "Dynamic" : "Model";
        const std::string fileMode = root.value("animationMode", expectedMode);
        if (fileMode != expectedMode)
        {
            _ASSERT_EXPR(false, L"Animator mode does not match the loaded file.");
            return;
        }

        anim.ClearAll();

        // Parameters
        for (const auto& p : root["parameters"])
        {
            std::string name = p["name"];
            std::string type = p["type"];
            if (type == "float")      anim.AddFloat(name, p["value"].get<float>());
            else if (type == "int")   anim.AddInt(name, p["value"].get<int>());
            else if (type == "bool")  anim.AddBool(name, p["value"].get<bool>());
        }

        // Triggers
        for (const auto& t : root["triggers"])
            anim.AddTrigger(t.get<std::string>());

        // Layers
        for (const auto& jLayer : root["layers"])
        {
            Animator::AvatarMask mask;
            mask.nodes = jLayer.value("mask", std::vector<int>{});

            int li = anim.AddLayer(
                jLayer["name"].get<std::string>(),
                (Animator::BlendMode)jLayer["blendMode"].get<int>(),
                jLayer["weight"].get<float>(),
                mask);
            anim.GetLayer(li).hasAnyStateEditorPosition = jLayer.value("hasAnyStateEditorPosition", false);
            anim.GetLayer(li).anyStateEditorPosX = jLayer.value("anyStateEditorPosX", 0.0f);
            anim.GetLayer(li).anyStateEditorPosY = jLayer.value("anyStateEditorPosY", 0.0f);

            // States
            for (const auto& jState : jLayer["states"])
            {
                int si = -1;
                if (anim.IsDynamicMode())
                {
                    si = anim.AddDynamicState(
                        li,
                        jState["name"].get<std::string>(),
                        jState.value("dynamicClipPath", std::string()),
                        jState["loop"].get<bool>(),
                        jState["speed"].get<float>());
                }
                else
                {
                    si = anim.AddState(
                        li,
                        jState["name"].get<std::string>(),
                        jState.value("animationIndex", -1),
                        jState["loop"].get<bool>(),
                        jState["speed"].get<float>());
                }
                anim.GetLayer(li).states[si].blockAnyStateTransitions =
                    jState.value("blockAnyStateTransitions", false);
                anim.GetLayer(li).states[si].hasEditorPosition =
                    jState.value("hasEditorPosition", false);
                anim.GetLayer(li).states[si].editorPosX =
                    jState.value("editorPosX", 0.0f);
                anim.GetLayer(li).states[si].editorPosY =
                    jState.value("editorPosY", 0.0f);

                // Transitions
                // AddTransition は内部でソートするため、JSONの順番が崩れる。
                // 直接 push_back → Condition を付与 → 最後にまとめてソートする。
                auto& stateRef = anim.GetLayer(li).states[si];
                if (jState.contains("footIKRanges"))
                {
                    for (const auto& jRange : jState["footIKRanges"])
                    {
                        Animator::FootIKRange range;
                        range.name = jRange.value("name", std::string("FootIK"));
                        range.targetName = jRange.value("targetName", jRange.value("targetBoneName", std::string("All")));
                        if (range.targetName.empty()) range.targetName = "All";
                        range.startRatio = jRange.value("startRatio", 0.0f);
                        range.endRatio = jRange.value("endRatio", 1.0f);
                        range.weight = jRange.value("weight", 1.0f);
                        range.fadeInRatio = jRange.value("fadeInRatio", 0.03f);
                        range.fadeOutRatio = jRange.value("fadeOutRatio", 0.03f);
                        stateRef.footIKRanges.push_back(range);
                    }
                }
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

                    // Conditions
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
                // JSON の並び順 = priority 順として保存されているのでソート不要だが、
                // 念のため priority 値でソートして整合性を保つ
                std::sort(stateRef.transitions.begin(), stateRef.transitions.end(),
                          [](const Animator::Transition& a, const Animator::Transition& b)
                { return a.priority > b.priority; });

                // Callbacks の復元（label / enter / exit のみ。std::function は BindCallbacks() で再バインドする）
                if (jState.contains("callbacks"))
                {
                    for (const auto& jCb : jState["callbacks"])
                    {
                        stateRef.AddCallback(
                            jCb["label"].get<std::string>(),
                            jCb["enterTimePer"].get<float>(),
                            jCb["exitTimePer"].get<float>());
                    }
                }
            }

            // DefaultState
            int defState = jLayer["defaultState"].get<int>();
            if (defState >= 0)
                anim.SetDefaultState(li, defState);

            // AnyState Transitions
            if (jLayer.contains("anyStateTransitions"))
            {
                auto& anyTrans = anim.GetAnyStateTransitions_Mutable(li);
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
};



