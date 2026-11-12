#include "UI/UIEventSystem.h"

#include <InputKit/InputKit.h>
#include "Application/Time/GameTime.h"
#include "UI/WidgetGroup.h"

#include <algorithm>
#include <cmath>
#include <limits>

void UIEventSystem::Collect(const std::shared_ptr<Widget>& widget,
	std::vector<std::shared_ptr<Widget>>& parents)
{
	if (!widget || !widget->IsActive() || widget->IsPendingDestroy()) return;
	if (widget->GetComponent<EventTrigger>()) targets.push_back({widget, parents});
	if (auto* group = dynamic_cast<WidgetGroup*>(widget.get()))
	{
		parents.push_back(widget);
		for (const auto& child : group->GetChildren()) Collect(child, parents);
		parents.pop_back();
	}
}

bool UIEventSystem::IsAvailable(const std::shared_ptr<Widget>& widget) const
{
	if (!widget || !widget->IsActive() || widget->IsPendingDestroy()) return false;
	auto* trigger = widget->GetComponent<EventTrigger>();
	if (!trigger || !trigger->IsInteractable()) return false;
	for (const auto& target : targets)
	{
		if (target.widget != widget) continue;
		for (const auto& parent : target.parents)
			if (!parent->IsActive() || parent->IsPendingDestroy()) return false;
		return true;
	}
	return false;
}

std::shared_ptr<Widget> UIEventSystem::Raycast(const Vector2& position) const
{
	for (auto it = targets.rbegin(); it != targets.rend(); ++it)
		if (IsAvailable(it->widget) && it->widget->ContainsPoint(position)) return it->widget;
	return {};
}

void UIEventSystem::Dispatch(const std::shared_ptr<Widget>& widget,
	EventTriggerType type, UIEventData& data)
{
	if (!IsAvailable(widget)) return;
	data.target = widget.get();
	data.used = false;
	widget->GetComponent<EventTrigger>()->Execute(type, data);
}

void UIEventSystem::SetHovered(const std::shared_ptr<Widget>& widget, UIEventData& data)
{
	auto previous = hovered.lock();
	if (previous == widget) return;
	hovered.reset();
	if (previous)
	{
		previous->GetComponent<EventTrigger>()->pointerInside = false;
		data.relatedTarget = widget.get();
		Dispatch(previous, EventTriggerType::PointerExit, data);
	}
	if (!IsAvailable(widget)) return;
	hovered = widget;
	widget->GetComponent<EventTrigger>()->pointerInside = true;
	data.relatedTarget = previous.get();
	Dispatch(widget, EventTriggerType::PointerEnter, data);
	data.relatedTarget = nullptr;
}

void UIEventSystem::SetSelected(const std::shared_ptr<Widget>& widget, UIEventData& data)
{
	auto previous = selected.lock();
	if (previous == widget) return;
	selected.reset();
	if (previous)
	{
		previous->GetComponent<EventTrigger>()->selected = false;
		data.relatedTarget = widget.get();
		Dispatch(previous, EventTriggerType::Deselect, data);
	}
	if (!IsAvailable(widget)) return;
	selected = widget;
	widget->GetComponent<EventTrigger>()->selected = true;
	data.relatedTarget = previous.get();
	Dispatch(widget, EventTriggerType::Select, data);
	data.relatedTarget = nullptr;
}

void UIEventSystem::ResetPointer(UIEventData& data)
{
	SetHovered({}, data);
	for (size_t index = 0; index < pointers.size(); ++index)
	{
		auto& pointer = pointers[index];
		if (auto widget = pointer.pressed.lock())
		{
			auto* trigger = widget->GetComponent<EventTrigger>();
			trigger->pressedCount = std::max(trigger->pressedCount - 1, 0);
			data.button = static_cast<UIPointerButton>(index);
			data.pressPosition = pointer.pressPosition;
			data.dragging = pointer.dragging;
			Dispatch(widget, EventTriggerType::PointerUp, data);
			if (pointer.dragging) Dispatch(widget, EventTriggerType::EndDrag, data);
		}
		pointer = {};
	}
}

void UIEventSystem::UpdateInputMode(UIEventData& data)
{
	auto& input = InputKit::InputSystem::Instance();
	auto& mouse = input.GetMouse();
	const auto navigation = input.ReadValue<InputKit::Vector2>("UIInput/Navigate");
	const Vector2 navigationInput(
		std::abs(navigation.x) > 0.5f ? (navigation.x > 0 ? 1.0f : -1.0f) : 0.0f,
		std::abs(navigation.y) > 0.5f ? (navigation.y > 0 ? 1.0f : -1.0f) : 0.0f);
	const bool mouseUsed = !ImGui::GetIO().WantCaptureMouse &&
		mouse.IsCursorVisible() && !mouse.IsCursorLocked() &&
		(data.delta.LengthSquared() > 0.0f || mouse.GetButtonDown() || mouse.GetButtonUp() || mouse.GetWheel());
	const bool navigationUsed = !ImGui::GetIO().WantCaptureKeyboard && !ImGui::GetIO().WantTextInput &&
		((navigationInput != Vector2::Zero && navigationInput != lastNavigationInput) ||
		input.GetGamePad().GetButtonDown() || input.IsDown("UIInput/Submit") || input.IsDown("UIInput/Cancel"));
	lastNavigationInput = navigationInput;

	if (mouseUsed && !pointerMode)
	{
		navigationSelection = selected;
		pointerMode = true;
		SetSelected({}, data);
		lastMove = Vector2::Zero;
	}
	else if (!mouseUsed && navigationUsed && pointerMode)
	{
		pointerMode = false;
		ResetPointer(data);
		SetSelected(navigationSelection.lock(), data);
		lastMove = Vector2::Zero;
		moveTimer.Stop();
	}
}

void UIEventSystem::UpdatePointer(UIEventData& data)
{
	InputKit::Mouse& mouse = InputKit::InputSystem::Instance().GetMouse();
	if (!pointerMode || ImGui::GetIO().WantCaptureMouse || !mouse.IsCursorVisible() || mouse.IsCursorLocked())
	{
		ResetPointer(data);
		return;
	}
	SetHovered(Raycast(data.position), data);
	const InputKit::MouseButton buttons[] = {InputKit::Mouse::BTN_LEFT, InputKit::Mouse::BTN_RIGHT, InputKit::Mouse::BTN_MIDDLE};
	for (size_t index = 0; index < pointers.size(); ++index)
	{
		auto& pointer = pointers[index];
		data.relatedTarget = nullptr;
		data.button = static_cast<UIPointerButton>(index);
		data.dragging = pointer.dragging;
		data.pressPosition = pointer.pressPosition;
		auto pressed = pointer.pressed.lock();
		if (pressed && !IsAvailable(pressed))
		{
			auto* trigger = pressed->GetComponent<EventTrigger>();
			trigger->pressedCount = std::max(trigger->pressedCount - 1, 0);
			pointer = {};
			pressed.reset();
		}
		if (mouse.GetButtonDown() & buttons[index])
		{
			auto hit = Raycast(data.position);
			if (!IsAvailable(hit)) continue;
			pointer.pressed = hit;
			pointer.pressPosition = data.position;
			pointer.dragging = false;
			data.pressPosition = data.position;
			data.dragging = false;
			hit->GetComponent<EventTrigger>()->pressedCount++;
			Dispatch(hit, EventTriggerType::PointerDown, data);
			Dispatch(hit, EventTriggerType::InitializePotentialDrag, data);
			pressed = hit;
		}
		if (!pressed) continue;
		data.pressPosition = pointer.pressPosition;
		if ((mouse.GetButton() & buttons[index]) && IsAvailable(pressed))
		{
			const Vector2 distance = data.position - pointer.pressPosition;
			if (!pointer.dragging && distance.LengthSquared() >= dragThreshold * dragThreshold &&
				data.delta.LengthSquared() > 0.0f)
			{
				pointer.dragging = true;
				data.dragging = true;
				Dispatch(pressed, EventTriggerType::BeginDrag, data);
			}
			if (pointer.dragging && data.delta.LengthSquared() > 0.0f)
				Dispatch(pressed, EventTriggerType::Drag, data);
		}
		if (mouse.GetButtonUp() & buttons[index])
		{
			data.dragging = pointer.dragging;
			auto* trigger = pressed->GetComponent<EventTrigger>();
			trigger->pressedCount = std::max(trigger->pressedCount - 1, 0);
			Dispatch(pressed, EventTriggerType::PointerUp, data);
			auto hit = Raycast(data.position);
			if (pointer.dragging)
			{
				data.relatedTarget = pressed.get();
				Dispatch(hit, EventTriggerType::Drop, data);
				data.relatedTarget = hit.get();
				Dispatch(pressed, EventTriggerType::EndDrag, data);
			}
			else if (pressed == hit)
				Dispatch(pressed, EventTriggerType::PointerClick, data);
			pointer = {};
		}
	}
	if (mouse.GetWheel())
	{
		data.scrollDelta = Vector2(0, static_cast<float>(mouse.GetWheel()) / WHEEL_DELTA);
		Dispatch(Raycast(data.position), EventTriggerType::Scroll, data);
	}
}

std::shared_ptr<Widget> UIEventSystem::FindNext(const std::shared_ptr<Widget>& current,
	const Vector2& direction) const
{
	const auto* trigger = current->GetComponent<EventTrigger>();
	const UINavigation mode = trigger->GetNavigation();
	if (mode == UINavigation::None ||
		(mode == UINavigation::Vertical && direction.y == 0) ||
		(mode == UINavigation::Horizontal && direction.x == 0)) return current;
	const Vector2 center = current->GetWorldCenter();
	std::shared_ptr<Widget> next;
	std::shared_ptr<Widget> wrap;
	float bestScore = -std::numeric_limits<float>::max();
	float wrapDistance = 0.0f;
	for (const auto& target : targets)
	{
		const auto& candidate = target.widget;
		if (candidate == current || !IsAvailable(candidate) ||
			candidate->GetComponent<EventTrigger>()->GetNavigation() == UINavigation::None) continue;
		const Vector2 candidateCenter = candidate->GetWorldCenter();
		const Vector2 delta = candidateCenter - center;
		const float distanceSquared = delta.LengthSquared();
		const float forward = delta.Dot(direction);
		if (forward > 0.001f && distanceSquared > 0.0f)
		{
			const float score = forward / distanceSquared;
			if (score > bestScore)
			{
				bestScore = score;
				next = candidate;
			}
		}
		else if (forward < wrapDistance)
		{
			wrapDistance = forward;
			wrap = candidate;
		}
	}
	if (!next && trigger->wrapNavigation) next = wrap;
	return next ? next : current;
}

void UIEventSystem::UpdateNavigation(UIEventData& data)
{
	const auto& input = InputKit::InputSystem::Instance();
	const bool submitHeld = input.IsPressed("UIInput/Submit");
	const bool cancelHeld = input.IsPressed("UIInput/Cancel");
	if (ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput)
	{
		submitWasHeld = submitHeld;
		cancelWasHeld = cancelHeld;
		submitArmed = false;
		lastMove = Vector2::Zero;
		return;
	}
	auto current = selected.lock();
	if (!IsAvailable(current)) SetSelected({}, data);
	for (const auto& target : targets)
	{
		auto* trigger = target.widget->GetComponent<EventTrigger>();
		if (!trigger->selectionRequested || !IsAvailable(target.widget)) continue;
		trigger->selectionRequested = false;
		navigationSelection = target.widget;
		if (!pointerMode) SetSelected(target.widget, data);
	}
	// マウス操作中はホバーだけを使い、自動選択や決定入力を処理しない。
	if (pointerMode)
	{
		SetSelected({}, data);
		submitWasHeld = submitHeld;
		cancelWasHeld = cancelHeld;
		submitArmed = !submitHeld;
		lastMove = Vector2::Zero;
		return;
	}
	current = selected.lock();
	if (!current)
	{
		for (const auto& target : targets)
		{
			if (!IsAvailable(target.widget) ||
				target.widget->GetComponent<EventTrigger>()->GetNavigation() == UINavigation::None) continue;
			SetSelected(target.widget, data);
			break;
		}
	}
	current = selected.lock();
	if (!submitHeld) submitArmed = true;
	Vector2 direction = Vector2::Zero;
	const auto navigation = input.ReadValue<InputKit::Vector2>("UIInput/Navigate");
	const float x = navigation.x;
	const float y = navigation.y;
	if (std::abs(x) > 0.5f || std::abs(y) > 0.5f)
	{
		if (std::abs(x) > std::abs(y)) direction.x = x > 0 ? 1.0f : -1.0f;
		else direction.y = y > 0 ? -1.0f : 1.0f;
	}
	moveTimer.Update(std::max(Game::Time::unscaledDeltaTime, 0.0f));
	if (current && direction != Vector2::Zero && (direction != lastMove || moveTimer.GetRemaining() <= 0.0f))
	{
		data.moveVector = direction;
		data.moveDirection = direction.x < 0 ? UIMoveDirection::Left :
			direction.x > 0 ? UIMoveDirection::Right :
			direction.y < 0 ? UIMoveDirection::Up : UIMoveDirection::Down;
		Dispatch(current, EventTriggerType::Move, data);
		if (!data.used && IsAvailable(current)) SetSelected(FindNext(current, direction), data);
		moveTimer.Reset(direction == lastMove ? 0.1f : 0.45f);
	}
	lastMove = direction;
	current = selected.lock();
	Dispatch(current, EventTriggerType::UpdateSelected, data);
	if (submitArmed && submitHeld && !submitWasHeld)
		Dispatch(current, EventTriggerType::Submit, data);
	if (cancelHeld && !cancelWasHeld) Dispatch(current, EventTriggerType::Cancel, data);
	submitWasHeld = submitHeld;
	cancelWasHeld = cancelHeld;
}

void UIEventSystem::Update(const std::vector<std::shared_ptr<Widget>>& roots)
{
	targets.clear();
	std::vector<std::shared_ptr<Widget>> parents;
	// ポストプロセス適用後に描画する通常UIを、手前の入力対象として扱う。
	for (bool postProcess : {true, false})
		for (const auto& widget : roots)
			if (widget && !widget->GetParent() && widget->GetAffectedByPostProcess() == postProcess) Collect(widget, parents);
	InputKit::Mouse& mouse = InputKit::InputSystem::Instance().GetMouse();
	UIEventData data;
	data.position = Vector2(static_cast<float>(mouse.GetPositionX()), static_cast<float>(mouse.GetPositionY()));
	data.delta = hasPosition ? data.position - lastPosition : Vector2::Zero;
	lastPosition = data.position;
	hasPosition = true;
	if (!InputKit::InputSystem::IsFocusedWindow(true))
	{
		ResetPointer(data);
		submitArmed = false;
		submitWasHeld = cancelWasHeld = true;
		lastMove = Vector2::Zero;
		return;
	}
	UpdateInputMode(data);
	UpdatePointer(data);
	data.relatedTarget = nullptr;
	data.dragging = false;
	UpdateNavigation(data);
	// コールバックで非表示にされた対象の状態も同じフレームで解消する。
	for (auto& pointer : pointers)
	{
		auto pressed = pointer.pressed.lock();
		if (!pressed || IsAvailable(pressed)) continue;
		auto* trigger = pressed->GetComponent<EventTrigger>();
		trigger->pressedCount = std::max(trigger->pressedCount - 1, 0);
		pointer = {};
	}
	if (!IsAvailable(hovered.lock())) SetHovered({}, data);
	if (!IsAvailable(selected.lock())) SetSelected({}, data);
}
