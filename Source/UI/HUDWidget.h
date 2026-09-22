#pragma once

#include <memory>
#include <vector>

#include "UI/Widget.h"

// Widgetを用途ごとにまとめる階層コンテナ
class WidgetGroup : public Widget
{
public:
	WidgetGroup(const std::string& name);

	template<class T>
	std::shared_ptr<T> AddChild(const std::shared_ptr<T>& child)
	{
		children.push_back(child);
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

// 画面単位のルートWidgetGroup
class HUDWidget : public WidgetGroup
{
public:
	HUDWidget(const std::string& name) : WidgetGroup(name) {}
};
