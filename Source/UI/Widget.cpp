#include <ProfileKit/ProfileKit.h>
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
	const std::string label = std::string(ICON_FA_WINDOW_MAXIMIZE " ") +
		(name.empty() ? "Unnamed Object" : name) + "###WidgetInspector";
    if (ImGui::CollapsingHeader(label.c_str()))
    {
		Object::DrawGUI();

        if(ImGui::TreeNode(ICON_FA_INFO_CIRCLE " Widget Info"))
        {
			ImGui::TextDisabled("Widget info is void");
            ImGui::TreePop();
		}

        if (ImGui::TreeNode(ICON_FA_VECTOR_SQUARE " RectTransform"))
        {
            ImGui::DragFloat2("Position", &rect.position.x);
            ImGui::DragFloat("Angle", &rect.angle);
            ImGui::DragFloat2("Size", &rect.size.x);
            ImGui::DragFloat2("Scale", &rect.scale.x, 0.01f);
            ImGui::DragFloat2("Anchor", &rect.anchor.x, 0.01f, 0.0f, 1.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode(ICON_FA_SLIDERS_H " User param"))
        {
            OnDrawGUI();
            ImGui::TreePop();
        }
    }
    ImGui::PopID();

    ImGui::Separator();
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

void Widget::DrawInspector()
{
	ImGui::PushID(this);
	DrawBasicInspector();
	if (ImGui::CollapsingHeader("RectTransform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat2("Position", &rect.position.x);
		ImGui::DragFloat("Angle", &rect.angle);
		ImGui::DragFloat2("Size", &rect.size.x);
		ImGui::DragFloat2("Scale", &rect.scale.x, 0.01f);
		ImGui::DragFloat2("Anchor", &rect.anchor.x, 0.01f, 0.0f, 1.0f);
	}
	if (ImGui::CollapsingHeader((const char*)u8"ユーザーパラメーター", ImGuiTreeNodeFlags_DefaultOpen)) OnDrawGUI();
	DrawComponentInspector();
	ImGui::PopID();
}
