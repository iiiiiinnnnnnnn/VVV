#pragma once

#include "Application/Time/Timer.h"

#include <memory>
#include <string>
#include <utility>

#include "UI/UIFont.h"
#include "UI/Widget.h"

class Texture;

// 画像・選択時の装飾・ラベルをまとめて描画するWidget。
// 入力が必要な場合は、通常のWidgetと同じくEventTriggerを別途追加する。
class Button final : public Widget
{
public:
	Button(const std::string& name, const std::string& label = "");

	void SetFontSize(float value) { fontSize = value; }
	void SetLabel(const std::string& value) { label = value; }
	// ボタンのローカル座標で指定するピクセル単位のずれ。右・下が正。
	void SetLabelOffset(const Vector2& value) { labelOffset = value; }
	const Vector2& GetLabelOffset() const { return labelOffset; }
	// 空文字・未存在の画像は未設定として扱い、画像なしでも文字を表示する。
	void SetTexture(const std::string& path);
	void SetTexture(std::shared_ptr<Texture> value) { texture = std::move(value); }
	void SetSelectionTexture(const std::string& path);
	// 共通の氷結ボタン素材。装飾を切り取らず画像全体を表示する。
	void SetMenuStyle();
	// 背景・下線・光の装飾を省き、選択時の文字色だけを変える。
	void SetLabelOnly(bool value = true) { labelOnly = value; }
	void SetSelectionSourceRect(const Vector2& position, const Vector2& size);

protected:
	void OnUpdate() override;
	void OnDrawGUI() override;
	void OnRender(const RenderContext& rc) override;

private:
	std::string label;
	Vector2 labelOffset = Vector2::Zero;
	std::shared_ptr<Texture> texture;
	std::shared_ptr<Texture> selectionTexture;
	std::shared_ptr<Texture> whiteTexture;
	Vector2 selectionSourcePosition = Vector2::Zero;
	Vector2 selectionSourceSize = Vector2::Zero;
	bool useSelectionSource = false;
	bool menuStyle = false;
	bool labelOnly = false;
	bool wasHighlighted = false;
	float fontSize = 42.0f;
	float focusBlend = 0.0f;
	Game::Timer focusTime{0.0f};
};
