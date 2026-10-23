#pragma once

#include "Application/Time/Timer.h"

#include "UI/EventTrigger.h"
#include "UI/Widget.h"

#include <array>
#include <memory>
#include <vector>

// シーンのWidgetManagerごとに1つ所有する入力ルーター。
// 後から描画されるWidgetを優先し、親の非表示・破棄やImGuiへの入力を考慮する。
class UIEventSystem
{
public:
	void Update(const std::vector<std::shared_ptr<Widget>>& roots);
	void SetDragThreshold(float pixels) { dragThreshold = std::max(pixels, 0.0f); }

private:
	struct Target
	{
		std::shared_ptr<Widget> widget;
		std::vector<std::shared_ptr<Widget>> parents;
	};
	struct PointerState
	{
		std::weak_ptr<Widget> pressed;
		Vector2 pressPosition = Vector2::Zero;
		bool dragging = false;
	};

	void Collect(const std::shared_ptr<Widget>& widget, std::vector<std::shared_ptr<Widget>>& parents);
	bool IsAvailable(const std::shared_ptr<Widget>& widget) const;
	std::shared_ptr<Widget> Raycast(const Vector2& position) const;
	void Dispatch(const std::shared_ptr<Widget>& widget, EventTriggerType type, UIEventData& data);
	void SetHovered(const std::shared_ptr<Widget>& widget, UIEventData& data);
	void SetSelected(const std::shared_ptr<Widget>& widget, UIEventData& data);
	void ResetPointer(UIEventData& data);
	void UpdatePointer(UIEventData& data);
	void UpdateNavigation(UIEventData& data);
	std::shared_ptr<Widget> FindNext(const std::shared_ptr<Widget>& current, const Vector2& direction) const;

	std::vector<Target> targets;
	std::weak_ptr<Widget> hovered;
	std::weak_ptr<Widget> selected;
	std::array<PointerState, 3> pointers;
	Vector2 lastPosition = Vector2::Zero;
	Vector2 lastMove = Vector2::Zero;
	Game::Timer moveTimer{0.0f};
	float dragThreshold = 5.0f;
	bool submitWasHeld = true;
	bool submitArmed = false;
	bool cancelWasHeld = true;
	bool hasPosition = false;
};
