#pragma once

#include "UI/DissolveWidget.h"
#include "UI/EffectWidget.h"

class Player;
class Camera;

class CmdActionWidget : public DissolveWidget
{
public:
	enum class ActionBtns
	{
		A,
		B,
		X,
		Y
	};
	CmdActionWidget(
		ActionBtns btn, Player* player, Camera* camera, const Vector2& defaultSize = {80, 80});
	// 消去中・非表示中も、状態とディゾルブを更新する
	void Update() override;
	void OnDrawGUI() override;
	void SetExternalPrompt(float remaining, float duration);
	void PlaySuccess();
	void ResetPrompt();

protected:
	void OnAwake() override;

private:
	bool UpdateScreenPosition();
	void UpdateVisibility(bool visible, float deltaTime, float revealDuration);
	void HideImmediately();

	ActionBtns button;
	Player* player = nullptr;
	Camera* camera = nullptr;
	Vector2 defaultSize = {80.0f, 80.0f};

	std::unique_ptr<EffectWidget> effectW;
	bool externalControl = false;
	Vector2 screenPositionRatio = {0.5f, 0.7f};
	bool externalSuccess = false;
	float externalRemaining = 0.0f;
	float externalDuration = 0.0f;
	bool showing = false;
	bool wasSkillActive = false;
	bool wasContinuationQueued = false;
	float continuationDisplayDuration = 0.0f;

	float animDuration = 0.15f;
	float scaleTime = 0.0f;
	float showTimer = 0.0f;
	float showDuration = 0.15f;
	float hideDuration = 1.2f;
	float dissolveTime = 0.0f;
	float dissolveStart = 1.5f;
};
