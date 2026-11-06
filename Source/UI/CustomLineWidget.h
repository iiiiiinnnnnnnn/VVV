#pragma once

#include "UI/Widget.h"
#include <algorithm>
#include <vector>

class CustomLineWidget : public Widget
{
  public:
	CustomLineWidget(const std::string& name = "", const std::vector<Vector2>& points = {},
		const Color& color = Color(1, 1, 1, 1), float lineWidth = 2.0f);
	void OnAwake() override;

	void SetPoints(const std::vector<Vector2>& value) { points = value; }
	const std::vector<Vector2>& GetPoints() const { return points; }
	void AddPoint(const Vector2& point) { points.push_back(point); }
	void ClearPoints() { points.clear(); }
	void SetColor(const Color& value) { color = value; }
	const Color& GetColor() const { return color; }
	void SetLineWidth(float value) { lineWidth = std::max(value, 0.0f); }
	float GetLineWidth() const { return lineWidth; }

  protected:
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;

  private:
	void DrawLine(const Vector2& start, const Vector2& end);

	std::vector<Vector2> points;
	Color color;
	float lineWidth;
	std::shared_ptr<Texture> texture;
};
