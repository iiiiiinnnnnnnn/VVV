#include "UI/Button.h"

#include <algorithm>
#include <cmath>

#include "UI/EventTrigger.h"
#include <stdexcept>
#include "Application/Time/GameTime.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"

Button::Button(const std::string& name, const std::string& label)
	: Widget(name), label(label), whiteTexture(std::make_shared<Texture>(Color(1, 1, 1, 1)))
{
	SetAffectedByPostProcess(false);
}

// ボタンの装飾画像は任意。未設定・未存在・読込失敗では画像だけを省略する。
static std::shared_ptr<Texture> LoadOptionalButtonTexture(const std::string& path)
{
	if (path.empty()) return {};
	try
	{
		return ResourceManager::Instance().LoadTexture(path, false);
	}
	catch (const std::runtime_error&)
	{
		return {};
	}
}

void Button::SetTexture(const std::string& path)
{
	texture = LoadOptionalButtonTexture(path);
}

void Button::SetSelectionTexture(const std::string& path)
{
	selectionTexture = LoadOptionalButtonTexture(path);
}

void Button::SetMenuStyle()
{
	labelOffset = {0, 12.0f};
	menuStyle = true;
	labelOnly = false;
	SetTexture("Resources/UI/buttonBack.png");
	SetSelectionTexture("Resources/UI/buttonFront.png");
	useSelectionSource = false;
}

void Button::SetSelectionSourceRect(const Vector2& position, const Vector2& size)
{
	selectionSourcePosition = position;
	selectionSourceSize = size;
	useSelectionSource = true;
}

void Button::OnUpdate()
{
	const auto* trigger = GetComponent<EventTrigger>();
	const bool highlighted = trigger && (trigger->IsSelected() || trigger->IsPointerInside());
	if (highlighted && !wasHighlighted) focusTime.Reset();
	wasHighlighted = highlighted;
	const float dt = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	const float target = highlighted ? 1.0f : 0.0f;
	focusBlend += (target - focusBlend) * (1.0f - std::exp(-14.0f * dt));
	if (highlighted) focusTime.Advance(dt);
}

void Button::OnDrawGUI()
{
	ImGui::DragFloat2("Label Offset", &labelOffset.x, 1.0f);
}

void Button::OnRender(const RenderContext&)
{
	SpriteRenderer* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	const Vector2 topLeft = rect.position - rect.size * rect.anchor;
	const Vector2 rotationCenter = topLeft + rect.size * 0.5f;
	const float radians = DirectX::XMConvertToRadians(rect.angle);
	const float cosine = std::cos(radians);
	const float sine = std::sin(radians);
	const auto rotateTopLeft = [&](const Vector2& position, const Vector2& size)
	{
		const Vector2 offset = position + size * 0.5f - rotationCenter;
		return rotationCenter + Vector2(offset.x * cosine - offset.y * sine,
			offset.x * sine + offset.y * cosine) - size * 0.5f;
	};
	const auto* trigger = GetComponent<EventTrigger>();
	const bool selected = trigger && trigger->IsSelected();
	const bool hovered = trigger && trigger->IsPointerInside();
	const bool highlighted = selected || hovered;
	// 通常画像は任意。画像なしのボタンでも文字と装飾は描画する。
	if (!labelOnly && texture && texture->GetWidth() > 0 && texture->GetHeight() > 0)
		renderer->Draw(SpriteShaderId::Basic, texture, {topLeft.x, topLeft.y, 0},
			rect.size, Vector2::Zero,
			Vector2(static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight())),
			rect.angle, Color(1, 1, 1, 1));
	const float easedFocus = 1.0f - std::pow(1.0f - std::clamp(focusBlend, 0.0f, 1.0f), 3.0f);
	if (!labelOnly && focusBlend > 0.005f && selectionTexture)
	{
		const Vector2 sourcePosition = useSelectionSource ? selectionSourcePosition : Vector2::Zero;
		const Vector2 sourceSize = useSelectionSource ? selectionSourceSize : Vector2(
			static_cast<float>(selectionTexture->GetWidth()), static_cast<float>(selectionTexture->GetHeight()));
		const Vector2 animatedTopLeft = topLeft + Vector2(
			-(1.0f - easedFocus) * rect.size.x * 0.075f,
			-(easedFocus - focusBlend) * rect.size.y * 0.035f);
		const Vector2 animatedSize(
			rect.size.x * (0.86f + 0.14f * easedFocus),
			rect.size.y * (0.94f + 0.06f * easedFocus));
		const Vector2 rotatedTopLeft = rotateTopLeft(animatedTopLeft, animatedSize);
		renderer->Draw(SpriteShaderId::Basic, selectionTexture,
			{rotatedTopLeft.x, rotatedTopLeft.y, 0.0f}, animatedSize,
			sourcePosition, sourceSize, rect.angle,
			hovered ? Color(1, 1, 1, focusBlend) : Color(0.92f, 0.96f, 1.0f, 0.9f * focusBlend));
	}
	if (whiteTexture && !menuStyle && !labelOnly)
	{
		const Vector2 size(rect.size.x * 0.78f, 2.0f);
		const Vector2 position = rotateTopLeft(
			topLeft + Vector2(rect.size.x * 0.08f, rect.size.y - 2.0f), size);
		renderer->Draw(SpriteShaderId::Basic, whiteTexture,
			{position.x, position.y, 0.0f}, size, Vector2::Zero, Vector2(1, 1), rect.angle,
			highlighted ? Color(0.05f, 0.82f, 1.0f, 0.95f) : Color(0.3f, 0.65f, 0.74f, 0.7f));
	}
	UIFont::Default().DrawFont(label,
		topLeft + Vector2(rect.size.x * (labelOnly ? 0.0f : 0.08f), 0.0f) + labelOffset,
		{rect.size.x * (labelOnly ? 1.0f : 0.86f), rect.size.y},
		fontSize, highlighted ? Color(0.82f, 0.97f, 1.0f, 1.0f) : Color(0.86f, 0.91f, 0.95f, 1.0f),
		menuStyle && !labelOnly ? UITextAlignment::Center : UITextAlignment::Left,
		rect.angle, rotationCenter);

	// 選択中だけ、選択帯の縁に小さな光を流してフォーカスを見失いにくくする。
	if (!labelOnly && selected && whiteTexture && focusBlend > 0.05f)
	{
		for (int i = 0; i < 5; ++i)
		{
			const float phase = focusTime.GetElapsed() * (1.45f + i * 0.08f) + i * 1.37f;
			const float pulse = std::pow(std::max(std::sin(phase), 0.0f), 5.0f);
			if (pulse < 0.02f) continue;
			const float along = std::fmod(0.17f + i * 0.23f + focusTime.GetElapsed() * 0.055f, 0.92f);
			const float size = rect.size.y * (0.055f + 0.07f * pulse);
			const Vector2 sparkle = topLeft + Vector2(
				rect.size.x * along,
				rect.size.y * (i % 2 == 0 ? 0.12f : 0.84f));
			const Vector2 rotatedSparkle = rotateTopLeft(sparkle, {size, size});
			renderer->Draw(SpriteShaderId::Basic, whiteTexture,
				{rotatedSparkle.x, rotatedSparkle.y, 0.0f}, {size, size}, Vector2::Zero,
				Vector2(1, 1), rect.angle + 45.0f, Color(0.62f, 0.94f, 1.0f, pulse * focusBlend));
		}
	}
}
