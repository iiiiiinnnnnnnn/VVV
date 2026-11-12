#include <ProfileKit/ProfileKit.h>
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include "UI/Widget.h"
#include "UI/WidgetAlpha.h"
#include "UI/WidgetGroup.h"
#include "Rendering/Core/Graphics.h"
#include "IconsFontAwesome5.h"
#include <cmath>

void Widget::Update()
{
	PROFILE_FUNCTION();
	Awake();
    if (!isActive) return;

    Object::Update();
}

void Widget::DrawGUI()
{
	ImGui::PushID(this);
	DrawBasicInspector();
	if (ImGuiToolKit::CollapsingHeader(u8"2Dトランスフォーム", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGuiToolKit::Checkbox(u8"自動配置", &autoLayout);
		bool changed = ImGuiToolKit::DragPosFloat2(u8"位置", &rect.position.x);
		changed |= ImGuiToolKit::DragRotFloat(u8"角度", &rect.angle);
		changed |= ImGuiToolKit::DragSizeFloat2(u8"サイズ", &rect.size.x);
		changed |= ImGuiToolKit::DragSizeFloat2(u8"スケール", &rect.scale.x, 0.01f);
		changed |= ImGuiToolKit::DragFloat2(u8"アンカー", &rect.anchor.x, 0.01f, 0.0f, 1.0f);
		if (changed) autoLayout = false;
		ImGuiToolKit::TextDisabled(u8"手動で変更すると自動配置がオフになります");
	}
	if (ImGuiToolKit::CollapsingHeader(u8"ユーザー定義", ImGuiTreeNodeFlags_DefaultOpen)) OnDrawGUI();
	DrawComponentInspector();
	ImGui::PopID();
}

void Widget::Render(const RenderContext& rc)
{
	if (!IsActive() || IsPendingDestroy()) return;
	auto* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	const Matrix previous = renderer->GetTransform();
	const float previousAlpha = renderer->GetAlpha();
	renderer->SetTransform(GetWorldTransform());
	renderer->SetAlpha(GetWorldAlpha());
	try { Object::Render(rc); }
	catch (...)
	{
		renderer->SetTransform(previous);
		renderer->SetAlpha(previousAlpha);
		throw;
	}
	renderer->SetTransform(previous);
	renderer->SetAlpha(previousAlpha);
}

float Widget::GetWorldAlpha() const
{
	const auto* component = GetComponent<WidgetAlpha>();
	const float alpha = component && component->IsActive() ? component->GetAlpha() : 1.0f;
	return parent ? alpha * parent->GetWorldAlpha() : alpha;
}

Matrix Widget::GetWorldTransform() const
{
	return parent ? rect.GetMatrix() * parent->GetWorldTransform() : rect.GetMatrix();
}

Vector2 Widget::GetWorldCenter() const
{
	return Vector2::Transform(rect.size * 0.5f, GetWorldTransform());
}

bool Widget::ScreenToLocal(const Vector2& screen, Vector2& local) const
{
	const Matrix world = GetWorldTransform();
	if (std::abs(world.Determinant()) < 0.000001f) return false;
	local = Vector2::Transform(screen, world.Invert());
	return true;
}

bool Widget::SetScreenPosition(const Vector2& screen)
{
	if (!parent)
	{
		rect.position = screen;
		return true;
	}
	return parent->ScreenToLocal(screen, rect.position);
}

bool Widget::ContainsPoint(const Vector2& point) const
{
	if (!IsActive() || IsPendingDestroy() || rect.size.x <= 0 || rect.size.y <= 0) return false;
	Vector2 local;
	return ScreenToLocal(point, local) && local.x >= 0 && local.x <= rect.size.x &&
		local.y >= 0 && local.y <= rect.size.y;
}
