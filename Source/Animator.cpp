// Animator.cpp
#include "Animator.h"

Animator::Animator(Actor* owner, std::shared_ptr<Model> model)
    : Component(owner), model(model) {
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
    layers[li].states.push_back(s);
    return (int)layers[li].states.size() - 1;
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
    layer.currentStateIndex = stateIndex;
    layer.currentTime = 0.0f;
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

// =========================================================
// Update
// =========================================================
void Animator::Update(float elapsedTime)
{
    if (!model) return;

    // ベースポーズ初期化（全ノードをバインドポーズに）
    int nodeCount = (int)model->GetNodes().size();
    std::vector<Model::NodePose> finalPoses(nodeCount);

    // レイヤーを順番に評価してfinalPosesに書き込む
    for (auto& layer : layers)
    {
        if (layer.currentStateIndex < 0) continue;
        UpdateLayer(layer, elapsedTime, finalPoses);
    }

    model->SetNodePoses(finalPoses);
    model->UpdateTransform(Matrix::Identity);

    ResetTriggers();
}

void Animator::DrawGUI(float elapsedTime)
{
    if (ImGui::TreeNode("Animator"))
    {
        if (ImGui::Button("Go AnimEditor")) {

        }
        ImGui::TreePop();
    }
}

void Animator::UpdateLayer(AnimatorLayer& layer, float elapsedTime,
    std::vector<Model::NodePose>& finalPoses)
{
    State& curState = layer.states[layer.currentStateIndex];
    const Model::Animation& curAnim = model->GetAnimations()[curState.animationIndex];

    // 時間更新
    layer.currentTime += elapsedTime * curState.speed;
    if (layer.currentTime > curAnim.secondsLength)
    {
        if (curState.loop) layer.currentTime -= curAnim.secondsLength;
        else               layer.currentTime = curAnim.secondsLength;
    }

    // このレイヤーのポーズ計算
    model->ComputeAnimation(curState.animationIndex, layer.currentTime, nodePoses);

    // トランジション中はブレンド
    if (layer.isTransitioning)
    {
        State& nxtState = layer.states[layer.nextStateIndex];
        const Model::Animation& nxtAnim = model->GetAnimations()[nxtState.animationIndex];

        layer.nextTime += elapsedTime * nxtState.speed;
        if (layer.nextTime > nxtAnim.secondsLength)
        {
            if (nxtState.loop) layer.nextTime -= nxtAnim.secondsLength;
            else               layer.nextTime = nxtAnim.secondsLength;
        }

        model->ComputeAnimation(nxtState.animationIndex, layer.nextTime, nextNodePoses);

        layer.blendTime += elapsedTime;
        float w = layer.blendDuration > 0.0f ? layer.blendTime / layer.blendDuration : 1.0f;

        if (w >= 1.0f)
        {
            // 遷移完了
            layer.currentStateIndex = layer.nextStateIndex;
            layer.currentTime = layer.nextTime;
            layer.nextStateIndex = -1;
            layer.isTransitioning = false;
            layer.blendTime = 0.0f;
        }
        else
        {
            // cur と nxt をブレンド
            for (size_t i = 0; i < nodePoses.size(); ++i)
            {
                nodePoses[i].position = Vector3::Lerp(nodePoses[i].position, nextNodePoses[i].position, w);
                nodePoses[i].rotation = Quaternion::Slerp(nodePoses[i].rotation, nextNodePoses[i].rotation, w);
                nodePoses[i].scale = Vector3::Lerp(nodePoses[i].scale, nextNodePoses[i].scale, w);
            }
        }

        // 割り込みチェック
        for (const Transition& tr : curState.transitions)
        {
            if (!tr.canInterrupt) continue;
            if (tr.toStateIndex == layer.nextStateIndex) continue;
            if (EvaluateTransition(tr))
            {
                layer.nextStateIndex = tr.toStateIndex;
                layer.nextTime = 0.0f;
                layer.blendTime = 0.0f;
                layer.blendDuration = tr.transitionDuration;
                break;
            }
        }
    }
    else
    {
        // トランジション評価
        float normalizedTime = curAnim.secondsLength > 0.0f
            ? layer.currentTime / curAnim.secondsLength : 0.0f;

        for (const Transition& tr : curState.transitions)
        {
            if (tr.hasExitTime&& normalizedTime < tr.exitTime) continue;
            if (EvaluateTransition(tr))
            {
                layer.nextStateIndex = tr.toStateIndex;
                layer.nextTime = 0.0f;
                layer.blendTime = 0.0f;
                layer.blendDuration = tr.transitionDuration;
                layer.isTransitioning = tr.transitionDuration > 0.0f;

                if (!layer.isTransitioning)
                {
                    layer.currentStateIndex = layer.nextStateIndex;
                    layer.currentTime = 0.0f;
                    layer.nextStateIndex = -1;
                }
                break;
            }
        }
    }

    // マスクに従って finalPoses に書き込む
    for (int ni = 0; ni < (int)nodePoses.size(); ++ni)
    {
        if (!layer.mask.Contains(ni)) continue;

        if (layer.blendMode == BlendMode::Override)
        {
            // weightで下のレイヤーとブレンド
            finalPoses[ni].position = Vector3::Lerp(
                finalPoses[ni].position, nodePoses[ni].position, layer.weight);
            finalPoses[ni].rotation = Quaternion::Slerp(
                finalPoses[ni].rotation, nodePoses[ni].rotation, layer.weight);
            finalPoses[ni].scale = Vector3::Lerp(
                finalPoses[ni].scale, nodePoses[ni].scale, layer.weight);
        }
        else // Additive
        {
            finalPoses[ni].position += nodePoses[ni].position * layer.weight;
            finalPoses[ni].rotation = finalPoses[ni].rotation * nodePoses[ni].rotation;
        }
    }
}

void Animator::Play(int layerIndex, int animationIndex, bool loop)
{
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
        float v = std::get<float>(val), t = std::get<float>(thr);
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
        int v = std::get<int>(val), t = std::get<int>(thr);
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
    if (t.conditions.empty()) return false;
    for (const Condition& c : t.conditions)
        if (!EvaluateCondition(c)) return false;
    return true;
}

const std::string& Animator::GetCurrentStateName(int li) const
{
    static std::string empty;
    const auto& layer = layers[li];
    if (layer.currentStateIndex < 0) return empty;
    return layer.states[layer.currentStateIndex].name;
}
