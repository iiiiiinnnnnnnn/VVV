#include "UI/Button.h"

#include <algorithm>
#include <cmath>

#include "Application/Input/Input.h"
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

void Button::SetSelectionTexture(const std::string& path)
{
	selectionTexture = ResourceManager::Instance().LoadTexture(path);
}

void Button::SetSelectionSourceRect(const Vector2& position, const Vector2& size)
{
	selectionSourcePosition = position;
	selectionSourceSize = size;
	useSelectionSource = true;
}

void Button::Activate()
{
	if (onClick) onClick();
}

void Button::SetSelected(bool value)
{
	if (value && !selected) focusTime = 0.0f;
	selected = value;
}

void Button::OnUpdate()
{
	Mouse& mouse = Game::Input::Instance().GetMouse();
	const float left = rect.position.x - rect.size.x * rect.anchor.x;
	const float top = rect.position.y - rect.size.y * rect.anchor.y;
	const float x = static_cast<float>(mouse.GetPositionX());
	const float y = static_cast<float>(mouse.GetPositionY());
	hovered = x >= left && x <= left + rect.size.x && y >= top && y <= top + rect.size.y;
	const float dt = (std::max)(Game::Time::unscaledDeltaTime, 0.0f);
	const float target = selected || hovered ? 1.0f : 0.0f;
	focusBlend += (target - focusBlend) * (1.0f - std::exp(-14.0f * dt));
	if (selected || hovered) focusTime += dt;
	if (hovered && (mouse.GetButtonDown() & Mouse::BTN_LEFT)) Activate();
}

void Button::OnRender(const RenderContext&)
{
	SpriteRenderer* renderer = Game::Graphics::Instance().GetSpriteRenderer();
	const Vector2 topLeft = rect.position - rect.size * rect.anchor;
	const bool highlighted = selected || hovered;
	const float easedFocus = 1.0f - std::pow(1.0f - std::clamp(focusBlend, 0.0f, 1.0f), 3.0f);
	if (focusBlend > 0.005f && selectionTexture)
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
		renderer->Draw(SpriteShaderId::Basic, selectionTexture,
			{animatedTopLeft.x, animatedTopLeft.y, 0.0f}, animatedSize,
			sourcePosition, sourceSize, 0.0f,
			hovered ? Color(1, 1, 1, focusBlend) : Color(0.92f, 0.96f, 1.0f, 0.9f * focusBlend));
	}
	if (whiteTexture)
		renderer->Draw(SpriteShaderId::Basic, whiteTexture,
			{topLeft.x + rect.size.x * 0.08f, topLeft.y + rect.size.y - 2.0f, 0.0f},
			{rect.size.x * 0.78f, 2.0f}, Vector2::Zero, Vector2(1, 1), 0.0f,
			highlighted ? Color(0.05f, 0.82f, 1.0f, 0.95f) : Color(0.3f, 0.65f, 0.74f, 0.7f));
	UIFont::Default().DrawText(label,
		{topLeft.x + rect.size.x * 0.08f, topLeft.y}, {rect.size.x * 0.86f, rect.size.y},
		fontSize, highlighted ? Color(0.72f, 0.94f, 1.0f, 1.0f) : Color(0.86f, 0.88f, 0.9f, 0.95f));

	// 選択中だけ、選択帯の縁に小さな光を流してフォーカスを見失いにくくする。
	if (selected && whiteTexture && focusBlend > 0.05f)
	{
		for (int i = 0; i < 5; ++i)
		{
			const float phase = focusTime * (1.45f + i * 0.08f) + i * 1.37f;
			const float pulse = std::pow((std::max)(std::sin(phase), 0.0f), 5.0f);
			if (pulse < 0.02f) continue;
			const float along = std::fmod(0.17f + i * 0.23f + focusTime * 0.055f, 0.92f);
			const float size = rect.size.y * (0.055f + 0.07f * pulse);
			const Vector2 sparkle = topLeft + Vector2(
				rect.size.x * along,
				rect.size.y * (i % 2 == 0 ? 0.12f : 0.84f));
			renderer->Draw(SpriteShaderId::Basic, whiteTexture,
				{sparkle.x, sparkle.y, 0.0f}, {size, size}, Vector2::Zero,
				Vector2(1, 1), RAD(45.0f), Color(0.62f, 0.94f, 1.0f, pulse * focusBlend));
		}
	}
}
