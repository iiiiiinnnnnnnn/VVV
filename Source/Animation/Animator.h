// Animator.h

#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <imgui.h>
#include <imgui_node_editor.h>

#include "Core/Foundation/Common.h"
#include "Resource/Model.h"
#include "Core/Object/Component.h"
#include "Application/Tools/DynamicAnimation.h"

#define ANIM(name) model->GetAnimationIndex(name)

class Animator : public Component
{
public:
    enum class AnimationMode
    {
        Model,
        Dynamic
    };

    Animator(Object* owner, std::shared_ptr<Model> model, bool unscaledTime = false);
    Animator(Object* owner, bool unscaledTime = true);
    ~Animator() override;
    void Update() override;
    void DrawGUI() override;
	const char* GetDebugName() const override { return ICON_FA_FILM " Animator"; }
    int GetUpdateOrder() const override { return 100; }
    void _print() const; // デバッグ用

    // =========================================================
    // 型定義
    // =========================================================
    using ParamValue = std::variant<float, int, bool>;

    enum class ConditionMode
    {
        Greater, Less, Equals, NotEquals,
        IsTrue, IsFalse, Trigger,
    };

    enum class BlendMode
    {
        Override,   // 下のレイヤーを上書き（通常）
        Additive,   // 下のレイヤーに加算
    };

    struct Condition
    {
        std::string   paramName;
        ConditionMode mode = ConditionMode::IsTrue;
        ParamValue    threshold = 0.0f;
    };

    struct Transition
    {
        int                    toStateIndex = -1;
        std::vector<Condition> conditions;
        // AnyState 遷移用: この配列に含まれるステートIndexにいる間は遷移しない
        std::vector<int>       excludedFromStateIndices;
        float                  exitTime = 1.0f;
        float                  transitionDuration = 0.1f;
        bool                   hasExitTime = false;
        // true のときは遷移元再生率の条件を使わない（デフォルト）
        bool                   isAny = true;
        // isAny == false のとき、遷移元アニメの再生率がこの値以上なら遷移可能（0.0 - 1.0）
        float                  sourceProgressMin = 0.0f;  // 遷移可能な再生率の下限
        float                  sourceProgressMax = 1.0f;  // 遷移可能な再生率の上限（1.0=制限なし）
        int                    priority = 0;
        bool                   canInterrupt = false;
    };

    struct FootIKRange
    {
        std::string name = "FootIK";
        std::string targetName = "All";
        float startRatio = 0.0f;
        float endRatio = 1.0f;
        float weight = 1.0f;
        float fadeInRatio = 0.03f;
        float fadeOutRatio = 0.03f;
    };
    struct State
    {
        struct Callback
        {
            std::string label;
            float enterTimePer;
            float exitTimePer;
            std::function<void(const State& state)> onEnter;
            std::function<void(const State& state)> onExit;
            bool entering = false;
        };

        std::string             name;
        int                     animationIndex = -1;
        std::string             dynamicClipPath;
        float                   speed = 1.0f;
        bool                    loop = true;
        // true の場合、このステート滞在中は AnyState 遷移を評価しない
        bool                    blockAnyStateTransitions = false;
        // AnimEditor ノード座標（保存用）
        bool                    hasEditorPosition = false;
        float                   editorPosX = 0.0f;
        float                   editorPosY = 0.0f;
        std::vector<Transition> transitions;
        std::vector<FootIKRange> footIKRanges;

        std::vector<Callback> callbacks;
        void AddCallback(const std::string& label, float enterPer, float exitPer)
        {
            callbacks.push_back({label, enterPer, exitPer});
        }
    };

    // AvatarMask : 適用するノードIndexのセット（空 = 全ノード）
    struct AvatarMask
    {
        std::vector<int> nodes; // 空なら全身
        bool Contains(int nodeIndex) const
        {
            if (nodes.empty()) return true;
            for (int n : nodes) if (n == nodeIndex) return true;
            return false;
        }
    };

    // レイヤー本体
    struct AnimatorLayer
    {
        std::string  name;
        float        weight = 1.0f;
        BlendMode    blendMode = BlendMode::Override;
        AvatarMask   mask;                     // 空=全身

        // 独立したステートマシン
        std::vector<State> states;
        int   defaultStateIndex = 0;
        int   currentStateIndex = -1;
        int   nextStateIndex = -1;
        float currentTime = 0.0f;
        float nextTime = 0.0f;
        float blendTime = 0.0f;
        float blendDuration = 0.0f;
        bool  isTransitioning = false;

        // AnyState トランジション（どのステートからでも遷移できる）
        std::vector<Transition> anyStateTransitions;
        // AnimEditor AnyState ノード座標（保存用）
        bool  hasAnyStateEditorPosition = false;
        float anyStateEditorPosX = 0.0f;
        float anyStateEditorPosY = 0.0f;

        State* GetState(const std::string& name)
        {
            for (auto& s : states)
                if (s.name == name) return &s;
            return nullptr;
        }
    };

    // エディタウィンドウを開く
    void OpenEditor();

    int GetLayerCount() const { return (int)layers.size(); }

    const std::unordered_map<std::string, ParamValue>& GetParameters() const { return parameters; }
    const std::unordered_map<std::string, bool>&       GetTriggers()   const { return triggers; }
    // エディタ用：直接削除できるよう非const参照を返す
    std::unordered_map<std::string, ParamValue>& GetParameters_Mutable() { return parameters; }
    std::unordered_map<std::string, bool>&       GetTriggers_Mutable()   { return triggers; }

    // モデルへのアクセス (進捗バー描画用)
    std::shared_ptr<Model> GetModel() const { return model; }
    AnimationMode GetAnimationMode() const { return animationMode; }
    bool IsDynamicMode() const { return animationMode == AnimationMode::Dynamic; }

    float GetStateLength(const State& state) const;
    std::string GetStateAnimationName(const State& state) const;
    bool SetDynamicClipPath(int layerIndex, int stateIndex, const std::string& path);
    void ReloadDynamicClips();
    const std::string& GetDynamicAnimationError() const { return dynamicAnimationError; }

    // =========================================================
    // レイヤー操作
    // =========================================================
    // レイヤー追加、追加したレイヤーのIndexを返す
    int  AddLayer(const std::string& name,
                  BlendMode blendMode = BlendMode::Override,
                  float weight = 1.0f,
                  AvatarMask mask = {});

    void SetLayerWeight(int layerIndex, float weight);
    void SetLayerMask(int layerIndex, AvatarMask mask);
    AnimatorLayer& GetLayer(int layerIndex) { return layers[layerIndex]; }
    const AnimatorLayer& GetLayer(int layerIndex) const { return layers[layerIndex]; }
    void RemoveLayer(int layerIndex);
    void SwapLayers(int a, int b);
    void DuplicateLayer(int layerIndex);

    // =========================================================
    // ステート操作（レイヤーIndex指定）
    // =========================================================
    int  AddState(int layerIndex, const std::string& name,
                  int animationIndex, bool loop = true, float speed = 1.0f);

    int  AddDynamicState(int layerIndex, const std::string& name,
                         const std::string& clipPath = "",
                         bool loop = true, float speed = 1.0f);

    int  AddTransition(int layerIndex, int fromState, int toState,
                       float transitionDuration = 0.1f,
                       bool hasExitTime = false, float exitTime = 1.0f,
                       int priority = 0, bool canInterrupt = false);

    void AddCondition(int layerIndex, int fromState, int transitionIndex,
                      const std::string& paramName, ConditionMode mode,
                      ParamValue threshold = 0.0f);

    void SetDefaultState(int layerIndex, int stateIndex);

    // =========================================================
    // AnyState トランジション（どのステートからでも遷移できる）
    // =========================================================
    // AnyState から指定ステートへのトランジションを追加する。
    // 戻り値はそのレイヤーの anyStateTransitions 配列内のインデックス。
    int  AddAnyStateTransition(int layerIndex, int toState,
                               float transitionDuration = 0.1f,
                               bool hasExitTime = false, float exitTime = 1.0f,
                               int priority = 0, bool canInterrupt = false);

    void AddAnyStateCondition(int layerIndex, int transitionIndex,
                              const std::string& paramName, ConditionMode mode,
                              ParamValue threshold = 0.0f);

    // AnyState トランジション一覧を取得（エディタ用）
    const std::vector<Transition>& GetAnyStateTransitions(int layerIndex) const
    {
        return layers[layerIndex].anyStateTransitions;
    }
    std::vector<Transition>& GetAnyStateTransitions_Mutable(int layerIndex)
    {
        return layers[layerIndex].anyStateTransitions;
    }

    const std::string& GetCurrentStateName(int layerIndex = 0) const;
    int  GetCurrentStateIndex(int layerIndex = 0) const { return layers[layerIndex].currentStateIndex; }
    int  GetCurrentAnimationIndex(int layerIndex = 0) const;
    float GetCurrentAnimationTime(int layerIndex = 0) const;
    float EvaluateCurrentFootIKWeight(const std::string& targetName, int layerIndex = 0) const;

    // =========================================================
    // パラメータ（Animator全体で共有）
    // =========================================================
    void AddFloat(const std::string& name, float defaultValue = 0.0f);
    void AddInt(const std::string& name, int defaultValue = 0);
    void AddBool(const std::string& name, bool defaultValue = false);
    void AddTrigger(const std::string& name);

    void SetFloat(const std::string& name, float value);
    void SetInt(const std::string& name, int value);
    void SetBool(const std::string& name, bool value);
    void SetTrigger(const std::string& name);

    float GetFloat(const std::string& name) const { return parameters.find(name) != parameters.end() ? std::get<float>(parameters.at(name)) : 0.0f; }
    int   GetInt(const std::string& name)   const { return parameters.find(name) != parameters.end() ? std::get<int>(parameters.at(name)) : 0; }
    bool  GetBool(const std::string& name)  const { return parameters.find(name) != parameters.end() ? std::get<bool>(parameters.at(name)) : false; }

    // =========================================================
    // 直接再生（ステートマシンを使わない場合）
    // =========================================================
    void Play(int layerIndex, int animationIndex, bool loop = true);
    void Stop(int layerIndex);

    bool Save(const std::string& path);
    void Load(const std::string& path);
    const std::string& GetLastPath() const { return m_lastPath; }

    void ClearAll()
    {
        layers.clear();
        parameters.clear();
        triggers.clear();
        nodePoses.clear();
        nextNodePoses.clear();
        dynamicClipCache.clear();
    }

    // ルートモーションを有効化し、対象のノード名を設定する
    void SetRootMotion(const std::string& rootNodeName);

    // プレイヤーが毎フレーム取得して移動に利用する「ローカル空間の移動差分」
    Vector3 GetRootMotionVec() const { return rootMotionVec; }
    Quaternion GetRootMotionRot() const { return rootMotionRot; }

    // コールバック
    void BindCallbacks();
    void AddCallbackFunc(const std::string& label, std::function<void(const Animator::State&)> enter, std::function<void(const Animator::State&)> exit)
    {
        g_AnimCallbackRegistry[label] = {enter, exit};
    }

private:
    bool Serialize(const std::string& path) const;
    void Deserialize(const std::string& path);
    void DrawEditor(bool* open);
    void DrawLayerEditor(AnimatorLayer& layer, int layerIndex);
    void DrawNodes(AnimatorLayer& layer, int layerIndex);
    void DrawLinks(AnimatorLayer& layer, int layerIndex);
    void HandleInteractions(AnimatorLayer& layer, int layerIndex);
    void DrawParameterPanel();
    void DrawLayerSettings(AnimatorLayer& layer, int layerIndex);
    void DrawFootIKRangeEditor(State& state);
    void DrawAnimationKeyEditor(State& state);
    void DrawVectorKeys(const char* label, std::vector<Model::VectorKeyframe>& keys, float duration, const Vector3& defaultValue);
    void DrawQuaternionKeys(const char* label, std::vector<Model::QuaternionKeyframe>& keys, float duration, const Quaternion& defaultValue);
    void DrawStateDetail(AnimatorLayer& layer, int layerIndex);
    void DrawTransitionDetail(AnimatorLayer& layer);
    static ax::NodeEditor::PinId OutPin(int layerIndex, int stateIndex);
    static ax::NodeEditor::PinId InPin(int layerIndex, int stateIndex);
    static ax::NodeEditor::NodeId NodeId(int layerIndex, int stateIndex);
    static ax::NodeEditor::LinkId LinkId(int layerIndex, int fromState, int transitionIndex);
    static int DecodeOutPin(int layerIndex, ax::NodeEditor::PinId pin);
    static int DecodeInPin(int layerIndex, ax::NodeEditor::PinId pin);
    static void DecodeLinkId(ax::NodeEditor::LinkId id, int& layerIndex, int& fromState, int& transitionIndex);
    bool EvaluateCondition(const Condition& c) const;
    bool EvaluateTransition(const Transition& t) const;
    bool EvaluateTransition(const Transition& t, float normalizedTime) const;
    void EvaluateCallbacks(State& state, float currentTime, float animLength);
    void ResetTriggers();
    void UpdateLayer(AnimatorLayer& layer, std::vector<Model::NodePose>& finalPoses);
    void UpdateDynamicLayer(AnimatorLayer& layer);
    void ApplyDynamicState(const State& state, float time);
    void ApplyDynamicTransition(const State& currentState, float currentTime,
                                const State& nextState, float nextTime, float blendWeight);
    void ApplyDynamicTrack(const DynamicAnimationTrack& track, const ::ParamValue& value);
    std::shared_ptr<DynamicAnimationClip> GetDynamicClip(const std::string& path) const;
    void ReloadActiveDynamicClipsIfChanged();
    bool ReloadDynamicClipIfChanged(const std::string& path);
    const DynamicAnimationTrack* FindMatchingTrack(
        const DynamicAnimationClip& clip,
        const DynamicAnimationTrack& sourceTrack) const;

    AnimationMode animationMode = AnimationMode::Model;
    std::shared_ptr<Model> model;
    bool unscaledTime = false;
    mutable std::unordered_map<std::string, std::shared_ptr<DynamicAnimationClip>> dynamicClipCache;
    mutable std::unordered_map<std::string, long long> dynamicClipWriteStamps;
    mutable std::string dynamicAnimationError;
    float dynamicClipWatchTimer = 0.0f;
    static constexpr float DynamicClipWatchInterval = 0.25f;

    // レイヤーリスト（追加順に評価）
    std::vector<AnimatorLayer> layers;

    // Animator全体で共有するパラメータ
    std::unordered_map<std::string, ParamValue> parameters;
    std::unordered_map<std::string, bool>       triggers;

    std::vector<Model::NodePose> nodePoses;
    std::vector<Model::NodePose> nextNodePoses;

    std::string m_lastPath;
    ax::NodeEditor::EditorContext* editorContext = nullptr;
    bool editorOpen = false;
    int currentEditorLayer = 0;
    char currentAnimatorPath[MAX_PATH] = {};
    static constexpr int ANY_STATE_INDEX = 999;
    struct SelectedTransition
    {
        int layerIndex = -1;
        int fromStateIndex = -1;
        int transIndex = -1;
    };
    SelectedTransition selectedTransition;
    struct SelectedState { int layerIndex = -1; int stateIndex = -1; };
    SelectedState selectedState;
    std::unordered_map<int, bool> editorPositionSet;
    ImVec2 editorContextMenuPosition = {0.0f, 0.0f};
    ImVec2 editorCanvasMousePosition = {0.0f, 0.0f};
    bool pendingNodePlacement = false;
    int pendingNodeStateIndex = -1;
    ImVec2 pendingNodePosition = {0.0f, 0.0f};
    ax::NodeEditor::NodeId deleteNodeId;
    ax::NodeEditor::LinkId deleteLinkId;
    bool suppressNodeEditorInteractions = false;
    float editorLeftPanelWidth = 230.0f;
    bool addLayerPopupOpen = false;
    char addLayerName[64] = "New Layer";
    std::vector<bool> maskSelection;
    int contextLayerIndex = -1;
    int keyEditorNodeIndex = 0;

    struct two { std::function<void(const Animator::State&)> a, b; };
    std::unordered_map<std::string, two> g_AnimCallbackRegistry;

    // root motion

    bool useRootMotion = false;
    std::string rootNodeName = "";
    int rootNodeIndex = -1; // キャッシュされたルートノードのインデックス

    Vector3 rootMotionVec = Vector3::Zero;
    Quaternion rootMotionRot = Quaternion::Identity;

    // 1つのノードのポーズを特定時間からサンプリングするヘルパー
    Model::NodePose SampleNodePose(int animIndex, float time, int nodeIdx);
};


