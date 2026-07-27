// AIFlow.h

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Core/Object/Component.h"

class AIFlow : public Component
{
public:
    enum class ParameterType
    {
        Bool,
        Float,
        Vector3
    };

    enum class CompareOp
    {
        IsTrue,
        IsFalse,
        Less,
        LessEqual,
        Greater,
        GreaterEqual,
        Equal,
        NotEqual
    };

    enum class ConditionLogic
    {
        And,
        Or
    };

    struct Parameter
    {
        std::string name;
        ParameterType type = ParameterType::Float;
        bool boolValue = false;
        float floatValue = 0.0f;
        Vector3 vectorValue = Vector3::Zero;
        bool runtimeOnly = false;
    };

    struct Condition
    {
        std::string parameterName;
        CompareOp compare = CompareOp::IsTrue;
        float threshold = 0.0f;
    };

    struct Transition
    {
        int id = -1;
        int targetStateId = -1;
        ConditionLogic conditionLogic = ConditionLogic::And;
        std::vector<Condition> conditions;
    };

    struct State
    {
        int id = -1;
        std::string name = "State";
        std::string callbackName;
        std::vector<Transition> transitions;
        bool hasEditorPosition = false;
        float editorPosX = 0.0f;
        float editorPosY = 0.0f;
    };

    using StateCallback = std::function<void(const State&)>;

    struct StateCallbacks
    {
        StateCallback onEnter;
        StateCallback onUpdate;
        StateCallback onLateUpdate;
        StateCallback onExit;
    };

    AIFlow(Object* owner);
    ~AIFlow() override;

    void OnStart() override;
    void OnUpdate() override;
    void OnLateUpdate() override;
    void OnEnabled() override;
    void OnDisabled() override;
    void OnDrawGUI() override;
    const char* GetDebugName() const override { return ICON_FA_COG " AIFlow"; }

    void AddCallbackFunc(
        const std::string& name,
        StateCallback onEnter = {},
        StateCallback onUpdate = {},
        StateCallback onLateUpdate = {},
        StateCallback onExit = {});
    void BindCallbacks();

    State& AddState(const std::string& name, const std::string& callbackName = "");
    Transition& AddTransition(int sourceStateId, int targetStateId);
    bool RemoveState(int stateId);
    bool RemoveTransition(int transitionId);
    void ClearGraph();
    void SetEntryState(int stateId) { entryStateId = stateId; }

    bool Load(const std::string& path);
    bool Save(const std::string& path) const;
    void SetGraphPath(const std::string& path) { graphPath = path; }
    const std::string& GetGraphPath() const { return graphPath; }

    void SetBool(const std::string& name, bool value, bool runtimeOnly = false);
    void SetFloat(const std::string& name, float value, bool runtimeOnly = false);
    void SetVector3(const std::string& name, const Vector3& value, bool runtimeOnly = false);
    bool GetBool(const std::string& name, bool fallback = false) const;
    float GetFloat(const std::string& name, float fallback = 0.0f) const;
    Vector3 GetVector3(const std::string& name, const Vector3& fallback = Vector3::Zero) const;

    int GetCurrentStateId() const { return currentStateId; }
    const State* GetCurrentState() const;
    const std::vector<State>& GetStates() const { return states; }
    std::vector<State>& GetStates() { return states; }
    const std::vector<Parameter>& GetParameters() const { return parameters; }
    std::vector<Parameter>& GetParameters() { return parameters; }
    State* FindState(int stateId);
    const State* FindState(int stateId) const;
    Transition* FindTransition(int transitionId, State** sourceState = nullptr);

protected:
    virtual void UpdateBlackboard() {}
    virtual void DrawFlowInspector() {}
    virtual bool LoadFlowExtension(const std::string&) { return true; }
    virtual bool SaveFlowExtension(const std::string&) const { return true; }

private:
    struct EditorData;

    void EnsureCurrentState();
    void ChangeState(int stateId);
    void RestartStateMachine();
    void EvaluateTransitions();
    bool EvaluateTransition(const Transition& transition) const;
    bool EvaluateCondition(const Condition& condition) const;
    void Invoke(StateCallback StateCallbacks::* callbackMember);
    Parameter* FindParameter(const std::string& name);
    const Parameter* FindParameter(const std::string& name) const;
    void DrawEditor(bool* open);

    std::vector<State> states;
    std::vector<Parameter> parameters;
    std::unordered_map<std::string, StateCallbacks> callbackRegistry;
    std::unordered_map<int, StateCallbacks> boundCallbacks;
    std::unique_ptr<EditorData> editor;
    std::string graphPath = "Data/AI/AIFlow.json";
    int entryStateId = -1;
    int currentStateId = -1;
    int nextStateId = 1;
    int nextTransitionId = 1;
    bool editorOpen = false;
    bool pendingEnter = true;
};
