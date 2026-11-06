#pragma once

#include <memory>
#include <vector>

#include "UI/Widget.h"

// Widgetを用途ごとにまとめる階層コンテナ
class WidgetGroup : public Widget
{
public:
	WidgetGroup(const std::string& name);
	~WidgetGroup() override;
	void RemoveChild(Widget* child);

	template<class T>
	std::shared_ptr<T> AddChild(std::shared_ptr<T> child)
	{
		if (!child || child->IsPendingDestroy() || child->parent == this) return child;
		for (Widget* ancestor = this; ancestor; ancestor = ancestor->GetParent())
			if (ancestor == child.get()) return child;

		child->Awake();
		if (child->parent) child->parent->RemoveChild(child.get());
		children.push_back(child);
		child->parent = this;
		child->Start();
		return child;
	}

	const std::vector<std::shared_ptr<Widget>>& GetChildren() const { return children; }
	void SetChildrenActive(bool active);

protected:
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

private:
	std::vector<std::shared_ptr<Widget>> children;
};
