// Animator.cpp

#include "Animator.h"
#include "AnimEditorWindow.h"
#include "AnimatorSerializer.h"
#include "Actor.h"
#include "GameTime.h"
#include "DynamicAnimationSerializer.h"
#include "Widget.h"
#include "SpriteRenderComponent.h"

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

Animator::Animator(Object* owner, std::shared_ptr<Model> model, bool unscaledTime)
    : Component(owner), animationMode(AnimationMode::Model), model(model), unscaledTime(unscaledTime)
{
    Component::GetOwnerAsActor();
}

Animator::Animator(Object* owner, bool unscaledTime)
    : Component(owner), animationMode(AnimationMode::Dynamic), unscaledTime(unscaledTime)
{
    Component::GetOwnerAsWidget();
}

// =========================================================
// レイヤー操作
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
    // ランタイム状態はリセット
    copy.currentStateIndex = copy.defaultStateIndex;
    copy.currentTime = 0.0f;
    copy.isTransitioning = false;
    copy.nextStateIndex = -1;
    copy.blendTime = 0.0f;
    layers.insert(layers.begin() + layerIndex + 1, std::move(copy));
}

// =========================================================
// ステート操作
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
// AnyState トランジション
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
// パラメータ（全レイヤー共有）
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
        dynamicClipWatchTimer += max(
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
    std::vector<Model::NodePose> finalPoses(nodeCount);

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
    if (ImGui::TreeNode("Animator"))
    {
        ImGui::TextDisabled(
            animationMode == AnimationMode::Dynamic
                ? "Mode: Dynamic (.danim)"
                : "Mode: Model");

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

        if (ImGui::Button("Go AnimEditor"))
            OpenAnimEditor();

        ImGui::TreePop();
    }

    if (animEditor && animEditorOpen)
        animEditor->Draw(&animEditorOpen);
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

void Animator::OpenAnimEditor()
{
    if (!animEditor)
        animEditor = std::make_unique<AnimEditorWindow>(this);
    animEditorOpen = true;
}

void Animator::UpdateLayer(AnimatorLayer& layer,
                           std::vector<Model::NodePose>& finalPoses)
{
    _ASSERT_EXPR(layer.currentStateIndex >= 0 && layer.currentStateIndex < (int)layer.states.size(), L"Invalid current state index in layer");

    State& curState = layer.states[layer.currentStateIndex];
    const Model::Animation& curAnim = model->GetAnimations()[curState.animationIndex];

    float prevTime = layer.currentTime;
    float dt = unscaledTime ? Game::Time::unscaledDeltaTime : Game::Time::deltaTime;

    // 時間更新
    layer.currentTime += dt * curState.speed;
    EvaluateCallbacks(curState, layer.currentTime, curAnim.secondsLength);

    bool looped = false;
    if (layer.currentTime > curAnim.secondsLength)
    {
        if (curState.loop) {
            layer.currentTime -= curAnim.secondsLength;
            looped = true;
        }
        else {
            layer.currentTime = curAnim.secondsLength;
        }
    }

    // --- 【ルートモーションの抽出】 ---
    if (useRootMotion && rootNodeIndex != -1)
    {
        Model::NodePose posePrev = SampleNodePose(curState.animationIndex, prevTime, rootNodeIndex);
        Model::NodePose poseCur  = SampleNodePose(curState.animationIndex, layer.currentTime, rootNodeIndex);

        Vector3 deltaPos;
        Quaternion deltaRot;

        if (!looped)
        {
            deltaPos = poseCur.position - posePrev.position;
            Quaternion invPrev;
            posePrev.rotation.Inverse(invPrev);
            deltaRot = invPrev * poseCur.rotation;
        }
        else
        {
            Model::NodePose poseEnd   = SampleNodePose(curState.animationIndex, curAnim.secondsLength, rootNodeIndex);
            Model::NodePose poseStart = SampleNodePose(curState.animationIndex, 0.0f, rootNodeIndex);

            deltaPos = (poseEnd.position - posePrev.position) + (poseCur.position - poseStart.position);

            Quaternion invPrev, invStart;
            posePrev.rotation.Inverse(invPrev);
            poseStart.rotation.Inverse(invStart);
            deltaRot = (invPrev * poseEnd.rotation) * (invStart * poseCur.rotation);
        }

        rootMotionVec += deltaPos * layer.weight;
        rootMotionRot = rootMotionRot * Quaternion::Lerp(Quaternion::Identity, deltaRot, layer.weight);
    }

    // カレントポーズの計算
    model->ComputeAnimation(curState.animationIndex, layer.currentTime, nodePoses);

    // トランジション中はブレンド
    if (layer.isTransitioning)
    {
        State& nxtState = layer.states[layer.nextStateIndex];
        const Model::Animation& nxtAnim = model->GetAnimations()[nxtState.animationIndex];

        layer.nextTime += dt * nxtState.speed;
        if (layer.nextTime > nxtAnim.secondsLength)
        {
            if (nxtState.loop) layer.nextTime -= nxtAnim.secondsLength;
            else               layer.nextTime = nxtAnim.secondsLength;
        }

        EvaluateCallbacks(nxtState, layer.nextTime, nxtAnim.secondsLength);

        model->ComputeAnimation(nxtState.animationIndex, layer.nextTime, nextNodePoses);

        layer.blendTime += dt;
        float w = layer.blendDuration > 0.0f ? layer.blendTime / layer.blendDuration : 1.0f;
        if (w > 1.0f) w = 1.0f;

        for (size_t i = 0; i < nodePoses.size(); ++i)
            nodePoses[i] = nodePoses[i].Lerp(nextNodePoses[i], w);

        if (w >= 1.0f)
        {
            // curState の残存コールバックを強制 onExit
            for (auto& cb : curState.callbacks)
            {
                if (cb.entering)
                {
                    if (cb.onExit) cb.onExit(curState);
                    cb.entering = false;
                }
            }
            layer.currentStateIndex = layer.nextStateIndex;
            layer.currentTime       = layer.nextTime;
            layer.nextStateIndex    = -1;
            layer.isTransitioning   = false;
            layer.blendTime         = 0.0f;
        }
        else
        {
            // 割り込みチェック
            for (const Transition& tr : curState.transitions)
            {
                if (!tr.canInterrupt) continue;
                if (tr.toStateIndex == layer.nextStateIndex) continue;
                if (EvaluateTransition(tr))
                {
                    // 差し替え前に古い nextState の entering をリセット
                    for (auto& cb : layer.states[layer.nextStateIndex].callbacks)
                        cb.entering = false;

                    layer.nextStateIndex = tr.toStateIndex;
                    layer.nextTime       = 0.0f;
                    layer.blendTime      = 0.0f;
                    layer.blendDuration  = tr.transitionDuration;

                    // 新しい nextState の entering をリセット
                    for (auto& cb : layer.states[layer.nextStateIndex].callbacks)
                        cb.entering = false;
                    break;
                }
            }
        }
    }
    else
    {
        float normalizedTime = curAnim.secondsLength > 0.0f
            ? layer.currentTime / curAnim.secondsLength : 0.0f;

        // AnyState トランジション（優先評価）
        if (!curState.blockAnyStateTransitions)
        {
            for (const Transition& tr : layer.anyStateTransitions)
            {
                if (tr.toStateIndex == layer.currentStateIndex) continue;
                if (std::find(tr.excludedFromStateIndices.begin(),
                    tr.excludedFromStateIndices.end(),
                    layer.currentStateIndex) != tr.excludedFromStateIndices.end())
                    continue;
                if (tr.hasExitTime && normalizedTime < tr.exitTime) continue;
                if (EvaluateTransition(tr, normalizedTime))
                {
                    // nextState の entering をリセット
                    for (auto& cb : layer.states[tr.toStateIndex].callbacks)
                        cb.entering = false;

                    layer.nextStateIndex  = tr.toStateIndex;
                    layer.nextTime        = 0.0f;
                    layer.blendTime       = 0.0f;
                    layer.blendDuration   = tr.transitionDuration;
                    layer.isTransitioning = tr.transitionDuration > 0.0f;

                    if (!layer.isTransitioning)
                    {
                        layer.currentStateIndex = layer.nextStateIndex;
                        layer.currentTime       = 0.0f;
                        layer.nextStateIndex    = -1;
                    }
                    break;
                }
            }
        }

        // 通常トランジション
        if (!layer.isTransitioning && layer.nextStateIndex < 0)
        {
            for (const Transition& tr : curState.transitions)
            {
                if (tr.hasExitTime && normalizedTime < tr.exitTime) continue;
                if (EvaluateTransition(tr, normalizedTime))
                {
                    // nextState の entering をリセット
                    for (auto& cb : layer.states[tr.toStateIndex].callbacks)
                        cb.entering = false;

                    layer.nextStateIndex  = tr.toStateIndex;
                    layer.nextTime        = 0.0f;
                    layer.blendTime       = 0.0f;
                    layer.blendDuration   = tr.transitionDuration;
                    layer.isTransitioning = tr.transitionDuration > 0.0f;

                    if (!layer.isTransitioning)
                    {
                        layer.currentStateIndex = layer.nextStateIndex;
                        layer.currentTime       = 0.0f;
                        layer.nextStateIndex    = -1;
                    }
                    break;
                }
            }
        }
    }

    // マスクに従って finalPoses に書き込む
    for (int ni = 0; ni < (int)nodePoses.size(); ++ni)
    {
        if (!layer.mask.Contains(ni)) continue;

        if (useRootMotion && ni == rootNodeIndex)
        {
            finalPoses[ni].position = Vector3::Zero;
            finalPoses[ni].rotation = Quaternion::Identity;
            continue;
        }

        if (layer.blendMode == BlendMode::Override)
        {
            finalPoses[ni] = finalPoses[ni].Lerp(nodePoses[ni], layer.weight);
        }
        else
        {
            finalPoses[ni].position += nodePoses[ni].position * layer.weight;
            finalPoses[ni].rotation = finalPoses[ni].rotation * nodePoses[ni].rotation;
        }
    }
}


void Animator::UpdateDynamicLayer(AnimatorLayer& layer)
{
    if (layer.currentStateIndex < 0 ||
        layer.currentStateIndex >= static_cast<int>(layer.states.size()))
    {
        return;
    }

    const float dt = unscaledTime
        ? Game::Time::unscaledDeltaTime
        : Game::Time::deltaTime;

    State& currentState = layer.states[layer.currentStateIndex];
    const float loadedCurrentLength = GetStateLength(currentState);
    const float currentLength = loadedCurrentLength > 0.0f
        ? loadedCurrentLength
        : 1.0f;

    layer.currentTime += dt * currentState.speed;
    EvaluateCallbacks(currentState, layer.currentTime, currentLength);

    if (layer.currentTime > currentLength)
    {
        if (currentState.loop)
        {
            layer.currentTime = std::fmod(layer.currentTime, currentLength);
        }
        else
        {
            layer.currentTime = currentLength;
        }
    }
    else if (layer.currentTime < 0.0f)
    {
        layer.currentTime = 0.0f;
    }

    auto beginTransition = [&](const Transition& transition) -> bool
    {
        if (transition.toStateIndex < 0 ||
            transition.toStateIndex >= static_cast<int>(layer.states.size()))
        {
            return false;
        }

        State& nextState = layer.states[transition.toStateIndex];
        for (auto& callback : nextState.callbacks)
            callback.entering = false;

        layer.nextStateIndex = transition.toStateIndex;
        layer.nextTime = 0.0f;
        layer.blendTime = 0.0f;
        layer.blendDuration = max(transition.transitionDuration, 0.0f);
        layer.isTransitioning = layer.blendDuration > 0.0f;

        if (!layer.isTransitioning)
        {
            for (auto& callback : currentState.callbacks)
            {
                if (callback.entering)
                {
                    if (callback.onExit)
                        callback.onExit(currentState);
                    callback.entering = false;
                }
            }

            layer.currentStateIndex = layer.nextStateIndex;
            layer.currentTime = 0.0f;
            layer.nextStateIndex = -1;
            ApplyDynamicState(layer.states[layer.currentStateIndex], 0.0f);
        }

        return true;
    };

    if (layer.isTransitioning)
    {
        if (layer.nextStateIndex < 0 ||
            layer.nextStateIndex >= static_cast<int>(layer.states.size()))
        {
            layer.isTransitioning = false;
            layer.nextStateIndex = -1;
            ApplyDynamicState(currentState, layer.currentTime);
            return;
        }

        State& nextState = layer.states[layer.nextStateIndex];
        const float loadedNextLength = GetStateLength(nextState);
        const float nextLength = loadedNextLength > 0.0f
            ? loadedNextLength
            : 1.0f;

        layer.nextTime += dt * nextState.speed;
        EvaluateCallbacks(nextState, layer.nextTime, nextLength);

        if (layer.nextTime > nextLength)
        {
            if (nextState.loop)
                layer.nextTime = std::fmod(layer.nextTime, nextLength);
            else
                layer.nextTime = nextLength;
        }
        else if (layer.nextTime < 0.0f)
        {
            layer.nextTime = 0.0f;
        }

        layer.blendTime += max(dt, 0.0f);
        const float blendWeight = layer.blendDuration > 0.0f
            ? std::clamp(layer.blendTime / layer.blendDuration, 0.0f, 1.0f)
            : 1.0f;

        ApplyDynamicTransition(
            currentState,
            layer.currentTime,
            nextState,
            layer.nextTime,
            blendWeight);

        if (blendWeight >= 1.0f)
        {
            for (auto& callback : currentState.callbacks)
            {
                if (callback.entering)
                {
                    if (callback.onExit)
                        callback.onExit(currentState);
                    callback.entering = false;
                }
            }

            layer.currentStateIndex = layer.nextStateIndex;
            layer.currentTime = layer.nextTime;
            layer.nextStateIndex = -1;
            layer.isTransitioning = false;
            layer.blendTime = 0.0f;
            return;
        }

        for (const Transition& transition : currentState.transitions)
        {
            if (!transition.canInterrupt)
                continue;
            if (transition.toStateIndex == layer.nextStateIndex)
                continue;
            if (transition.toStateIndex < 0 ||
                transition.toStateIndex >= static_cast<int>(layer.states.size()))
            {
                continue;
            }
            if (!EvaluateTransition(transition))
                continue;

            if (layer.nextStateIndex >= 0 &&
                layer.nextStateIndex < static_cast<int>(layer.states.size()))
            {
                for (auto& callback : layer.states[layer.nextStateIndex].callbacks)
                    callback.entering = false;
            }

            layer.nextStateIndex = transition.toStateIndex;
            layer.nextTime = 0.0f;
            layer.blendTime = 0.0f;
            layer.blendDuration = max(transition.transitionDuration, 0.0f);

            for (auto& callback : layer.states[layer.nextStateIndex].callbacks)
                callback.entering = false;

            if (layer.blendDuration <= 0.0f)
            {
                for (auto& callback : currentState.callbacks)
                {
                    if (callback.entering)
                    {
                        if (callback.onExit)
                            callback.onExit(currentState);
                        callback.entering = false;
                    }
                }

                layer.currentStateIndex = layer.nextStateIndex;
                layer.currentTime = 0.0f;
                layer.nextStateIndex = -1;
                layer.isTransitioning = false;
                ApplyDynamicState(layer.states[layer.currentStateIndex], 0.0f);
            }
            break;
        }

        return;
    }

    ApplyDynamicState(currentState, layer.currentTime);

    const float normalizedTime = currentLength > 0.0f
        ? layer.currentTime / currentLength
        : 0.0f;

    if (!currentState.blockAnyStateTransitions)
    {
        for (const Transition& transition : layer.anyStateTransitions)
        {
            if (transition.toStateIndex == layer.currentStateIndex)
                continue;
            if (std::find(
                transition.excludedFromStateIndices.begin(),
                transition.excludedFromStateIndices.end(),
                layer.currentStateIndex) != transition.excludedFromStateIndices.end())
            {
                continue;
            }
            if (transition.hasExitTime && normalizedTime < transition.exitTime)
                continue;
            if (EvaluateTransition(transition, normalizedTime) && beginTransition(transition))
                return;
        }
    }

    for (const Transition& transition : currentState.transitions)
    {
        if (transition.hasExitTime && normalizedTime < transition.exitTime)
            continue;
        if (EvaluateTransition(transition, normalizedTime) && beginTransition(transition))
            return;
    }
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
    Widget* widget = Component::GetOwnerAsWidget();

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

        // 保存途中の一時的な読込失敗を考慮し、
        // 更新日時は進めず次回の監視で再試行する。
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
    if (animationMode == AnimationMode::Model)
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
    if (animationMode == AnimationMode::Model)
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

// 特定ボーンのサンプリング用ヘルパー
Model::NodePose Animator::SampleNodePose(int animIndex, float time, int nodeIdx)
{
    static std::vector<Model::NodePose> tempPoses;
    model->ComputeAnimation(animIndex, time, tempPoses);
    if (nodeIdx >= 0 && nodeIdx < (int)tempPoses.size())
    {
        return tempPoses[nodeIdx];
    }
    return Model::NodePose{}; // Identity
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
    // 範囲外
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
    if (animationMode != AnimationMode::Model)
        return;

    rootNodeName = name;
    useRootMotion = true;
    rootNodeIndex = -1;

    if (!model) return;

    // モデルのノード群から名前が一致するインデックスを検索
    const auto& nodes = model->GetNodes();
    for (int i = 0; i < (int)nodes.size(); ++i)
    {
        if (nodes[i].name == rootNodeName)
        {
            rootNodeIndex = i;
            break;
        }
    }

    _ASSERT_EXPR(rootNodeIndex != -1, L"Root Motion node not found in Model!");
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
        // threshold が float で保存されている場合も安全に取り出す
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

bool Animator::EvaluateTransition(const Transition& t) const
{
    return EvaluateTransition(t, 0.0f);
}

bool Animator::EvaluateTransition(const Transition& t, float normalizedTime) const
{
    if (!t.isAny)
    {
        float progress = std::clamp(normalizedTime, 0.0f, 1.0f);
        if (progress < t.sourceProgressMin) return false;
        if (t.sourceProgressMax < 1.0f && progress > t.sourceProgressMax) return false;
    }

    // 条件が空の場合: hasExitTime トランジションなら条件なしで通過
    if (t.conditions.empty()) return t.hasExitTime;
    for (const Condition& c : t.conditions)
        if (!EvaluateCondition(c)) return false;
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

bool Animator::Save(const std::string& path)
{
    m_lastPath = path;
    return AnimatorSerializer::Save(*this, path);
}

void Animator::Load(const std::string& path)
{
    m_lastPath = path;

    dynamicClipCache.clear();
    dynamicClipWriteStamps.clear();
    dynamicAnimationError.clear();
    dynamicClipWatchTimer = 0.0f;

    AnimatorSerializer::Load(*this, path);

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
