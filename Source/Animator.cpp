// Animator.cpp

#include "Animator.h"
#include "AnimEditorWindow.h"
#include "AnimatorSerializer.h"
#include "Actor.h"
#include "GameTime.h"

Animator::Animator(Object* owner, std::shared_ptr<Model> model, bool unscaledTime)
	: Component(owner), model(model), unscaledTime(unscaledTime)
{
    Actor* actor = dynamic_cast<Actor*>(owner);
    _ASSERT_EXPR(actor != nullptr, L"Object is not Actor");
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
    if (!model) return;

    // ルートモーションの差分をリセット（プレイヤーに毎フレーム新鮮な差分を渡すため）
    rootMotionVec = Vector3::Zero;
    rootMotionRot = Quaternion::Identity;

    // 遅延対策：初期化時にインデックスが見つかっていなければ再検索
    if (useRootMotion && rootNodeIndex == -1)
    {
        SetRootMotion(rootNodeName);
    }

    // ベースポーズ初期化
    int nodeCount = (int)model->GetNodes().size();
    std::vector<Model::NodePose> finalPoses(nodeCount);

    // レイヤー評価（この内部で rootMotionVec / rot が蓄積される）
    for (auto& layer : layers)
    {
        if (layer.currentStateIndex < 0) continue;
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
        // レイヤー情報を簡易表示
        for (int li = 0; li < (int)layers.size(); ++li)
        {
            const AnimatorLayer& layer = layers[li];
            ImGui::TextDisabled("[%d] %s  w=%.2f", li, layer.name.c_str(), layer.weight);
            if (layer.currentStateIndex >= 0)
            {
                const State& cur = layer.states[layer.currentStateIndex];
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f),
                    "  -> %s", cur.name.c_str());
            }
        }

        if (ImGui::Button("Go AnimEditor"))
            OpenAnimEditor();

        if (animEditor && animEditorOpen)
            animEditor->Draw(&animEditorOpen);

        ImGui::TreePop();
    }
}

void Animator::_print() const
{
    for(auto& anim : model->GetAnimations())
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
    // 範囲外
	_ASSERT_EXPR(layer.currentStateIndex >= 0 && layer.currentStateIndex < (int)layer.states.size(), L"Invalid current state index in layer");

    State& curState = layer.states[layer.currentStateIndex];
    const Model::Animation& curAnim = model->GetAnimations()[curState.animationIndex];

    float prevTime = layer.currentTime;

    // 時間更新
    layer.currentTime += (unscaledTime ? Game::Time::unscaledDeltaTime : Game::Time::deltaTime) * curState.speed;
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

            // posePrev.rotation の逆クォータニオンを invPrev に取得
            Quaternion invPrev;
            posePrev.rotation.Inverse(invPrev); 

            deltaRot = invPrev * poseCur.rotation;
        }
        else
        {
            Model::NodePose poseEnd   = SampleNodePose(curState.animationIndex, curAnim.secondsLength, rootNodeIndex);
            Model::NodePose poseStart = SampleNodePose(curState.animationIndex, 0.0f, rootNodeIndex);

            deltaPos = (poseEnd.position - posePrev.position) + (poseCur.position - poseStart.position);

            // それぞれの逆クォータニオンを計算
            Quaternion invPrev, invStart;
            posePrev.rotation.Inverse(invPrev);
            poseStart.rotation.Inverse(invStart);

            deltaRot = (invPrev * poseEnd.rotation) * (invStart * poseCur.rotation);
        }

        // レイヤーウェイトを乗算してメンバ変数に蓄積
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

        layer.nextTime += (unscaledTime ? Game::Time::unscaledDeltaTime : Game::Time::deltaTime) * nxtState.speed;
        if (layer.nextTime > nxtAnim.secondsLength)
        {
            if (nxtState.loop) layer.nextTime -= nxtAnim.secondsLength;
            else               layer.nextTime = nxtAnim.secondsLength;
        }

        model->ComputeAnimation(nxtState.animationIndex, layer.nextTime, nextNodePoses);

        layer.blendTime += (unscaledTime ? Game::Time::unscaledDeltaTime : Game::Time::deltaTime);
        float w = layer.blendDuration > 0.0f ? layer.blendTime / layer.blendDuration : 1.0f;
        if (w > 1.0f) w = 1.0f;

        // cur(w=0) → nxt(w=1) でブレンド（w=1 でも必ずブレンドしてから完了判定）
        for (size_t i = 0; i < nodePoses.size(); ++i)
        {
            nodePoses[i] = nodePoses[i].Lerp(nextNodePoses[i], w);
        }

        if (w >= 1.0f)
        {
            // ブレンド済みポーズのまま遷移完了
            layer.currentStateIndex = layer.nextStateIndex;
            layer.currentTime       = layer.nextTime;
            layer.nextStateIndex    = -1;
            layer.isTransitioning   = false;
            layer.blendTime         = 0.0f;
        }
        else
        {
            // 割り込みチェック（canInterrupt なトランジションが成立したら nxt を差し替え）
            for (const Transition& tr : curState.transitions)
            {
                if (!tr.canInterrupt) continue;
                if (tr.toStateIndex == layer.nextStateIndex) continue;
                if (EvaluateTransition(tr))
                {
                    layer.nextStateIndex  = tr.toStateIndex;
                    layer.nextTime        = 0.0f;
                    layer.blendTime       = 0.0f;
                    layer.blendDuration   = tr.transitionDuration;
                    // isTransitioning は既に true なので変更不要
                    break;
                }
            }
        }
    }
    else
    {
        // トランジション評価
        float normalizedTime = curAnim.secondsLength > 0.0f
            ? layer.currentTime / curAnim.secondsLength : 0.0f;

        // --- AnyState トランジション（優先評価） ---
        if (!curState.blockAnyStateTransitions)
        {
            for (const Transition& tr : layer.anyStateTransitions)
            {
                // 既に同じステートに遷移中の場合はスキップ
                if (tr.toStateIndex == layer.currentStateIndex) continue;
                if (std::find(tr.excludedFromStateIndices.begin(),
                    tr.excludedFromStateIndices.end(),
                    layer.currentStateIndex) != tr.excludedFromStateIndices.end())
                    continue;
                if (tr.hasExitTime && normalizedTime < tr.exitTime) continue;
                if (EvaluateTransition(tr, normalizedTime))
                {
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

        // AnyState で既に遷移が決まっていなければ通常トランジションを評価
        if (!layer.isTransitioning && layer.nextStateIndex < 0)
        {
        for (const Transition& tr : curState.transitions)
        {
            if (tr.hasExitTime && normalizedTime < tr.exitTime) continue;
            if (EvaluateTransition(tr, normalizedTime))
            {
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

        // 【重要】ルートモーション対象ノードはメッシュを動かさないよう原点に縛り付ける
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
    return EvaluateTransition(t, 0.0f);
}

bool Animator::EvaluateTransition(const Transition& t, float normalizedTime) const
{
    if (!t.isAny && normalizedTime < std::clamp(t.sourceProgressThreshold, 0.0f, 1.0f)) return false;

    // 条件が空の場合: hasExitTime トランジションなら条件なしで通過
    if (t.conditions.empty()) return t.hasExitTime;
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

bool Animator::Save(const std::string& path)
{
    m_lastPath = path;
    return AnimatorSerializer::Save(*this, path);
}

void Animator::Load(const std::string& path)
{
    m_lastPath = path;
    AnimatorSerializer::Load(*this, path);
}
