#include "UI/UIEventSystem.h"

#include "Application/Input/Input.h"
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

void UIEventSystem::UpdatePointer(UIEventData& data)
{
	Mouse& mouse = Game::Input::Instance().GetMouse();
	if (ImGui::GetIO().WantCaptureMouse || !mouse.IsCursorVisible() || mouse.IsCursorLocked())
	{
		ResetPointer(data);
		return;
	}
	SetHovered(Raycast(data.position), data);
	const MouseButton buttons[] = {Mouse::BTN_LEFT, Mouse::BTN_RIGHT, Mouse::BTN_MIDDLE};
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
			if (index == 0) SetSelected(hit, data);
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
	const Vector2 center = current->rect.position + current->rect.size * (Vector2(0.5f, 0.5f) - current->rect.anchor);
	std::shared_ptr<Widget> next;
	std::shared_ptr<Widget> wrap;
	float bestScore = -std::numeric_limits<float>::max();
	float wrapDistance = 0.0f;
	for (const auto& target : targets)
	{
		const auto& candidate = target.widget;
		if (candidate == current || !IsAvailable(candidate) ||
			candidate->GetComponent<EventTrigger>()->GetNavigation() == UINavigation::None) continue;
		const Vector2 candidateCenter = candidate->rect.position +
			candidate->rect.size * (Vector2(0.5f, 0.5f) - candidate->rect.anchor);
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
	GamePad& pad = Game::Input::Instance().GetGamePad();
	const bool submitHeld = (pad.GetButton() & GamePad::BTN_A) ||
		(GetAsyncKeyState(VK_RETURN) & 0x8000) || (GetAsyncKeyState(VK_SPACE) & 0x8000);
	const bool cancelHeld = (pad.GetButton() & (GamePad::BTN_B | GamePad::BTN_ESCAPE)) != 0;
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
		SetSelected(target.widget, data);
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
	const float x = pad.GetAxisLX();
	const float y = pad.GetAxisLY();
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
			if (widget && widget->GetAffectedByPostProcess() == postProcess) Collect(widget, parents);
	Mouse& mouse = Game::Input::Instance().GetMouse();
	UIEventData data;
	data.position = Vector2(static_cast<float>(mouse.GetPositionX()), static_cast<float>(mouse.GetPositionY()));
	data.delta = hasPosition ? data.position - lastPosition : Vector2::Zero;
	lastPosition = data.position;
	hasPosition = true;
	if (!Game::Input::IsFocusedWindow(true))
	{
		ResetPointer(data);
		submitArmed = false;
		submitWasHeld = cancelWasHeld = true;
		lastMove = Vector2::Zero;
		return;
	}
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
