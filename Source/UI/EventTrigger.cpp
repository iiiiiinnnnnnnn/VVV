#include "UI/EventTrigger.h"

#include "Application/Time/GameTime.h"

void EventTrigger::AddListener(EventTriggerType type, Callback callback)
{
	const size_t index = static_cast<size_t>(type);
	if (index >= listeners.size() || !callback) return;
	listeners[index].push_back(std::move(callback));
}

// ボタン相当の起動操作。右・中クリックは起動せず、パッド決定はSubmitとして扱う。
void EventTrigger::AddActivationListener(std::function<void()> action)
{
	if (!action) return;
	AddListener(EventTriggerType::PointerClick, [action](UIEventData& data)
	{
		if (data.button == UIPointerButton::Left) action();
	});
	AddListener(EventTriggerType::Submit, [action](UIEventData&) { action(); });
}

void EventTrigger::RemoveListeners(EventTriggerType type)
{
	const size_t index = static_cast<size_t>(type);
	if (index < listeners.size()) listeners[index].clear();
}

bool EventTrigger::HasListener(EventTriggerType type) const
{
	const size_t index = static_cast<size_t>(type);
	return index < listeners.size() && !listeners[index].empty();
}

void EventTrigger::Execute(EventTriggerType type, UIEventData& data)
{
	const size_t index = static_cast<size_t>(type);
	if (!IsActive() || index >= listeners.size()) return;
	// コールバック内で登録内容を変更してもイテレーターを壊さない。
	const auto callbacks = listeners[index];
	for (const auto& callback : callbacks)
	{
		if (!IsActive()) break;
		callback(data);
	}
}

void EventTrigger::OnUpdate()
{
	inputDelay.Update(Game::Time::unscaledDeltaTime);
}

void EventTrigger::OnDisabled()
{
	selected = pointerInside = selectionRequested = false;
	pressedCount = 0;
}

void EventTrigger::OnDrawGUI()
{
	ImGui::Checkbox("Interactable", &interactable);
	const char* modes[] = {"None", "Horizontal", "Vertical", "Automatic"};
	int mode = static_cast<int>(navigation);
	if (ImGui::Combo("Navigation", &mode, modes, 4)) navigation = static_cast<UINavigation>(mode);
	ImGui::Checkbox("Wrap Navigation", &wrapNavigation);
	ImGui::Text("Hovered: %s / Selected: %s / Pressed: %s",
		pointerInside ? "Yes" : "No", selected ? "Yes" : "No", IsPressed() ? "Yes" : "No");
	static const char* names[] = {
		"PointerEnter", "PointerExit", "PointerDown", "PointerUp", "PointerClick",
		"Drag", "Drop", "Scroll", "UpdateSelected", "Select", "Deselect", "Move",
		"InitializePotentialDrag", "BeginDrag", "EndDrag", "Submit", "Cancel"
	};
	for (size_t index = 0; index < listeners.size(); ++index)
		if (!listeners[index].empty()) ImGui::Text("%s: %zu", names[index], listeners[index].size());
}
