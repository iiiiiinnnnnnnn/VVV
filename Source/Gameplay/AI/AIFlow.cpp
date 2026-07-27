// AIFlow.cpp

#include "Gameplay/AI/AIFlow.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <unordered_set>

#include "imgui.h"
#include "imgui_node_editor.h"
#include "Core/Foundation/Json.h"

namespace ed = ax::NodeEditor;

namespace
{
    constexpr int NODE_ID_BASE = 1000;
    constexpr int PIN_ID_BASE = 100000;
    constexpr int LINK_ID_BASE = 200000;

    ed::NodeId ToNodeId(int stateId)
    {
        return ed::NodeId(NODE_ID_BASE + stateId);
    }

    ed::PinId ToInputPinId(int stateId)
    {
        return ed::PinId(PIN_ID_BASE + stateId * 2);
    }

    ed::PinId ToOutputPinId(int stateId)
    {
        return ed::PinId(PIN_ID_BASE + stateId * 2 + 1);
    }

    ed::LinkId ToLinkId(int transitionId)
    {
        return ed::LinkId(LINK_ID_BASE + transitionId);
    }

    int DecodeInputPin(ed::PinId pin)
    {
        const int value = static_cast<int>(pin.Get()) - PIN_ID_BASE;
        if (value < 0 || value % 2 != 0) return -1;
        return value / 2;
    }

    int DecodeOutputPin(ed::PinId pin)
    {
        const int value = static_cast<int>(pin.Get()) - PIN_ID_BASE;
        if (value < 0 || value % 2 != 1) return -1;
        return value / 2;
    }

    int DecodeLink(ed::LinkId link)
    {
        return static_cast<int>(link.Get()) - LINK_ID_BASE;
    }

    const char* CompareName(AIFlow::CompareOp compare)
    {
        using CompareOp = AIFlow::CompareOp;
        switch (compare)
        {
            case CompareOp::IsTrue: return "Is True";
            case CompareOp::IsFalse: return "Is False";
            case CompareOp::Less: return "<";
            case CompareOp::LessEqual: return "<=";
            case CompareOp::Greater: return ">";
            case CompareOp::GreaterEqual: return ">=";
            case CompareOp::Equal: return "==";
            case CompareOp::NotEqual: return "!=";
        }
        return "Unknown";
    }
}

struct AIFlow::EditorData
{
    EditorData()
    {
        ed::Config config;
        config.SettingsFile = nullptr;
        context = ed::CreateEditor(&config);
    }

    ~EditorData()
    {
        if (context) ed::DestroyEditor(context);
    }

    ed::EditorContext* context = nullptr;
    std::unordered_set<int> positionedStateIds;
    int selectedStateId = -1;
    int selectedTransitionId = -1;
    int pendingStateId = -1;
    ImVec2 pendingStatePosition = ImVec2(0.0f, 0.0f);
    ImVec2 contextMenuPosition = ImVec2(0.0f, 0.0f);
    int contextStateId = -1;
    int contextTransitionId = -1;
    std::string newParameterName = "Parameter";
    ParameterType newParameterType = ParameterType::Float;
};

AIFlow::AIFlow(Object* owner)
    : Component(owner), editor(std::make_unique<EditorData>())
{
}

AIFlow::~AIFlow() = default;

void AIFlow::OnStart()
{
    if (states.empty() && !graphPath.empty()) Load(graphPath);
    BindCallbacks();
    pendingEnter = true;
}

void AIFlow::OnUpdate()
{
    UpdateBlackboard();
    EnsureCurrentState();
    Invoke(&StateCallbacks::onUpdate);
    EvaluateTransitions();
}

void AIFlow::OnLateUpdate()
{
    EnsureCurrentState();
    Invoke(&StateCallbacks::onLateUpdate);
}

void AIFlow::OnEnabled()
{
    pendingEnter = true;
}

void AIFlow::OnDisabled()
{
    Invoke(&StateCallbacks::onExit);
    currentStateId = -1;
    pendingEnter = true;
}

void AIFlow::OnDrawGUI()
{
    const State* current = GetCurrentState();
    ImGui::Text("Current State: %s", current ? current->name.c_str() : "None");
    ImGui::Text(
        "Callback Bound: %s",
        current && boundCallbacks.contains(current->id) ? "true" : "false");
    DrawFlowInspector();

    ImGui::InputText("Graph Path", &graphPath);
    if (ImGui::Button("Open AI Graph")) editorOpen = true;
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
        Load(graphPath);
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        Save(graphPath);

    DrawEditor(&editorOpen);
}

void AIFlow::AddCallbackFunc(
    const std::string& name,
    StateCallback onEnter,
    StateCallback onUpdate,
    StateCallback onLateUpdate,
    StateCallback onExit)
{
    callbackRegistry[name] = {
        std::move(onEnter),
        std::move(onUpdate),
        std::move(onLateUpdate),
        std::move(onExit)
    };
}

void AIFlow::BindCallbacks()
{
    boundCallbacks.clear();
    for (const State& state : states)
    {
        const auto found = callbackRegistry.find(state.callbackName);
        if (found == callbackRegistry.end()) continue;
        boundCallbacks[state.id] = found->second;
    }
}

AIFlow::State& AIFlow::AddState(
    const std::string& name,
    const std::string& callbackName)
{
    State state;
    state.id = nextStateId++;
    state.name = name;
    state.callbackName = callbackName;
    states.push_back(std::move(state));
    if (entryStateId < 0) entryStateId = states.back().id;
    return states.back();
}

AIFlow::Transition& AIFlow::AddTransition(
    int sourceStateId,
    int targetStateId)
{
    State* source = FindState(sourceStateId);
    if (!source) source = &AddState("State");

    Transition transition;
    transition.id = nextTransitionId++;
    transition.targetStateId = targetStateId;
    source->transitions.push_back(std::move(transition));
    return source->transitions.back();
}

bool AIFlow::RemoveState(int stateId)
{
    const auto found = std::find_if(
        states.begin(), states.end(),
        [stateId](const State& state) { return state.id == stateId; });
    if (found == states.end()) return false;

    states.erase(found);
    for (State& state : states)
    {
        std::erase_if(
            state.transitions,
            [stateId](const Transition& transition)
            {
                return transition.targetStateId == stateId;
            });
    }

    boundCallbacks.erase(stateId);
    if (entryStateId == stateId)
        entryStateId = states.empty() ? -1 : states.front().id;
    if (currentStateId == stateId)
    {
        currentStateId = -1;
        pendingEnter = true;
    }
    return true;
}

bool AIFlow::RemoveTransition(int transitionId)
{
    for (State& state : states)
    {
        const auto found = std::find_if(
            state.transitions.begin(), state.transitions.end(),
            [transitionId](const Transition& transition)
            {
                return transition.id == transitionId;
            });
        if (found == state.transitions.end()) continue;
        state.transitions.erase(found);
        return true;
    }
    return false;
}

void AIFlow::ClearGraph()
{
    states.clear();
    boundCallbacks.clear();
    entryStateId = -1;
    currentStateId = -1;
    nextStateId = 1;
    nextTransitionId = 1;
    pendingEnter = true;
    editor->positionedStateIds.clear();
    editor->selectedStateId = -1;
    editor->selectedTransitionId = -1;
}

bool AIFlow::Load(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream) return false;

    try
    {
        json root;
        stream >> root;

        ClearGraph();
        parameters.clear();
        entryStateId = root.value("entryStateId", -1);

        if (root.contains("parameters"))
        {
            for (const json& value : root["parameters"])
            {
                Parameter parameter;
                parameter.name = value.value("name", "Parameter");
                parameter.type = static_cast<ParameterType>(value.value("type", 1));
                parameter.boolValue = value.value("bool", false);
                parameter.floatValue = value.value("float", 0.0f);
                parameter.runtimeOnly = value.value("runtimeOnly", false);
                if (value.contains("vector"))
                {
                    parameter.vectorValue.x = value["vector"].value("x", 0.0f);
                    parameter.vectorValue.y = value["vector"].value("y", 0.0f);
                    parameter.vectorValue.z = value["vector"].value("z", 0.0f);
                }
                parameters.push_back(std::move(parameter));
            }
        }

        if (root.contains("states"))
        {
            for (const json& stateJson : root["states"])
            {
                State state;
                state.id = stateJson.value("id", nextStateId++);
                nextStateId = std::max(nextStateId, state.id + 1);
                state.name = stateJson.value("name", "State");
                state.callbackName = stateJson.value("callback", "");
                state.hasEditorPosition = stateJson.value("hasEditorPosition", false);
                state.editorPosX = stateJson.value("editorPosX", 0.0f);
                state.editorPosY = stateJson.value("editorPosY", 0.0f);

                if (stateJson.contains("transitions"))
                {
                    for (const json& transitionJson : stateJson["transitions"])
                    {
                        Transition transition;
                        transition.id = transitionJson.value("id", nextTransitionId++);
                        nextTransitionId = std::max(nextTransitionId, transition.id + 1);
                        transition.targetStateId = transitionJson.value("targetStateId", -1);
                        transition.conditionLogic =
                            transitionJson.value("conditionLogic", std::string("AND")) == "OR"
                            ? ConditionLogic::Or
                            : ConditionLogic::And;

                        if (transitionJson.contains("conditions"))
                        {
                            for (const json& conditionJson : transitionJson["conditions"])
                            {
                                Condition condition;
                                condition.parameterName = conditionJson.value("parameter", "");
                                condition.compare = static_cast<CompareOp>(conditionJson.value("compare", 0));
                                condition.threshold = conditionJson.value("threshold", 0.0f);
                                transition.conditions.push_back(std::move(condition));
                            }
                        }
                        state.transitions.push_back(std::move(transition));
                    }
                }
                states.push_back(std::move(state));
            }
        }

        graphPath = path;
        if (!LoadFlowExtension(path)) return false;
        BindCallbacks();
        return true;
    }
    catch (...)
    {
        ClearGraph();
        return false;
    }
}

bool AIFlow::Save(const std::string& path) const
{
    json root;
    root["entryStateId"] = entryStateId;
    root["parameters"] = json::array();
    root["states"] = json::array();

    for (const Parameter& parameter : parameters)
    {
        root["parameters"].push_back({
            {"name", parameter.name},
            {"type", static_cast<int>(parameter.type)},
            {"bool", parameter.boolValue},
            {"float", parameter.floatValue},
            {"runtimeOnly", parameter.runtimeOnly},
            {"vector", {
                {"x", parameter.vectorValue.x},
                {"y", parameter.vectorValue.y},
                {"z", parameter.vectorValue.z}
            }}
        });
    }

    for (const State& state : states)
    {
        json stateJson = {
            {"id", state.id},
            {"name", state.name},
            {"callback", state.callbackName},
            {"hasEditorPosition", state.hasEditorPosition},
            {"editorPosX", state.editorPosX},
            {"editorPosY", state.editorPosY},
            {"transitions", json::array()}
        };

        for (const Transition& transition : state.transitions)
        {
            json transitionJson = {
                {"id", transition.id},
                {"targetStateId", transition.targetStateId},
                {"conditionLogic", transition.conditionLogic == ConditionLogic::Or ? "OR" : "AND"},
                {"conditions", json::array()}
            };
            for (const Condition& condition : transition.conditions)
            {
                transitionJson["conditions"].push_back({
                    {"parameter", condition.parameterName},
                    {"compare", static_cast<int>(condition.compare)},
                    {"threshold", condition.threshold}
                });
            }
            stateJson["transitions"].push_back(std::move(transitionJson));
        }
        root["states"].push_back(std::move(stateJson));
    }

    std::ofstream stream(path);
    if (!stream) return false;
    stream << root.dump(4);
    const bool saved = stream.good();
    stream.close();
    return saved && SaveFlowExtension(path);
}

void AIFlow::SetBool(
    const std::string& name,
    bool value,
    bool runtimeOnly)
{
    Parameter* parameter = FindParameter(name);
    if (!parameter)
    {
        parameters.push_back({name, ParameterType::Bool});
        parameter = &parameters.back();
    }
    parameter->type = ParameterType::Bool;
    parameter->boolValue = value;
    parameter->runtimeOnly = runtimeOnly;
}

void AIFlow::SetFloat(
    const std::string& name,
    float value,
    bool runtimeOnly)
{
    Parameter* parameter = FindParameter(name);
    if (!parameter)
    {
        parameters.push_back({name, ParameterType::Float});
        parameter = &parameters.back();
    }
    parameter->type = ParameterType::Float;
    parameter->floatValue = value;
    parameter->runtimeOnly = runtimeOnly;
}

void AIFlow::SetVector3(
    const std::string& name,
    const Vector3& value,
    bool runtimeOnly)
{
    Parameter* parameter = FindParameter(name);
    if (!parameter)
    {
        parameters.push_back({name, ParameterType::Vector3});
        parameter = &parameters.back();
    }
    parameter->type = ParameterType::Vector3;
    parameter->vectorValue = value;
    parameter->runtimeOnly = runtimeOnly;
}

bool AIFlow::GetBool(const std::string& name, bool fallback) const
{
    const Parameter* parameter = FindParameter(name);
    if (!parameter || parameter->type != ParameterType::Bool) return fallback;
    return parameter->boolValue;
}

float AIFlow::GetFloat(const std::string& name, float fallback) const
{
    const Parameter* parameter = FindParameter(name);
    if (!parameter || parameter->type != ParameterType::Float) return fallback;
    return parameter->floatValue;
}

Vector3 AIFlow::GetVector3(
    const std::string& name,
    const Vector3& fallback) const
{
    const Parameter* parameter = FindParameter(name);
    if (!parameter || parameter->type != ParameterType::Vector3) return fallback;
    return parameter->vectorValue;
}

const AIFlow::State* AIFlow::GetCurrentState() const
{
    return FindState(currentStateId);
}

AIFlow::State* AIFlow::FindState(int stateId)
{
    for (State& state : states)
        if (state.id == stateId) return &state;
    return nullptr;
}

const AIFlow::State* AIFlow::FindState(int stateId) const
{
    for (const State& state : states)
        if (state.id == stateId) return &state;
    return nullptr;
}

AIFlow::Transition* AIFlow::FindTransition(
    int transitionId,
    State** sourceState)
{
    for (State& state : states)
    {
        for (Transition& transition : state.transitions)
        {
            if (transition.id != transitionId) continue;
            if (sourceState) *sourceState = &state;
            return &transition;
        }
    }
    return nullptr;
}

void AIFlow::EnsureCurrentState()
{
    if (!FindState(currentStateId)) currentStateId = entryStateId;
    if (!FindState(currentStateId) && !states.empty()) currentStateId = states.front().id;
    if (!pendingEnter || !FindState(currentStateId)) return;

    pendingEnter = false;
    Invoke(&StateCallbacks::onEnter);
}

void AIFlow::ChangeState(int stateId)
{
    if (currentStateId == stateId || !FindState(stateId)) return;
    Invoke(&StateCallbacks::onExit);
    currentStateId = stateId;
    pendingEnter = false;
    Invoke(&StateCallbacks::onEnter);
}

void AIFlow::RestartStateMachine()
{
    Invoke(&StateCallbacks::onExit);
    currentStateId = entryStateId;
    pendingEnter = true;
    EnsureCurrentState();
}

void AIFlow::EvaluateTransitions()
{
    const State* state = FindState(currentStateId);
    if (!state) return;

    for (const Transition& transition : state->transitions)
    {
        if (!EvaluateTransition(transition)) continue;
        ChangeState(transition.targetStateId);
        return;
    }
}

bool AIFlow::EvaluateTransition(const Transition& transition) const
{
    if (!FindState(transition.targetStateId)) return false;

    if (transition.conditions.empty()) return true;

    if (transition.conditionLogic == ConditionLogic::Or)
    {
        for (const Condition& condition : transition.conditions)
            if (EvaluateCondition(condition)) return true;
        return false;
    }

    for (const Condition& condition : transition.conditions)
        if (!EvaluateCondition(condition)) return false;
    return true;
}

bool AIFlow::EvaluateCondition(const Condition& condition) const
{
    const Parameter* parameter = FindParameter(condition.parameterName);
    if (!parameter) return false;

    if (parameter->type == ParameterType::Bool)
    {
        if (condition.compare == CompareOp::IsTrue) return parameter->boolValue;
        if (condition.compare == CompareOp::IsFalse) return !parameter->boolValue;
        if (condition.compare == CompareOp::Equal)
            return parameter->boolValue == (condition.threshold != 0.0f);
        if (condition.compare == CompareOp::NotEqual)
            return parameter->boolValue != (condition.threshold != 0.0f);
        return false;
    }

    if (parameter->type != ParameterType::Float) return false;
    const float value = parameter->floatValue;
    switch (condition.compare)
    {
    case CompareOp::Less: return value < condition.threshold;
    case CompareOp::LessEqual: return value <= condition.threshold;
    case CompareOp::Greater: return value > condition.threshold;
    case CompareOp::GreaterEqual: return value >= condition.threshold;
    case CompareOp::Equal: return fabsf(value - condition.threshold) <= eps;
    case CompareOp::NotEqual: return fabsf(value - condition.threshold) > eps;
    default: return false;
    }
}

void AIFlow::Invoke(StateCallback StateCallbacks::* callbackMember)
{
    const State* state = FindState(currentStateId);
    if (!state) return;

    const auto found = boundCallbacks.find(state->id);
    if (found == boundCallbacks.end()) return;

    const StateCallback& callback = found->second.*callbackMember;
    if (callback) callback(*state);
}

AIFlow::Parameter* AIFlow::FindParameter(const std::string& name)
{
    for (Parameter& parameter : parameters)
        if (parameter.name == name) return &parameter;
    return nullptr;
}

const AIFlow::Parameter* AIFlow::FindParameter(
    const std::string& name) const
{
    for (const Parameter& parameter : parameters)
        if (parameter.name == name) return &parameter;
    return nullptr;
}

void AIFlow::DrawEditor(bool* open)
{
    if (!open || !*open) return;

    bool liveTransitionChanged = false;
    int changedTransitionId = -1;

    ImGui::SetNextWindowSize(ImVec2(1150.0f, 720.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("AI Flow", open))
    {
        ImGui::End();
        return;
    }

    const float inspectorWidth = 330.0f;
    const float panelHeight = ImGui::GetContentRegionAvail().y;
    constexpr ImGuiTableFlags layoutFlags =
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_NoSavedSettings |
        ImGuiTableFlags_BordersInnerV;
    if (!ImGui::BeginTable(
        "##AIEditorLayout",
        2,
        layoutFlags,
        ImVec2(0.0f, panelHeight)))
    {
        ImGui::End();
        return;
    }
    ImGui::TableSetupColumn(
        "Inspector",
        ImGuiTableColumnFlags_WidthFixed,
        inspectorWidth);
    ImGui::TableSetupColumn(
        "Graph",
        ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(1);

    ImGui::BeginChild("##AIGraphCanvas", ImVec2(0.0f, panelHeight), true);
    ed::SetCurrentEditor(editor->context);
    ed::Begin("AI State Graph");

    const ImVec2 canvasMousePosition = ed::ScreenToCanvas(ImGui::GetMousePos());
    if (editor->pendingStateId >= 0)
    {
        if (State* pendingState = FindState(editor->pendingStateId))
        {
            const ed::NodeId nodeId = ToNodeId(pendingState->id);
            ed::SetNodePosition(nodeId, editor->pendingStatePosition);
            pendingState->hasEditorPosition = true;
            pendingState->editorPosX = editor->pendingStatePosition.x;
            pendingState->editorPosY = editor->pendingStatePosition.y;
            editor->positionedStateIds.insert(pendingState->id);
            editor->selectedStateId = pendingState->id;
            editor->selectedTransitionId = -1;
        }
        editor->pendingStateId = -1;
    }

    for (State& state : states)
    {
        const ed::NodeId nodeId = ToNodeId(state.id);
        if (!editor->positionedStateIds.contains(state.id))
        {
            const ImVec2 position = state.hasEditorPosition
                ? ImVec2(state.editorPosX, state.editorPosY)
                : ImVec2(80.0f + state.id * 180.0f, 100.0f + (state.id % 3) * 140.0f);
            ed::SetNodePosition(nodeId, position);
            editor->positionedStateIds.insert(state.id);
        }

        const bool isCurrent = state.id == currentStateId;
        if (isCurrent)
        {
            ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.06f, 0.28f, 0.10f, 0.96f));
            ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.2f, 0.85f, 0.3f, 1.0f));
            ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 5.0f);
        }

        ed::BeginNode(nodeId);
        ImGui::PushID(state.id);
        ImGui::TextUnformatted(state.name.c_str());
        if (isCurrent)
            ImGui::TextColored(ImVec4(0.25f, 1.0f, 0.35f, 1.0f), ">> RUNNING <<");
        ImGui::TextDisabled("Bind: %s", state.callbackName.empty() ? "None" : state.callbackName.c_str());
        if (state.id == entryStateId) ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Entry");

        ed::BeginPin(ToInputPinId(state.id), ed::PinKind::Input);
        ImGui::TextUnformatted(">");
        ed::EndPin();
        ImGui::SameLine(130.0f);
        ed::BeginPin(ToOutputPinId(state.id), ed::PinKind::Output);
        ImGui::TextUnformatted("o");
        ed::EndPin();
        ImGui::PopID();
        ed::EndNode();

        if (isCurrent)
        {
            ed::PopStyleVar();
            ed::PopStyleColor(2);
        }
        if (ed::IsNodeSelected(nodeId) && !ImGui::IsMouseDragging(0))
        {
            editor->selectedStateId = state.id;
            editor->selectedTransitionId = -1;
        }

        const ImVec2 position = ed::GetNodePosition(nodeId);
        state.hasEditorPosition = true;
        state.editorPosX = position.x;
        state.editorPosY = position.y;
    }

    for (const State& state : states)
    {
        for (const Transition& transition : state.transitions)
        {
            if (!FindState(transition.targetStateId)) continue;
            const ed::LinkId linkId = ToLinkId(transition.id);
            const bool selected = transition.id == editor->selectedTransitionId;
            ed::Link(
                linkId,
                ToOutputPinId(state.id),
                ToInputPinId(transition.targetStateId),
                selected ? ImVec4(1.0f, 0.8f, 0.1f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                2.0f);
            if (ed::IsLinkSelected(linkId))
            {
                editor->selectedTransitionId = transition.id;
                editor->selectedStateId = -1;
            }
        }
    }

    if (ed::BeginCreate())
    {
        ed::PinId startPin;
        ed::PinId endPin;
        if (ed::QueryNewLink(&startPin, &endPin) && startPin && endPin)
        {
            int sourceId = DecodeOutputPin(startPin);
            int targetId = DecodeInputPin(endPin);
            if (sourceId < 0 || targetId < 0)
            {
                sourceId = DecodeOutputPin(endPin);
                targetId = DecodeInputPin(startPin);
            }

            bool exists = false;
            if (const State* source = FindState(sourceId))
            {
                for (const Transition& transition : source->transitions)
                    if (transition.targetStateId == targetId) exists = true;
            }

            if (sourceId >= 0 && targetId >= 0 && sourceId != targetId && !exists)
            {
                if (ed::AcceptNewItem(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), 2.0f))
                {
                    Transition& transition = AddTransition(sourceId, targetId);
                    editor->selectedTransitionId = transition.id;
                    editor->selectedStateId = -1;
                }
            }
            else
            {
                ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
            }
        }
    }
    ed::EndCreate();

    if (ed::BeginDelete())
    {
        ed::LinkId linkId;
        while (ed::QueryDeletedLink(&linkId))
        {
            if (!ed::AcceptDeletedItem()) continue;
            const int transitionId = DecodeLink(linkId);
            RemoveTransition(transitionId);
            if (editor->selectedTransitionId == transitionId)
                editor->selectedTransitionId = -1;
        }
    }
    ed::EndDelete();

    bool openBackgroundMenu = false;
    bool openNodeMenu = false;
    bool openLinkMenu = false;
    ed::NodeId contextNodeId;
    ed::LinkId contextLinkId;

    if (ed::ShowBackgroundContextMenu())
    {
        openBackgroundMenu = true;
        editor->contextMenuPosition = canvasMousePosition;
    }
    if (ed::ShowNodeContextMenu(&contextNodeId))
    {
        openNodeMenu = true;
        editor->contextStateId = static_cast<int>(contextNodeId.Get()) - NODE_ID_BASE;
        editor->selectedStateId = editor->contextStateId;
        editor->selectedTransitionId = -1;
    }
    if (ed::ShowLinkContextMenu(&contextLinkId))
    {
        openLinkMenu = true;
        editor->contextTransitionId = DecodeLink(contextLinkId);
        editor->selectedTransitionId = editor->contextTransitionId;
        editor->selectedStateId = -1;
    }

    ed::Suspend();
    if (openBackgroundMenu) ImGui::OpenPopup("AIBackgroundContextMenu");
    if (openNodeMenu) ImGui::OpenPopup("AIStateContextMenu");
    if (openLinkMenu) ImGui::OpenPopup("AITransitionContextMenu");

    if (ImGui::BeginPopup("AIBackgroundContextMenu"))
    {
        if (ImGui::MenuItem("+ State"))
        {
            State& state = AddState("New State");
            editor->pendingStateId = state.id;
            editor->pendingStatePosition = editor->contextMenuPosition;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save Graph"))
            Save(graphPath);
        if (ImGui::MenuItem("Reload Graph"))
            Load(graphPath);
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AIStateContextMenu"))
    {
        const int stateId = editor->contextStateId;
        if (State* state = FindState(stateId))
        {
            ImGui::TextUnformatted(state->name.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Set As Entry")) entryStateId = stateId;
            if (ImGui::MenuItem("Delete State"))
            {
                editor->positionedStateIds.erase(stateId);
                editor->selectedStateId = -1;
                RemoveState(stateId);
            }
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopup("AITransitionContextMenu"))
    {
        const int transitionId = editor->contextTransitionId;
        if (Transition* transition = FindTransition(transitionId))
        {
            const State* targetState = FindState(transition->targetStateId);
            ImGui::Text("To: %s", targetState ? targetState->name.c_str() : "Missing");
            ImGui::Separator();
            if (ImGui::MenuItem("Edit Conditions"))
            {
                editor->selectedTransitionId = transitionId;
                editor->selectedStateId = -1;
            }
            if (ImGui::MenuItem("Delete Transition"))
            {
                editor->selectedTransitionId = -1;
                RemoveTransition(transitionId);
            }
        }
        ImGui::EndPopup();
    }
    ed::Resume();

    if (ed::IsBackgroundClicked())
    {
        editor->selectedStateId = -1;
        editor->selectedTransitionId = -1;
    }

    ed::LinkId clickedLink = ed::GetDoubleClickedLink();
    if (clickedLink ||
        (ImGui::IsMouseClicked(0) &&
         ed::GetHoveredLink().Get() != 0 &&
         (clickedLink = ed::GetHoveredLink(), true)))
    {
        const int transitionId = DecodeLink(clickedLink);
        if (FindTransition(transitionId))
        {
            editor->selectedTransitionId = transitionId;
            editor->selectedStateId = -1;
            ed::SelectLink(clickedLink);
        }
    }

    ed::End();
    ed::SetCurrentEditor(nullptr);
    ImGui::EndChild();

    ImGui::TableSetColumnIndex(0);
    ImGui::BeginChild("##AIGraphInspector", ImVec2(0.0f, panelHeight), true);
    ImGui::TextUnformatted("Graph");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##AIGraphPath", &graphPath);
    if (ImGui::Button("Load"))
        Load(graphPath) ? "Loaded." : "Load failed.";
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        Save(graphPath) ? "Saved." : "Save failed.";
    ImGui::SameLine();
    if (ImGui::Button("Bind"))
    {
        BindCallbacks();
    }
    if (ImGui::Button("Restart From Entry", ImVec2(-1.0f, 0.0f)))
    {
        RestartStateMachine();
    }

    const State* playingState = GetCurrentState();
    ImGui::TextColored(
        ImVec4(0.2f, 1.0f, 0.35f, 1.0f),
        "Playing: %s",
        playingState ? playingState->name.c_str() : "None");

    ImGui::Spacing();
    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    if (State* state = FindState(editor->selectedStateId))
    {
        ImGui::Text("State ID: %d", state->id);
        ImGui::InputText("Name", &state->name);
        if (ImGui::InputText("Callback", &state->callbackName)) BindCallbacks();
        if (ImGui::Button("Set As Entry")) entryStateId = state->id;
        ImGui::SameLine();
        if (ImGui::Button("Delete State"))
        {
            const int stateId = state->id;
            editor->selectedStateId = -1;
            editor->positionedStateIds.erase(stateId);
            RemoveState(stateId);
        }
    }
    else if (Transition* transition = FindTransition(editor->selectedTransitionId))
    {
        State* sourceState = nullptr;
        FindTransition(transition->id, &sourceState);
        const State* targetState = FindState(transition->targetStateId);
        ImGui::Text(
            "%s -> %s",
            sourceState ? sourceState->name.c_str() : "Missing",
            targetState ? targetState->name.c_str() : "Missing");
        const bool sourceIsCurrent = sourceState && sourceState->id == currentStateId;
        const bool transitionPasses = EvaluateTransition(*transition);
        ImGui::TextColored(
            sourceIsCurrent
                ? (transitionPasses
                    ? ImVec4(0.2f, 1.0f, 0.35f, 1.0f)
                    : ImVec4(1.0f, 0.45f, 0.2f, 1.0f))
                : ImVec4(0.65f, 0.65f, 0.65f, 1.0f),
            sourceIsCurrent
                ? (transitionPasses ? "LIVE: PASS" : "LIVE: BLOCKED")
                : "LIVE: WAITING (source state is not running)");

        int conditionLogic = static_cast<int>(transition->conditionLogic);
        const char* conditionLogicNames[] = {"AND (All)", "OR (Any)"};
        if (ImGui::Combo(
            "Condition Logic",
            &conditionLogic,
            conditionLogicNames,
            IM_ARRAYSIZE(conditionLogicNames)))
        {
            transition->conditionLogic = static_cast<ConditionLogic>(conditionLogic);
            liveTransitionChanged = true;
            changedTransitionId = transition->id;
        }

        ImGui::TextDisabled("No conditions means Always.");

        for (int index = 0; index < static_cast<int>(transition->conditions.size()); ++index)
        {
            Condition& condition = transition->conditions[index];
            ImGui::PushID(index);

            const char* parameterPreview = condition.parameterName.empty()
                ? "Select Parameter"
                : condition.parameterName.c_str();
            if (ImGui::BeginCombo("Parameter", parameterPreview))
            {
                for (const Parameter& parameter : parameters)
                {
                    if (parameter.type == ParameterType::Vector3) continue;
                    if (ImGui::Selectable(parameter.name.c_str(), condition.parameterName == parameter.name))
                    {
                        condition.parameterName = parameter.name;
                        condition.compare = parameter.type == ParameterType::Bool
                            ? CompareOp::IsTrue
                            : CompareOp::Greater;
                        liveTransitionChanged = true;
                        changedTransitionId = transition->id;
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::BeginCombo("Compare", CompareName(condition.compare)))
            {
                for (int value = 0; value <= static_cast<int>(CompareOp::NotEqual); ++value)
                {
                    const CompareOp compare = static_cast<CompareOp>(value);
                    if (ImGui::Selectable(CompareName(compare), compare == condition.compare))
                    {
                        condition.compare = compare;
                        liveTransitionChanged = true;
                        changedTransitionId = transition->id;
                    }
                }
                ImGui::EndCombo();
            }

            if (condition.compare != CompareOp::IsTrue && condition.compare != CompareOp::IsFalse)
            {
                if (ImGui::DragFloat("Threshold", &condition.threshold, 0.1f))
                {
                    liveTransitionChanged = true;
                    changedTransitionId = transition->id;
                }
            }

            const bool conditionPasses = EvaluateCondition(condition);
            ImGui::TextColored(
                conditionPasses
                    ? ImVec4(0.2f, 1.0f, 0.35f, 1.0f)
                    : ImVec4(1.0f, 0.45f, 0.2f, 1.0f),
                conditionPasses ? "PASS" : "BLOCKED");

            bool remove = ImGui::Button("Remove Condition");
            ImGui::Separator();
            ImGui::PopID();
            if (remove)
            {
                transition->conditions.erase(transition->conditions.begin() + index);
                liveTransitionChanged = true;
                changedTransitionId = transition->id;
                --index;
            }
        }

        if (ImGui::Button("Add Condition"))
        {
            Condition condition;
            for (const Parameter& parameter : parameters)
            {
                if (parameter.type == ParameterType::Vector3) continue;
                condition.parameterName = parameter.name;
                condition.compare = parameter.type == ParameterType::Bool
                    ? CompareOp::IsTrue
                    : CompareOp::Greater;
                break;
            }
            transition->conditions.push_back(std::move(condition));
            liveTransitionChanged = true;
            changedTransitionId = transition->id;
        }
        if (ImGui::Button("Delete Transition"))
        {
            const int transitionId = transition->id;
            editor->selectedTransitionId = -1;
            RemoveTransition(transitionId);
        }
    }
    else
    {
        ImGui::TextDisabled("Click a transition link to edit its conditions.");
        ImGui::TextDisabled("Right-click a link and choose Edit Conditions, too.");
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Blackboard");
    for (Parameter& parameter : parameters)
    {
        ImGui::PushID(&parameter);
        if (parameter.runtimeOnly) ImGui::BeginDisabled();
        switch (parameter.type)
        {
        case ParameterType::Bool:
            ImGui::Checkbox(parameter.name.c_str(), &parameter.boolValue);
            break;
        case ParameterType::Float:
            ImGui::DragFloat(parameter.name.c_str(), &parameter.floatValue, 0.1f);
            break;
        case ParameterType::Vector3:
            ImGui::DragFloat3(parameter.name.c_str(), &parameter.vectorValue.x, 0.1f);
            break;
        }
        if (parameter.runtimeOnly) ImGui::EndDisabled();
        ImGui::PopID();
    }

    ImGui::InputText("New Name", &editor->newParameterName);
    const char* typeNames[] = {"Bool", "Float", "Vector3"};
    int typeIndex = static_cast<int>(editor->newParameterType);
    if (ImGui::Combo("New Type", &typeIndex, typeNames, 3))
        editor->newParameterType = static_cast<ParameterType>(typeIndex);
    if (ImGui::Button("Add Parameter") && !editor->newParameterName.empty())
    {
        switch (editor->newParameterType)
        {
        case ParameterType::Bool: SetBool(editor->newParameterName, false); break;
        case ParameterType::Float: SetFloat(editor->newParameterName, 0.0f); break;
        case ParameterType::Vector3: SetVector3(editor->newParameterName, Vector3::Zero); break;
        }
    }

    if (liveTransitionChanged)
    {
        State* sourceState = nullptr;
        if (Transition* changedTransition = FindTransition(changedTransitionId, &sourceState))
        {
            if (sourceState && sourceState->id == currentStateId)
                EvaluateTransitions();
        }
    }

    ImGui::EndChild();
    ImGui::EndTable();
    ImGui::End();
}
