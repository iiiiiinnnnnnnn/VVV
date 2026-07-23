// Animator.cpp

#include "Animation/Animator.h"
#include "Gameplay/Actor/Actor.h"
#include "Application/Time/GameTime.h"
#include "Application/Tools/DynamicAnimationSerializer.h"
#include "UI/Widget.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "IconsFontAwesome5.h"
#include <filesystem>
#include <unordered_set>

namespace
{
    bool TryGetFileWriteStamp(
        const std::string& path,
        long long& outStamp)
    {
        std::error_code error;
        const std::filesystem::file_time_type writeTime =
            std::filesystem::last_write_time(path, error);

        if (error)
        {
            return false;
        }

        outStamp = static_cast<long long>(
            writeTime.time_since_epoch().count());
        return true;
    }
}

Animator::Animator(Object* owner, std::shared_ptr<VMDLModel> model, bool unscaledTime)
    : Component(owner), animationMode(AnimationMode::VMDLModel), model(model), unscaledTime(unscaledTime)
{
    dynamic_cast<Actor*>(owner);
}

Animator::Animator(Object* owner, bool unscaledTime)
    : Component(owner), animationMode(AnimationMode::Dynamic), unscaledTime(unscaledTime)
{
    dynamic_cast<Widget*>(owner);
}

Animator::~Animator()
{
    if (editorContext) ax::NodeEditor::DestroyEditor(editorContext);
}

// =========================================================
// =========================================================
int Animator::AddLayer(const std::string& name, BlendMode blendMode, float weight, AvatarMask mask)
{
    AnimatorLayer layer;
    layer.name = name;
    layer.blendMode = blendMode;
    layer.weight = weight;
    layer.mask = mask;
    layers.push_back(layer);
    return (int)layers.size() - 1;
}

void Animator::SetLayerWeight(int li, float weight) { layers[li].weight = weight; }
void Animator::SetLayerMask(int li, AvatarMask mask) { layers[li].mask = mask; }

void Animator::RemoveLayer(int li)
{
    if (li < 0 || li >= (int)layers.size()) return;
    layers.erase(layers.begin() + li);
}

void Animator::SwapLayers(int a, int b)
{
    if (a < 0 || b < 0 || a >= (int)layers.size() || b >= (int)layers.size()) return;
    std::swap(layers[a], layers[b]);
}

void Animator::DuplicateLayer(int layerIndex)
{
    AnimatorLayer copy = layers[layerIndex];
    copy.name += " (Copy)";
    copy.currentStateIndex = copy.defaultStateIndex;
    copy.currentTime = 0.0f;
    copy.isTransitioning = false;
    copy.nextStateIndex = -1;
    copy.blendTime = 0.0f;
    layers.insert(layers.begin() + layerIndex + 1, std::move(copy));
}

// =========================================================
// =========================================================
int Animator::AddState(int li, const std::string& name, int animIndex, bool loop, float speed)
{
    State s;
    s.name = name;
    s.animationIndex = animIndex;
    s.loop = loop;
    s.speed = speed;

    AnimatorLayer& layer = layers[li];
    layer.states.push_back(s);

    const int stateIndex = static_cast<int>(layer.states.size()) - 1;
    if (layer.states.size() == 1)
    {
        layer.defaultStateIndex = stateIndex;
        layer.currentStateIndex = stateIndex;
        layer.currentTime = 0.0f;
    }

    return stateIndex;
}

int Animator::AddDynamicState(int li, const std::string& name,
                              const std::string& clipPath, bool loop, float speed)
{
    State s;
    s.name = name;
    s.animationIndex = -1;
    s.dynamicClipPath = clipPath;
    s.loop = loop;
    s.speed = speed;

    AnimatorLayer& layer = layers[li];
    layer.states.push_back(s);

    const int stateIndex = static_cast<int>(layer.states.size()) - 1;
    if (layer.states.size() == 1)
    {
        layer.defaultStateIndex = stateIndex;
        layer.currentStateIndex = stateIndex;
        layer.currentTime = 0.0f;
    }

    return stateIndex;
}

int Animator::AddTransition(int li, int from, int to, float duration,
                            bool hasExitTime, float exitTime, int priority, bool canInterrupt)
{
    Transition t;
    t.toStateIndex = to;
    t.transitionDuration = duration;
    t.hasExitTime = hasExitTime;
    t.exitTime = exitTime;
    t.priority = priority;
    t.canInterrupt = canInterrupt;

    auto& transitions = layers[li].states[from].transitions;
    transitions.push_back(t);
    std::sort(transitions.begin(), transitions.end(),
              [](const Transition& a, const Transition& b) { return a.priority > b.priority; });

    for (int i = 0; i < (int)transitions.size(); ++i)
        if (transitions[i].toStateIndex == to && transitions[i].priority == priority)
            return i;
    return (int)transitions.size() - 1;
}

void Animator::AddCondition(int li, int from, int ti,
                            const std::string& paramName, ConditionMode mode, ParamValue threshold)
{
    Condition c;
    c.paramName = paramName;
    c.mode = mode;
    c.threshold = threshold;
    layers[li].states[from].transitions[ti].conditions.push_back(c);
}

void Animator::SetDefaultState(int li, int stateIndex)
{
    auto& layer = layers[li];
    layer.defaultStateIndex = stateIndex;
    layer.currentStateIndex = stateIndex;
    layer.currentTime = 0.0f;
}

// =========================================================
// =========================================================
int Animator::AddAnyStateTransition(int li, int to, float duration,
                                    bool hasExitTime, float exitTime, int priority, bool canInterrupt)
{
    Transition t;
    t.toStateIndex       = to;
    t.transitionDuration = duration;
    t.hasExitTime        = hasExitTime;
    t.exitTime           = exitTime;
    t.priority           = priority;
    t.canInterrupt       = canInterrupt;

    auto& anyTrans = layers[li].anyStateTransitions;
    anyTrans.push_back(t);
    std::sort(anyTrans.begin(), anyTrans.end(),
              [](const Transition& a, const Transition& b) { return a.priority > b.priority; });

    for (int i = 0; i < (int)anyTrans.size(); ++i)
        if (anyTrans[i].toStateIndex == to && anyTrans[i].priority == priority)
            return i;
    return (int)anyTrans.size() - 1;
}

void Animator::AddAnyStateCondition(int li, int ti,
                                    const std::string& paramName, ConditionMode mode, ParamValue threshold)
{
    Condition c;
    c.paramName = paramName;
    c.mode      = mode;
    c.threshold = threshold;
    layers[li].anyStateTransitions[ti].conditions.push_back(c);
}

// =========================================================
// =========================================================
void Animator::AddFloat(const std::string& name, float v) { parameters[name] = v; }
void Animator::AddInt(const std::string& name, int v) { parameters[name] = v; }
void Animator::AddBool(const std::string& name, bool v) { parameters[name] = v; }
void Animator::AddTrigger(const std::string& name) { triggers[name] = false; }

void Animator::SetFloat(const std::string& name, float v) { parameters[name] = v; }
void Animator::SetInt(const std::string& name, int v) { parameters[name] = v; }
void Animator::SetBool(const std::string& name, bool v) { parameters[name] = v; }
void Animator::SetTrigger(const std::string& name)
{
    auto it = triggers.find(name);
    if (it != triggers.end()) it->second = true;
}

void Animator::ResetTriggers()
{
    for (auto& [name, val] : triggers) val = false;
}

void Animator::Update()
{
    rootMotionVec = Vector3::Zero;
    rootMotionRot = Quaternion::Identity;

    if (animationMode == AnimationMode::Dynamic)
    {
        dynamicClipWatchTimer += std::max(
            Game::Time::unscaledDeltaTime,
            0.0f);

        if (dynamicClipWatchTimer >= DynamicClipWatchInterval)
        {
            dynamicClipWatchTimer = std::fmod(
                dynamicClipWatchTimer,
                DynamicClipWatchInterval);
            ReloadActiveDynamicClipsIfChanged();
        }

        for (auto& layer : layers)
        {
            if (layer.currentStateIndex < 0)
                continue;
            UpdateDynamicLayer(layer);
        }

        ResetTriggers();
        return;
    }

    if (!model)
    {
        ResetTriggers();
        return;
    }

    if (useRootMotion && rootNodeIndex == -1)
    {
        SetRootMotion(rootNodeName);
    }

    const int nodeCount = static_cast<int>(model->GetNodes().size());
    std::vector<VMDLModel::NodePose> finalPoses(nodeCount);

    for (auto& layer : layers)
    {
        if (layer.currentStateIndex < 0)
            continue;
        UpdateLayer(layer, finalPoses);
    }

    model->SetNodePoses(finalPoses);
    model->UpdateTransform(Matrix::Identity);

    ResetTriggers();
}

void Animator::DrawGUI()
{
    ImGui::TextDisabled(
        animationMode == AnimationMode::Dynamic
        ? "Mode: Dynamic (.danim)"
        : "Mode: VMDLModel");

    if (animationMode == AnimationMode::Dynamic &&
        !dynamicAnimationError.empty())
    {
        ImGui::TextWrapped("Dynamic animation error:");
        ImGui::TextColored(
            ImVec4(1.0f, 0.25f, 0.25f, 1.0f),
            "%s",
            dynamicAnimationError.c_str());
    }

    for (int layerIndex = 0;
        layerIndex < static_cast<int>(layers.size());
        ++layerIndex)
    {
        const AnimatorLayer& layer = layers[layerIndex];
        ImGui::Text(
            "[%d] %s  w=%.2f",
            layerIndex,
            layer.name.c_str(),
            layer.weight);

        if (layer.currentStateIndex >= 0 &&
            layer.currentStateIndex < static_cast<int>(layer.states.size()))
        {
            const State& currentState =
                layer.states[layer.currentStateIndex];
            ImGui::SameLine();
            ImGui::TextColored(
                ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                "  -> %s",
                currentState.name.c_str());
        }
    }

    if (ImGui::Button("Open Animator"))
        OpenEditor();

    DrawEditor(&editorOpen);
}

void Animator::_print() const
{
    if (!model)
        return;

    for (const auto& anim : model->GetAnimations())
    {
        printf("Anim: %s, length=%.2f\n", anim.name.c_str(), anim.secondsLength);
    }
}

void Animator::OpenEditor()
{
    if (!editorContext)
    {
        ax::NodeEditor::Config config;
        config.SettingsFile = nullptr;
        config.CanvasSizeMode = ax::NodeEditor::CanvasSizeMode::FitVerticalView;
        editorContext = ax::NodeEditor::CreateEditor(&config);
    }
    editorOpen = true;
}

void Animator::UpdateLayer(
    AnimatorLayer& layer,
    std::vector<VMDLModel::NodePose>& finalPoses)
{
    _ASSERT_EXPR(
        layer.currentStateIndex >= 0 &&
        layer.currentStateIndex < static_cast<int>(layer.states.size()),
        L"Invalid current state index in layer");

    State& curState =
        layer.states[layer.currentStateIndex];

    const VMDLModel::Animation& curAnim =
        model->GetAnimations()[curState.animationIndex];

    float prevTime = layer.currentTime;

    float dt = unscaledTime
        ? Game::Time::unscaledDeltaTime
        : Game::Time::deltaTime;

    layer.currentTime += dt * curState.speed;

    float transitionNormalizedTime =
        curAnim.secondsLength > 0.0f
        ? layer.currentTime / curAnim.secondsLength
        : 0.0f;

    EvaluateCallbacks(
        curState,
        layer.currentTime,
        curAnim.secondsLength);

    bool looped = false;

	// ループ境界をまたいだフレームは、後段のルートモーション計算で終端→先頭を分割して扱う。
    if (layer.currentTime > curAnim.secondsLength)
    {
        if (curState.loop)
        {
            layer.currentTime -= curAnim.secondsLength;
            looped = true;
        }
        else
        {
            layer.currentTime =
                curAnim.secondsLength;
        }
    }

    if (useRootMotion &&
        rootNodeIndex != -1)
    {
		// 通常フレームは前回姿勢との差分だけを使う。
		// ループ時は「前回→終端」と「先頭→現在」を合成し、境界で逆向きに飛ぶのを防ぐ。
        VMDLModel::NodePose posePrev =
            SampleNodePose(
            curState.animationIndex,
            prevTime,
            rootNodeIndex);

        VMDLModel::NodePose poseCur =
            SampleNodePose(
            curState.animationIndex,
            layer.currentTime,
            rootNodeIndex);

        Vector3 deltaPos;
        Quaternion deltaRot;

        if (!looped)
        {
            deltaPos =
                poseCur.position -
                posePrev.position;

            Quaternion invPrev;
            posePrev.rotation.Inverse(invPrev);

            deltaRot =
                invPrev *
                poseCur.rotation;
        }
        else
        {
            VMDLModel::NodePose poseEnd =
                SampleNodePose(
                curState.animationIndex,
                curAnim.secondsLength,
                rootNodeIndex);

            VMDLModel::NodePose poseStart =
                SampleNodePose(
                curState.animationIndex,
                0.0f,
                rootNodeIndex);

            deltaPos =
                (poseEnd.position -
                posePrev.position) +
                (poseCur.position -
                poseStart.position);

            Quaternion invPrev;
            Quaternion invStart;

            posePrev.rotation.Inverse(invPrev);
            poseStart.rotation.Inverse(invStart);

            deltaRot =
                (invPrev * poseEnd.rotation) *
                (invStart * poseCur.rotation);
        }

        rootMotionVec +=
            deltaPos * layer.weight;

        rootMotionRot =
            rootMotionRot *
            Quaternion::Lerp(
            Quaternion::Identity,
            deltaRot,
            layer.weight);
    }

    model->ComputeAnimation(
        curState.animationIndex,
        layer.currentTime,
        nodePoses);

    if (layer.isTransitioning)
    {
		// 遷移元と遷移先を別々の時刻で評価し、blendDurationに沿って全ノードを補間する。
        State& nextState =
            layer.states[layer.nextStateIndex];

        if (model->GetAnimations().size() <= nextState.animationIndex)
        {
            return;
        }

        const VMDLModel::Animation& nextAnim =
            model->GetAnimations()[
                nextState.animationIndex];

        layer.nextTime +=
            dt * nextState.speed;

        if (layer.nextTime >
            nextAnim.secondsLength)
        {
            if (nextState.loop)
            {
                layer.nextTime -=
                    nextAnim.secondsLength;
            }
            else
            {
                layer.nextTime =
                    nextAnim.secondsLength;
            }
        }

        EvaluateCallbacks(
            nextState,
            layer.nextTime,
            nextAnim.secondsLength);

        model->ComputeAnimation(
            nextState.animationIndex,
            layer.nextTime,
            nextNodePoses);

        layer.blendTime += dt;

        float weight =
            layer.blendDuration > 0.0f
            ? layer.blendTime /
            layer.blendDuration
            : 1.0f;

        if (weight > 1.0f)
        {
            weight = 1.0f;
        }

        for (size_t i = 0;
            i < nodePoses.size();
            ++i)
        {
            nodePoses[i] =
                nodePoses[i].Lerp(
                nextNodePoses[i],
                weight);
        }

        if (weight >= 1.0f)
        {
            for (auto& callback :
                curState.callbacks)
            {
                if (!callback.entering)
                    continue;

                if (callback.onExit)
                {
                    callback.onExit(curState);
                }

                callback.entering = false;
            }

            layer.currentStateIndex =
                layer.nextStateIndex;

            layer.currentTime =
                layer.nextTime;

            layer.nextStateIndex = -1;
            layer.isTransitioning = false;
            layer.blendTime = 0.0f;
        }
        else
        {
			// ブレンド中断を許可した遷移だけ再評価する。同じ遷移先は再開始しない。
            for (const Transition& transition :
                curState.transitions)
            {
                if (!transition.canInterrupt)
                    continue;

                if (transition.toStateIndex ==
                    layer.nextStateIndex)
                {
                    continue;
                }

                if (transition.hasExitTime &&
                    transitionNormalizedTime <
                    transition.exitTime)
                {
                    continue;
                }

                if (!EvaluateTransition(
                    transition,
                    transitionNormalizedTime))
                {
                    continue;
                }

                for (auto& callback :
                    layer.states[
                        layer.nextStateIndex]
                    .callbacks)
                {
                    callback.entering = false;
                }

                layer.nextStateIndex =
                    transition.toStateIndex;

                layer.nextTime = 0.0f;
                layer.blendTime = 0.0f;

                layer.blendDuration =
                    transition.transitionDuration;

                for (auto& callback :
                    layer.states[
                        layer.nextStateIndex]
                    .callbacks)
                {
                    callback.entering = false;
                }

                break;
            }
        }
    }
    else
    {
        if (!curState.blockAnyStateTransitions)
        {
			// AnyStateを通常遷移より先に評価する。除外元リストに現在状態があれば候補から外す。
            for (const Transition& transition :
                layer.anyStateTransitions)
            {
                if (transition.toStateIndex ==
                    layer.currentStateIndex)
                {
                    continue;
                }

                if (std::find(
                    transition
                    .excludedFromStateIndices
                    .begin(),
                    transition
                    .excludedFromStateIndices
                    .end(),
                    layer.currentStateIndex) !=
                    transition
                    .excludedFromStateIndices
                    .end())
                {
                    continue;
                }

                if (transition.hasExitTime &&
                    transitionNormalizedTime <
                    transition.exitTime)
                {
                    continue;
                }

                if (!EvaluateTransition(
                    transition,
                    transitionNormalizedTime))
                {
                    continue;
                }

                for (auto& callback :
                    layer.states[
                        transition.toStateIndex]
                    .callbacks)
                {
                    callback.entering = false;
                }

                layer.nextStateIndex =
                    transition.toStateIndex;

                layer.nextTime = 0.0f;
                layer.blendTime = 0.0f;

                layer.blendDuration =
                    transition.transitionDuration;

                layer.isTransitioning =
                    transition.transitionDuration >
                    0.0f;

                if (!layer.isTransitioning)
                {
                    layer.currentStateIndex =
                        layer.nextStateIndex;

                    layer.currentTime = 0.0f;
                    layer.nextStateIndex = -1;
                }

                break;
            }
        }

        if (!layer.isTransitioning &&
            layer.nextStateIndex < 0)
        {
			// AnyStateで遷移が始まらなかった場合だけ、現在状態固有の遷移を優先順に評価する。
            for (const Transition& transition :
                curState.transitions)
            {
                if (transition.hasExitTime &&
                    transitionNormalizedTime <
                    transition.exitTime)
                {
                    continue;
                }

                if (!EvaluateTransition(
                    transition,
                    transitionNormalizedTime))
                {
                    continue;
                }

                for (auto& callback :
                    layer.states[
                        transition.toStateIndex]
                    .callbacks)
                {
                    callback.entering = false;
                }

                layer.nextStateIndex =
                    transition.toStateIndex;

                layer.nextTime = 0.0f;
                layer.blendTime = 0.0f;

                layer.blendDuration =
                    transition.transitionDuration;

                layer.isTransitioning =
                    transition.transitionDuration >
                    0.0f;

                if (!layer.isTransitioning)
                {
                    layer.currentStateIndex =
                        layer.nextStateIndex;

                    layer.currentTime = 0.0f;
                    layer.nextStateIndex = -1;
                }

                break;
            }
        }
    }

    for (int nodeIndex = 0;
        nodeIndex <
        static_cast<int>(nodePoses.size());
        ++nodeIndex)
    {
        if (!layer.mask.Contains(nodeIndex))
            continue;

        if (useRootMotion &&
            nodeIndex == rootNodeIndex)
        {
			// 移動量はActor側へ渡すため、描画モデルのルートには二重適用しない。
            finalPoses[nodeIndex].position =
                Vector3::Zero;

            finalPoses[nodeIndex].rotation =
                Quaternion::Identity;

            continue;
        }

        if (layer.blendMode ==
            BlendMode::Override)
        {
            finalPoses[nodeIndex] =
                finalPoses[nodeIndex].Lerp(
                nodePoses[nodeIndex],
                layer.weight);
        }
        else
        {
            finalPoses[nodeIndex].position +=
                nodePoses[nodeIndex].position *
                layer.weight;

            finalPoses[nodeIndex].rotation =
                finalPoses[nodeIndex].rotation *
                nodePoses[nodeIndex].rotation;
        }
    }
}

void Animator::UpdateDynamicLayer(
    AnimatorLayer& layer)
{
    if (layer.currentStateIndex < 0 ||
        layer.currentStateIndex >=
        static_cast<int>(layer.states.size()))
    {
        return;
    }

    const float dt = unscaledTime
        ? Game::Time::unscaledDeltaTime
        : Game::Time::deltaTime;

    State& currentState =
        layer.states[layer.currentStateIndex];

    const float loadedCurrentLength =
        GetStateLength(currentState);

    const float currentLength =
        loadedCurrentLength > 0.0f
        ? loadedCurrentLength
        : 1.0f;

    const float advancedCurrentTime =
        layer.currentTime +
        dt * currentState.speed;

    const float transitionNormalizedTime =
        currentLength > 0.0f
        ? advancedCurrentTime / currentLength
        : 1.0f;

    layer.currentTime = advancedCurrentTime;

    EvaluateCallbacks(
        currentState,
        layer.currentTime,
        currentLength);

    bool currentLooped = false;

    if (layer.currentTime > currentLength)
    {
        if (currentState.loop)
        {
            layer.currentTime =
                std::fmod(
                layer.currentTime,
                currentLength);

            currentLooped = true;
        }
        else
        {
            layer.currentTime =
                currentLength;
        }
    }
    else if (layer.currentTime < 0.0f)
    {
        layer.currentTime = 0.0f;
    }

    auto BeginTransition =
        [&](const Transition& transition) -> bool
    {
        if (transition.toStateIndex < 0 ||
            transition.toStateIndex >=
            static_cast<int>(layer.states.size()))
        {
            return false;
        }

        State& nextState =
            layer.states[transition.toStateIndex];

        for (auto& callback : nextState.callbacks)
        {
            callback.entering = false;
        }

        layer.nextStateIndex =
            transition.toStateIndex;

        layer.nextTime = 0.0f;
        layer.blendTime = 0.0f;

        layer.blendDuration =
            transition.transitionDuration;

        if (layer.blendDuration < 0.0f)
        {
            layer.blendDuration = 0.0f;
        }

        layer.isTransitioning =
            layer.blendDuration > 0.0f;

        if (!layer.isTransitioning)
        {
            for (auto& callback :
                currentState.callbacks)
            {
                if (!callback.entering)
                    continue;

                if (callback.onExit)
                {
                    callback.onExit(
                        currentState);
                }

                callback.entering = false;
            }

            layer.currentStateIndex =
                layer.nextStateIndex;

            layer.currentTime = 0.0f;
            layer.nextStateIndex = -1;

            ApplyDynamicState(
                layer.states[
                    layer.currentStateIndex],
                    0.0f);
        }

        return true;
    };

    auto CanTransition =
        [&](
        const Transition& transition,
        float normalizedTime) -> bool
    {
        if (transition.hasExitTime &&
            normalizedTime <
            transition.exitTime)
        {
            return false;
        }

        return EvaluateTransition(
            transition,
            normalizedTime);
    };

    if (layer.isTransitioning)
    {
        if (layer.nextStateIndex < 0 ||
            layer.nextStateIndex >=
            static_cast<int>(layer.states.size()))
        {
            layer.isTransitioning = false;
            layer.nextStateIndex = -1;

            ApplyDynamicState(
                currentState,
                layer.currentTime);

            return;
        }

        State& nextState =
            layer.states[layer.nextStateIndex];

        const float loadedNextLength =
            GetStateLength(nextState);

        const float nextLength =
            loadedNextLength > 0.0f
            ? loadedNextLength
            : 1.0f;

        layer.nextTime +=
            dt * nextState.speed;

        EvaluateCallbacks(
            nextState,
            layer.nextTime,
            nextLength);

        if (layer.nextTime > nextLength)
        {
            if (nextState.loop)
            {
                layer.nextTime =
                    std::fmod(
                    layer.nextTime,
                    nextLength);
            }
            else
            {
                layer.nextTime =
                    nextLength;
            }
        }
        else if (layer.nextTime < 0.0f)
        {
            layer.nextTime = 0.0f;
        }

        if (dt > 0.0f)
        {
            layer.blendTime += dt;
        }

        float blendWeight =
            layer.blendDuration > 0.0f
            ? layer.blendTime /
            layer.blendDuration
            : 1.0f;

        if (blendWeight < 0.0f)
        {
            blendWeight = 0.0f;
        }
        else if (blendWeight > 1.0f)
        {
            blendWeight = 1.0f;
        }

        ApplyDynamicTransition(
            currentState,
            layer.currentTime,
            nextState,
            layer.nextTime,
            blendWeight);

        if (blendWeight >= 1.0f)
        {
            for (auto& callback :
                currentState.callbacks)
            {
                if (!callback.entering)
                    continue;

                if (callback.onExit)
                {
                    callback.onExit(
                        currentState);
                }

                callback.entering = false;
            }

            layer.currentStateIndex =
                layer.nextStateIndex;

            layer.currentTime =
                layer.nextTime;

            layer.nextStateIndex = -1;
            layer.isTransitioning = false;
            layer.blendTime = 0.0f;

            return;
        }

        for (const Transition& transition :
            currentState.transitions)
        {
            if (!transition.canInterrupt)
            {
                continue;
            }

            if (transition.toStateIndex ==
                layer.nextStateIndex)
            {
                continue;
            }

            if (transition.toStateIndex < 0 ||
                transition.toStateIndex >=
                static_cast<int>(
                layer.states.size()))
            {
                continue;
            }

            if (!CanTransition(
                transition,
                transitionNormalizedTime))
            {
                continue;
            }

            if (layer.nextStateIndex >= 0 &&
                layer.nextStateIndex <
                static_cast<int>(
                layer.states.size()))
            {
                for (auto& callback :
                    layer.states[
                        layer.nextStateIndex]
                    .callbacks)
                {
                    callback.entering = false;
                }
            }

            layer.nextStateIndex =
                transition.toStateIndex;

            layer.nextTime = 0.0f;
            layer.blendTime = 0.0f;

            layer.blendDuration =
                transition.transitionDuration;

            if (layer.blendDuration < 0.0f)
            {
                layer.blendDuration = 0.0f;
            }

            for (auto& callback :
                layer.states[
                    layer.nextStateIndex]
                .callbacks)
            {
                callback.entering = false;
            }

            if (layer.blendDuration <= 0.0f)
            {
                for (auto& callback :
                    currentState.callbacks)
                {
                    if (!callback.entering)
                        continue;

                    if (callback.onExit)
                    {
                        callback.onExit(
                            currentState);
                    }

                    callback.entering = false;
                }

                layer.currentStateIndex =
                    layer.nextStateIndex;

                layer.currentTime = 0.0f;
                layer.nextStateIndex = -1;
                layer.isTransitioning = false;

                ApplyDynamicState(
                    layer.states[
                        layer.currentStateIndex],
                        0.0f);
            }

            break;
        }

        return;
    }

    if (!currentState.blockAnyStateTransitions)
    {
        for (const Transition& transition :
            layer.anyStateTransitions)
        {
            if (transition.toStateIndex ==
                layer.currentStateIndex)
            {
                continue;
            }

            if (std::find(
                transition
                .excludedFromStateIndices
                .begin(),
                transition
                .excludedFromStateIndices
                .end(),
                layer.currentStateIndex) !=
                transition
                .excludedFromStateIndices
                .end())
            {
                continue;
            }

            if (!CanTransition(
                transition,
                transitionNormalizedTime))
            {
                continue;
            }

            if (currentLooped &&
                transition.hasExitTime)
            {
                layer.currentTime =
                    currentLength;
            }

            if (BeginTransition(transition))
            {
                if (layer.isTransitioning)
                {
                    ApplyDynamicTransition(
                        currentState,
                        layer.currentTime,
                        layer.states[
                            layer.nextStateIndex],
                            0.0f,
                            0.0f);
                }

                return;
            }
        }
    }

    for (const Transition& transition :
        currentState.transitions)
    {
        if (!CanTransition(
            transition,
            transitionNormalizedTime))
        {
            continue;
        }

        if (currentLooped &&
            transition.hasExitTime)
        {
            layer.currentTime =
                currentLength;
        }

        if (BeginTransition(transition))
        {
            if (layer.isTransitioning)
            {
                ApplyDynamicTransition(
                    currentState,
                    layer.currentTime,
                    layer.states[
                        layer.nextStateIndex],
                        0.0f,
                        0.0f);
            }

            return;
        }
    }

    ApplyDynamicState(
        currentState,
        layer.currentTime);
}

void Animator::ApplyDynamicState(const State& state, float time)
{
    const std::shared_ptr<DynamicAnimationClip> clip =
        GetDynamicClip(state.dynamicClipPath);
    if (!clip)
        return;

    for (const DynamicAnimationTrack& track : clip->tracks)
    {
        ApplyDynamicTrack(
            track,
            EvaluateDynamicAnimationTrack(track, time));
    }
}

void Animator::ApplyDynamicTransition(
    const State& currentState,
    float currentTime,
    const State& nextState,
    float nextTime,
    float blendWeight)
{
    const std::shared_ptr<DynamicAnimationClip> currentClip =
        GetDynamicClip(currentState.dynamicClipPath);
    const std::shared_ptr<DynamicAnimationClip> nextClip =
        GetDynamicClip(nextState.dynamicClipPath);

    if (!currentClip)
    {
        ApplyDynamicState(nextState, nextTime);
        return;
    }

    if (!nextClip)
    {
        ApplyDynamicState(currentState, currentTime);
        return;
    }

    for (const DynamicAnimationTrack& currentTrack : currentClip->tracks)
    {
        const ::ParamValue currentValue =
            EvaluateDynamicAnimationTrack(currentTrack, currentTime);
        const DynamicAnimationTrack* nextTrack =
            FindMatchingTrack(*nextClip, currentTrack);

        if (nextTrack && currentTrack.valueType == nextTrack->valueType)
        {
            const ::ParamValue nextValue =
                EvaluateDynamicAnimationTrack(*nextTrack, nextTime);
            ApplyDynamicTrack(
                currentTrack,
                BlendDynamicAnimationValue(currentValue, nextValue, blendWeight));
        }
        else
        {
            ApplyDynamicTrack(currentTrack, currentValue);
        }
    }

    for (const DynamicAnimationTrack& nextTrack : nextClip->tracks)
    {
        if (FindMatchingTrack(*currentClip, nextTrack))
            continue;

        ApplyDynamicTrack(
            nextTrack,
            EvaluateDynamicAnimationTrack(nextTrack, nextTime));
    }
}

void Animator::ApplyDynamicTrack(
    const DynamicAnimationTrack& track,
    const ::ParamValue& value)
{
    Widget* widget = dynamic_cast<Widget*>(owner);

    if (track.target == DynamicAnimationTarget::WidgetProperty)
    {
        switch (track.widgetProperty)
        {
        case DynamicWidgetProperty::Position:
            if (const Vector2* v = std::get_if<Vector2>(&value))
                widget->rect.position = *v;
            break;

        case DynamicWidgetProperty::Angle:
            if (const float* v = std::get_if<float>(&value))
                widget->rect.angle = *v;
            break;

        case DynamicWidgetProperty::Size:
            if (const Vector2* v = std::get_if<Vector2>(&value))
                widget->rect.size = *v;
            break;

        case DynamicWidgetProperty::Anchor:
            if (const Vector2* v = std::get_if<Vector2>(&value))
                widget->rect.anchor = *v;
            break;
        }
        return;
    }

    if (SpriteRenderComponent* sprite =
        widget->GetComponent<SpriteRenderComponent>())
    {
        sprite->SetShaderParam(track.shaderParamName, value);
    }
}

std::shared_ptr<DynamicAnimationClip> Animator::GetDynamicClip(
    const std::string& path) const
{
    if (path.empty())
    {
        dynamicAnimationError = "Dynamic animation clip path is empty.";
        return nullptr;
    }

    auto cached = dynamicClipCache.find(path);
    if (cached != dynamicClipCache.end())
    {
        return cached->second;
    }

    auto clip = std::make_shared<DynamicAnimationClip>();
    std::string error;
    if (!DynamicAnimationSerializer::Load(path, *clip, &error))
    {
        dynamicAnimationError =
            "Failed to load .danim: " + path + " / " + error;
        OutputDebugStringA((dynamicAnimationError + "\n").c_str());
        return nullptr;
    }

    dynamicAnimationError.clear();
    dynamicClipCache[path] = clip;

    long long writeStamp = 0;
    if (TryGetFileWriteStamp(path, writeStamp))
    {
        dynamicClipWriteStamps[path] = writeStamp;
    }

    return clip;
}

void Animator::ReloadActiveDynamicClipsIfChanged()
{
    std::unordered_set<std::string> activeClipPaths;

    for (const AnimatorLayer& layer : layers)
    {
        if (layer.currentStateIndex >= 0 &&
            layer.currentStateIndex < static_cast<int>(layer.states.size()))
        {
            const std::string& currentPath =
                layer.states[layer.currentStateIndex].dynamicClipPath;

            if (!currentPath.empty())
            {
                activeClipPaths.insert(currentPath);
            }
        }

        if (layer.isTransitioning &&
            layer.nextStateIndex >= 0 &&
            layer.nextStateIndex < static_cast<int>(layer.states.size()))
        {
            const std::string& nextPath =
                layer.states[layer.nextStateIndex].dynamicClipPath;

            if (!nextPath.empty())
            {
                activeClipPaths.insert(nextPath);
            }
        }
    }

    for (const std::string& path : activeClipPaths)
    {
        ReloadDynamicClipIfChanged(path);
    }
}

bool Animator::ReloadDynamicClipIfChanged(const std::string& path)
{
    if (path.empty())
    {
        return false;
    }

    long long currentWriteStamp = 0;
    if (!TryGetFileWriteStamp(path, currentWriteStamp))
    {
        return false;
    }

    auto stampIt = dynamicClipWriteStamps.find(path);
    if (stampIt == dynamicClipWriteStamps.end())
    {
        dynamicClipWriteStamps[path] = currentWriteStamp;
        return false;
    }

    if (stampIt->second == currentWriteStamp)
    {
        return false;
    }

    auto reloadedClip = std::make_shared<DynamicAnimationClip>();
    std::string error;

    if (!DynamicAnimationSerializer::Load(
        path,
        *reloadedClip,
        &error))
    {
        dynamicAnimationError =
            "Failed to auto reload .danim: " + path + " / " + error;
        OutputDebugStringA((dynamicAnimationError + "\n").c_str());

        return false;
    }

    dynamicClipCache[path] = reloadedClip;
    stampIt->second = currentWriteStamp;
    dynamicAnimationError.clear();
    return true;
}

const DynamicAnimationTrack* Animator::FindMatchingTrack(
    const DynamicAnimationClip& clip,
    const DynamicAnimationTrack& sourceTrack) const
{
    for (const DynamicAnimationTrack& track : clip.tracks)
    {
        if (track.target != sourceTrack.target)
            continue;

        if (track.target == DynamicAnimationTarget::WidgetProperty)
        {
            if (track.widgetProperty == sourceTrack.widgetProperty)
                return &track;
        }
        else if (track.shaderParamName == sourceTrack.shaderParamName)
        {
            return &track;
        }
    }

    return nullptr;
}

float Animator::GetStateLength(const State& state) const
{
    if (animationMode == AnimationMode::VMDLModel)
    {
        if (!model || state.animationIndex < 0 ||
            state.animationIndex >= static_cast<int>(model->GetAnimations().size()))
        {
            return 0.0f;
        }

        return model->GetAnimations()[state.animationIndex].secondsLength;
    }

    const std::shared_ptr<DynamicAnimationClip> clip =
        GetDynamicClip(state.dynamicClipPath);
    return clip ? clip->length : 0.0f;
}

std::string Animator::GetStateAnimationName(const State& state) const
{
    if (animationMode == AnimationMode::VMDLModel)
    {
        if (!model || state.animationIndex < 0 ||
            state.animationIndex >= static_cast<int>(model->GetAnimations().size()))
        {
            return "(none)";
        }

        return model->GetAnimations()[state.animationIndex].name;
    }

    if (state.dynamicClipPath.empty())
        return "(none)";

    const std::size_t separator = state.dynamicClipPath.find_last_of("\\/");
    return separator == std::string::npos
        ? state.dynamicClipPath
        : state.dynamicClipPath.substr(separator + 1);
}

bool Animator::SetDynamicClipPath(
    int layerIndex,
    int stateIndex,
    const std::string& path)
{
    if (animationMode != AnimationMode::Dynamic)
        return false;
    if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size()))
        return false;
    if (stateIndex < 0 ||
        stateIndex >= static_cast<int>(layers[layerIndex].states.size()))
    {
        return false;
    }

    AnimatorLayer& layer = layers[layerIndex];
    State& state = layer.states[stateIndex];

    const std::string oldPath = state.dynamicClipPath;
    state.dynamicClipPath = path;

    if (!oldPath.empty())
    {
        dynamicClipCache.erase(oldPath);
        dynamicClipWriteStamps.erase(oldPath);
    }
    if (!path.empty())
    {
        dynamicClipCache.erase(path);
        dynamicClipWriteStamps.erase(path);
    }

    const bool loaded = path.empty() || GetDynamicClip(path) != nullptr;
    if (loaded && layer.currentStateIndex == stateIndex)
    {
        ApplyDynamicState(state, layer.currentTime);
    }

    return loaded;
}

void Animator::ReloadDynamicClips()
{
    dynamicClipCache.clear();
    dynamicClipWriteStamps.clear();
    dynamicAnimationError.clear();
    dynamicClipWatchTimer = 0.0f;
}

VMDLModel::NodePose Animator::SampleNodePose(int animIndex, float time, int nodeIdx)
{
    static std::vector<VMDLModel::NodePose> tempPoses;
    model->ComputeAnimation(animIndex, time, tempPoses);
    if (nodeIdx >= 0 && nodeIdx < (int)tempPoses.size())
    {
        return tempPoses[nodeIdx];
    }
    return VMDLModel::NodePose{}; // Identity
}

void Animator::BindCallbacks()
{
    for (auto& layer : layers)
        for (auto& state : layer.states)
            for (auto& cb : state.callbacks)
            {
                auto it = g_AnimCallbackRegistry.find(cb.label);
                if (it != g_AnimCallbackRegistry.end())
                {
                    cb.onEnter = it->second.a;
                    cb.onExit  = it->second.b;
                }
            }
}

void Animator::Play(int layerIndex, int animationIndex, bool loop)
{
    _ASSERT_EXPR(layerIndex >= 0 && layerIndex < (int)layers.size(), L"Invalid layer index");

    auto& layer = layers[layerIndex];
    int stateIndex = -1;
    for (size_t i = 0; i < layer.states.size(); ++i)
        if (layer.states[i].animationIndex == animationIndex)
        {
            stateIndex = (int)i;
            break;
        }
    if (stateIndex < 0) return;
    SetDefaultState(layerIndex, stateIndex);
}

void Animator::Stop(int layerIndex)
{
    auto& layer = layers[layerIndex];
    layer.currentStateIndex = -1;
    layer.nextStateIndex = -1;
    layer.currentTime = 0.0f;
    layer.nextTime = 0.0f;
    layer.blendTime = 0.0f;
    layer.blendDuration = 0.0f;
    layer.isTransitioning = false;
}

void Animator::SetRootMotion(const std::string& name)
{
    if (animationMode != AnimationMode::VMDLModel)
        return;

    rootNodeName = name;
    useRootMotion = true;
    rootNodeIndex = -1;

    if (!model) return;

    const auto& nodes = model->GetNodes();
    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        if (nodes[i].name == rootNodeName)
        {
            rootNodeIndex = i;
            break;
        }
    }

    _ASSERT_EXPR(rootNodeIndex != -1, L"Root Motion node not found in VMDLModel!");
}

bool Animator::EvaluateCondition(const Condition& c) const
{
    if (c.mode == ConditionMode::Trigger)
    {
        auto it = triggers.find(c.paramName);
        return it != triggers.end() && it->second;
    }
    auto it = parameters.find(c.paramName);
    if (it == parameters.end()) return false;

    const ParamValue& val = it->second;
    const ParamValue& thr = c.threshold;

    if (std::holds_alternative<float>(val))
    {
        float v = std::get<float>(val);
        float t = std::holds_alternative<float>(thr) ? std::get<float>(thr)
            : std::holds_alternative<int>(thr)   ? (float)std::get<int>(thr)
            : 0.0f;
        switch (c.mode) {
            case ConditionMode::Greater:   return v > t;
            case ConditionMode::Less:      return v < t;
            case ConditionMode::Equals:    return v == t;
            case ConditionMode::NotEquals: return v != t;
            default: return false;
        }
    }
    else if (std::holds_alternative<int>(val))
    {
        int v = std::get<int>(val);
        int t = std::holds_alternative<int>(thr) ? std::get<int>(thr)
            : std::holds_alternative<float>(thr) ? (int)std::get<float>(thr)
            : 0;
        switch (c.mode) {
            case ConditionMode::Greater:   return v > t;
            case ConditionMode::Less:      return v < t;
            case ConditionMode::Equals:    return v == t;
            case ConditionMode::NotEquals: return v != t;
            default: return false;
        }
    }
    else if (std::holds_alternative<bool>(val))
    {
        bool v = std::get<bool>(val);
        switch (c.mode) {
            case ConditionMode::IsTrue:  return v;
            case ConditionMode::IsFalse: return !v;
            default: return false;
        }
    }
    return false;
}

bool Animator::EvaluateTransition(
    const Transition& transition) const
{
    return EvaluateTransition(
        transition,
        0.0f);
}

bool Animator::EvaluateTransition(
    const Transition& transition,
    float normalizedTime) const
{
    if (!transition.isAny)
    {
        float progress = normalizedTime;

        if (progress < 0.0f)
        {
            progress = 0.0f;
        }
        else if (progress > 1.0f)
        {
            progress = 1.0f;
        }

        if (progress < transition.sourceProgressMin)
        {
            return false;
        }

        if (transition.sourceProgressMax < 1.0f &&
            progress > transition.sourceProgressMax)
        {
            return false;
        }
    }

    for (const Condition& condition : transition.conditions)
    {
        if (!EvaluateCondition(condition))
        {
            return false;
        }
    }

    return true;
}

void Animator::EvaluateCallbacks(State& state, float currentTime, float animLength)
{
    for (auto& callback : state.callbacks)
    {
        float nowPer = animLength > 0.0f ? currentTime / animLength : 0.0f;
        if (nowPer > callback.enterTimePer && nowPer < callback.exitTimePer)
        {
            if (!callback.entering)
            {
                if (callback.onEnter) callback.onEnter(state);
                callback.entering = true;
            }
        }
        else if (nowPer >= callback.exitTimePer)
        {
            if (callback.entering)
            {
                if (callback.onExit) callback.onExit(state);
                callback.entering = false;
            }
        }
    }
}

const std::string& Animator::GetCurrentStateName(int li) const
{
    static std::string empty;
    const auto& layer = layers[li];
    if (layer.currentStateIndex < 0) return empty;
    return layer.states[layer.currentStateIndex].name;
}

int Animator::GetCurrentAnimationIndex(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size())) return -1;

    const AnimatorLayer& layer = layers[layerIndex];
    if (layer.currentStateIndex < 0) return -1;
    if (layer.currentStateIndex >= static_cast<int>(layer.states.size())) return -1;

    return layer.states[layer.currentStateIndex].animationIndex;
}

float Animator::GetCurrentAnimationTime(int layerIndex) const
{
    if (layerIndex < 0 || layerIndex >= static_cast<int>(layers.size())) return 0.0f;
    return layers[layerIndex].currentTime;
}
bool Animator::Save(const std::string& path)
{
    m_lastPath = path;
    return Serialize(path);
}

void Animator::Load(const std::string& path)
{
    m_lastPath = path;

    dynamicClipCache.clear();
    dynamicClipWriteStamps.clear();
    dynamicAnimationError.clear();
    dynamicClipWatchTimer = 0.0f;

    Deserialize(path);

    if (animationMode != AnimationMode::Dynamic)
    {
        return;
    }

    for (AnimatorLayer& layer : layers)
    {
        if (layer.states.empty())
        {
            layer.currentStateIndex = -1;
            continue;
        }

        if (layer.defaultStateIndex < 0 ||
            layer.defaultStateIndex >= static_cast<int>(layer.states.size()))
        {
            layer.defaultStateIndex = 0;
        }

        layer.currentStateIndex = layer.defaultStateIndex;
        layer.nextStateIndex = -1;
        layer.currentTime = 0.0f;
        layer.nextTime = 0.0f;
        layer.blendTime = 0.0f;
        layer.blendDuration = 0.0f;
        layer.isTransitioning = false;

        ApplyDynamicState(
            layer.states[layer.currentStateIndex],
            0.0f);
    }
}
