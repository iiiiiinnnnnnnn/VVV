#include "UI/CmdActionUI.h"

#include "Application/Time/GameTime.h"
#include "Core/Foundation/Easing.h"
#include "Gameplay/Camera/Camera.h"
#include "Gameplay/Player/Player.h"
#include "Rendering/Core/Graphics.h"
#include "Audio/SoundSystem.h"

CmdActionUI::CmdActionUI(ActionBtns btn, Player* player, Camera* camera, const Vector2& defaultSize)
	: DissolveWidget("CmdActionUI", ""), button(btn), player(player), camera(camera),
	  defaultSize(defaultSize)
{
	rect.size = defaultSize;
	rect.anchor = {0.5f, 0.5f};
	SetDissolveAmount(1.0f);
	SetActive(false);
	const char* texturePath = button == ActionBtns::A	? "UI/cmdA.png"
							  : button == ActionBtns::B ? "UI/cmdB.png"
							  : button == ActionBtns::X ? "UI/cmdX.png"
														: "UI/cmdY.png";
	SetTexture(texturePath);
	SetDissolveMask("Resources/UI/dissolve_animation.png");
	SetDissolveMaskScale(1.5f);
	SetDissolveSoftness(0.16f);
}

void CmdActionUI::OnAwake()
{
	DissolveWidget::OnAwake();
	effectW = std::make_unique<EffectWidget>("button_clicked", 25.0f);
}

bool CmdActionUI::UpdateScreenPosition()
{
	const Vector2 viewportSize(Game::Graphics::ScreenWidth, Game::Graphics::ScreenHeight);
	if (externalControl)
	{
		if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) return false;
		rect.position = viewportSize * screenPositionRatio;
		return true;
	}
	if (!player || !camera || !player->IsActive() || player->IsDead()) return false;
	const Vector3 chestPosition = player->transform.position + Vector3(0, 1.5f, 0);
	Vector2 screenPosition;
	if (!WorldToScreen(chestPosition, camera->GetView() * camera->GetProjection(), viewportSize, screenPosition)) return false;
	if (screenPosition.x < 0.0f || screenPosition.x > viewportSize.x) return false;
	if (screenPosition.y < 0.0f || screenPosition.y > viewportSize.y) return false;
	screenPosition += Vector2(0, -60);
	if (screenPosition.x < 0.0f || screenPosition.x > viewportSize.x) return false;
	if (screenPosition.y < 0.0f || screenPosition.y > viewportSize.y) return false;
	rect.position = screenPosition;
	return true;
}

void CmdActionUI::UpdateVisibility(bool visible, float deltaTime, float revealDuration)
{
	if (visible != showing)
	{
		showing = visible;
		dissolveStart = GetDissolveAmount();
		dissolveTime = 0.0f;
		showTimer = 0.0f;
	}
	if (showing) showTimer += deltaTime;
	dissolveTime += deltaTime;
	const float duration = showing ? revealDuration : hideDuration;
	const float elapsed = showing ? showTimer : dissolveTime;
	const float progress = duration > 0.0f ? std::clamp(elapsed / duration, 0.0f, 1.0f) : 1.0f;
	const float eased = Easing::Evaluate(progress, Easing::Type::InOutSine);
	SetDissolveAmount(std::lerp(dissolveStart, showing ? 0.0f : 1.0f, eased));
	SetActive(showing || GetDissolveAmount() < 1.0f);
}

void CmdActionUI::HideImmediately()
{
	showing = false;
	showTimer = 0.0f;
	dissolveTime = 0.0f;
	dissolveStart = 1.0f;
	SetDissolveAmount(1.0f);
	SetActive(false);
}

void CmdActionUI::SetExternalPrompt(float remaining, float duration)
{
	externalControl = true;
	externalRemaining = std::max(remaining, 0.0f);
	externalDuration = std::max(duration, externalRemaining);
}

void CmdActionUI::PlaySuccess()
{
	externalSuccess = true;
	externalRemaining = 0.0f;
}

void CmdActionUI::ResetPrompt()
{
	externalSuccess = false;
	externalRemaining = externalDuration = 0.0f;
	HideImmediately();
	if (effectW) effectW->Stop();
}

void CmdActionUI::Update()
{
	Awake();
	const bool playerReady = player && player->IsActive() && !player->IsDead();
	const bool skillActive = !externalControl && playerReady && player->IsUsingJustDodgeSkill();
	const bool skillStarted = skillActive && !wasSkillActive;
	wasSkillActive = skillActive;
	const bool continuationQueued = !externalControl && playerReady && player->IsJustDodgeSkillContinuationQueued();
	const bool continuationAccepted = continuationQueued && !wasContinuationQueued;
	wasContinuationQueued = continuationQueued;
	const bool onScreen = UpdateScreenPosition();

	if (onScreen)
	{
		scaleTime = std::fmod(
			scaleTime + Game::Time::unscaledDeltaTime, std::max(animDuration, 0.01f) * 2.0f);
		const float progress = 1.0f - std::abs(scaleTime / std::max(animDuration, 0.01f) - 1.0f);
		const float eased = Easing::Evaluate(progress, Easing::Type::InOutSine);
		rect.size = defaultSize * std::lerp(0.85f, 1.15f, eased);
	}

	// 表示の有無にかかわらず、再生済みエフェクトもボタン中央へ追従する
	effectW->rect.position = rect.position;
	effectW->rect.size = rect.size;
	effectW->rect.anchor = rect.anchor;
	effectW->SetUseUnscaledTime(externalControl);
	if (externalSuccess || skillStarted || continuationAccepted)
	{
		if (onScreen) effectW->Play();
		// 入力成功時はエフェクトだけを出して、ボタンを即座に隠す
		HideImmediately();
	}
	else
	{
		const float remaining = externalControl ? externalRemaining : !playerReady ? 0.0f : skillActive
			? player->GetJustDodgeSkillInputRemaining() : player->GetJustDodgeInputRemaining();
		if (remaining <= 0.0f) HideImmediately();
		else
		{
			if (skillActive && !showing) continuationDisplayDuration = remaining;
			// 短い二段階目の受付でも、表示と消去が区間内に収まるようにする
			const bool shortWindow = externalControl || skillActive;
			const float duration = externalControl ? externalDuration : continuationDisplayDuration;
			const float revealDuration = shortWindow ? std::min(showDuration, duration * 0.25f) : showDuration;
			const float fadeDuration = shortWindow ? std::min(hideDuration, duration * 0.5f) : hideDuration;
			UpdateVisibility(onScreen, Game::Time::unscaledDeltaTime, revealDuration);
			if (onScreen && fadeDuration > 0.0f)
			{
				// 受付終了より前から消え始め、残り0秒で完全に消える
				const float progress = 1.0f - std::clamp(remaining / fadeDuration, 0.0f, 1.0f);
				const float amount = Easing::Evaluate(progress, Easing::Type::InOutSine);
				SetDissolveAmount(std::max(GetDissolveAmount(), amount));
			}
		}
	}
	externalSuccess = false;
	effectW->Update();
	Widget::Update();
}

void CmdActionUI::OnDrawGUI()
{
	DissolveWidget::OnDrawGUI();
	if (externalControl)
		ImGui::SliderFloat2((const char*)u8"画面位置（横・縦の比率）", &screenPositionRatio.x, 0.0f, 1.0f);
	ImGui::DragFloat("animDuration", &animDuration, 0.01f, 0.01f, 10.0f);
	ImGui::DragFloat((const char*)u8"表示のディゾルブ時間", &showDuration, 0.01f, 0.0f, 5.0f);
	ImGui::DragFloat((const char*)u8"受付終了前のディゾルブ時間（ゲーム内秒）", &hideDuration, 0.01f, 0.0f, 5.0f);
	ImGui::Text("showTimer: %.2f", showTimer);
}
