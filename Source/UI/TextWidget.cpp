#include "UI/TextWidget.h"
#include <filesystem>

TextWidget::TextWidget(const std::string& name, const std::string& text, float fontSize,
	const std::string& fontPath)
	: Widget(name), text(text), fontSize(fontSize), fontPath(fontPath)
{
	SetAffectedByPostProcess(false);
}

void TextWidget::OnAwake()
{
	if (!font && !fontPath.empty()) SetFont(fontPath);
}

bool TextWidget::SetFont(const std::string& path)
{
	if (path.empty())
	{
		font.reset();
		fontPath.clear();
		return true;
	}
	auto loaded = UIFont::Load(path);
	if (!loaded) return false;
	font = std::move(loaded);
	fontPath = path;
	return true;
}

void TextWidget::OnRender(const RenderContext&)
{
	DrawFont(Vector2::Zero, color);
}

void TextWidget::DrawShadow(const RenderContext&, const Vector2& offset,
	const Color& shadowColor)
{
	// 本体と同じ文字を指定された距離だけずらして描画
	DrawFont(offset, shadowColor);
}

void TextWidget::DrawFont(const Vector2& topLeft, const Color& drawColor)
{
	UIFont& selectedFont = font ? *font : UIFont::Default();
	selectedFont.DrawFont(text, topLeft, rect.size, fontSize, drawColor, alignment);
}

void TextWidget::OnDrawGUI()
{
	const std::string preview = fontPath.empty() ? "Default (Isometra)"
		: std::filesystem::path(fontPath).filename().string();
	ImGui::InputTextMultiline((const char*)u8"テキスト", &text);
	if (ImGui::BeginCombo((const char*)u8"フォントファミリ", preview.c_str()))
	{
		if (ImGui::Selectable((const char*)u8"既定(Isometra)", fontPath.empty())) SetFont("");
		std::error_code error;
		std::filesystem::directory_iterator files("Resources/Font", error);
		const std::filesystem::directory_iterator end;
		for (; !error && files != end; files.increment(error))
		{
			const auto& path = files->path();
			const auto extension = path.extension();
			if (extension != ".ttf" && extension != ".otf" && extension != ".ttc") continue;
			const std::string label = path.filename().string();
			const std::string value = path.generic_string();
			if (ImGui::Selectable(label.c_str(), fontPath == value) && !SetFont(value))
				ImGui::SetTooltip("Failed to load font: %s", value.c_str());
		}
		ImGui::EndCombo();
	}
	ImGui::DragFloat((const char*)u8"フォントサイズ", &fontSize);
	ImGui::ColorPicker4((const char*)u8"色", &color.x);
}
