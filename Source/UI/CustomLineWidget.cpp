#include "UI/CustomLineWidget.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include <cmath>

CustomLineWidget::CustomLineWidget(const std::string& name, const std::vector<Vector2>& points,
	const Color& color, float lineWidth)
	: Widget(name), points(points), color(color), lineWidth(std::max(lineWidth, 0.0f))
{
}

void CustomLineWidget::OnAwake()
{
	texture = std::make_shared<Texture>(Game::Graphics::Instance().GetDevice(), Color(1, 1, 1, 1));
}

void CustomLineWidget::OnRender(const RenderContext& rc)
{
	if (!texture || points.size() < 2 || lineWidth <= 0.0f || color.w <= 0.0f) return;

	for (size_t i = 1; i < points.size(); ++i)
		DrawLine(points[i - 1], points[i]);
}

void CustomLineWidget::DrawLine(const Vector2& start, const Vector2& end)
{
	const Vector2 direction = end - start;
	const Vector2 size(direction.Length(), lineWidth);
	if (size.x <= 0.0001f) return;

	const Vector2 center = (start + end) * 0.5f;
	const Vector2 position = center - size * 0.5f;
	const float angle = DEG(std::atan2(direction.y, direction.x));
	Game::Graphics::Instance().GetSpriteRenderer()->Draw(SpriteShaderId::Basic, texture,
		{position.x, position.y, 0.0f}, size, Vector2::Zero, {1.0f, 1.0f}, angle, color);
}

void CustomLineWidget::OnDrawGUI()
{
	ImGuiToolKit::ColorEdit4(u8"線の色", &color.x);
	if (ImGuiToolKit::DragSizeFloat(u8"線の太さ", &lineWidth, 0.1f)) SetLineWidth(lineWidth);
	ImGuiToolKit::TextDisabled(u8"各点は矩形左上からのピクセル座標です");
	for (size_t i = 0; i < points.size(); ++i)
	{
		ImGui::PushID(static_cast<int>(i));
		ImGuiToolKit::DragPosFloat2(u8"座標", &points[i].x, 1.0f);
		ImGui::PopID();
	}
	if (ImGuiToolKit::Button(u8"点を追加"))
		points.push_back(points.empty() ? Vector2::Zero : points.back() + Vector2(50.0f, 0.0f));
	ImGui::SameLine();
	ImGui::BeginDisabled(points.empty());
	if (ImGuiToolKit::Button(u8"末尾を削除")) points.pop_back();
	ImGui::EndDisabled();
}
