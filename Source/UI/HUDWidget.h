#pragma once

#include <memory>
#include <vector>

#include "UI/Widget.h"

// UnityのCanvasに相当する、画面単位のWidgetコンテナ。
class HUDWidget : public Widget
{
public:
	explicit HUDWidget(const std::string& name);

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
