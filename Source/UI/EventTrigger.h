#pragma once

#include "Application/Time/Timer.h"

#include "Core/Object/Component.h"
#include "Core/Foundation/Common.h"

#include <array>
#include <functional>
#include <utility>

class Widget;
class UIEventSystem;

// UnityのEventTriggerに対応するイベント。パッドの選択はPointerEnterとは分離する。
enum class EventTriggerType
{
	PointerEnter, PointerExit, PointerDown, PointerUp, PointerClick,
	Drag, Drop, Scroll, UpdateSelected, Select, Deselect, Move,
	InitializePotentialDrag, BeginDrag, EndDrag, Submit, Cancel, Count
};

enum class UIMoveDirection { None, Left, Up, Right, Down };
enum class UIPointerButton { Left, Right, Middle };
enum class UINavigation { None, Horizontal, Vertical, Automatic };

// 各コールバックに渡す入力情報。MoveをUseすると既定の選択移動を止められる。
struct UIEventData
{
	Widget* target = nullptr;
	Widget* relatedTarget = nullptr;
	Vector2 position = Vector2::Zero;
	Vector2 delta = Vector2::Zero;
	Vector2 pressPosition = Vector2::Zero;
	Vector2 scrollDelta = Vector2::Zero;
	Vector2 moveVector = Vector2::Zero;
	UIMoveDirection moveDirection = UIMoveDirection::None;
	UIPointerButton button = UIPointerButton::Left;
	bool dragging = false;
	bool used = false;
	void Use() { used = true; }
};

// 画像を持たないWidgetにも付けられる入力コンポーネント。
// WidgetManagerが毎Update、描画順と有効な親階層を考慮してイベントを配送する。
class EventTrigger final : public Component
{
public:
	using Callback = std::function<void(UIEventData&)>;

	explicit EventTrigger(Object* owner) : Component(owner) {}
	const char* GetDebugName() const override { return "EventTrigger"; }
	void AddListener(EventTriggerType type, Callback callback);
	void AddActivationListener(std::function<void()> action);
	void RemoveListeners(EventTriggerType type);
	bool HasListener(EventTriggerType type) const;
	void Execute(EventTriggerType type, UIEventData& data);

	void Select() { selectionRequested = true; }
	bool IsSelected() const { return selected; }
	bool IsPointerInside() const { return pointerInside; }
	bool IsPressed() const { return pressedCount > 0; }
	void SetInteractable(bool value) { interactable = value; }
	bool IsInteractable() const { return IsActive() && interactable && inputDelay.GetRemaining() <= 0.0f; }
	void SetNavigation(UINavigation value) { navigation = value; }
	UINavigation GetNavigation() const { return navigation; }
	void SetWrapNavigation(bool value) { wrapNavigation = value; }
	void SetInputDelay(float seconds) { inputDelay.Reset(std::max(seconds, 0.0f)); }

	void OnUpdate() override;
	void OnDisabled() override;
	void OnDrawGUI() override;

private:
	friend class UIEventSystem;
	std::array<std::vector<Callback>, static_cast<size_t>(EventTriggerType::Count)> listeners;
	UINavigation navigation = UINavigation::Automatic;
	bool wrapNavigation = false;
	bool interactable = true;
	bool selected = false;
	bool pointerInside = false;
	bool selectionRequested = false;
	int pressedCount = 0;
	Game::Timer inputDelay{0.0f};
};
