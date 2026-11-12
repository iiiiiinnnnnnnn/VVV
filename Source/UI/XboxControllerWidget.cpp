#include "UI/XboxControllerWidget.h"
#include <ImGuiToolKit/Utf8.h>
#include "UI/CustomLineWidget.h"
#include "UI/SpriteWidget.h"
#include "UI/TextWidget.h"
#include "UI/WidgetShadow.h"

XboxControllerWidget::XboxControllerWidget(const std::string& name, std::weak_ptr<Object> inputOwner, const Vector2& position, const Vector2& scale)
	: WidgetGroup(name), inputOwner(std::move(inputOwner))
{
	rect.position = position;
	rect.scale = scale;
	rect.anchor = Vector2(0.5f, 0.5f);
}

void XboxControllerWidget::OnAwake()
{
	auto xbox = std::make_shared<SpriteWidget>("Xbox", "UI/Xbox.png", SpriteShaderId::Basic);
	xbox->rect.position = {960.0f, 540.0f};
	xbox->rect.size = Vector2(4096.0f, 2160.0f) * 0.4f;
	xbox->rect.anchor = Vector2(0.5f, 0.5f);
	xbox->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});
	AddChild(xbox);

	{
		auto c = std::make_shared<CustomLineWidget>("Custom Line");
		c->AddPoint({1009.0f, 405.0f});
		c->AddPoint({1009.0f, 275.0f});
		c->AddPoint({1065.0f, 231.0f});
		c->SetLineWidth(2.0f);
		c->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"ポーズ"), 20.0f, "LINESeedJP");
		t->SetFontSize(40.0f);
		t->SetColor(Color(1, 1, 1, 1));
		t->rect.position = {1161.0f, 152.0f};
		t->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s = std::make_shared<SpriteWidget>("Menu", "UI/512w/Menu.png");
		s->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s->rect.position = {1057.0f, 154.0f};
		s->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		AddControl({c, t, s}, Control::Pause);
	}
	{
		auto c = std::make_shared<CustomLineWidget>("Custom Line");
		c->AddPoint({736.0f, 459.0f});
		c->AddPoint({692.0f, 494.0f});
		c->AddPoint({602.0f, 494.0f});
		c->SetLineWidth(2.0f);
		c->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"移動"), 20.0f, "LINESeedJP");
		t->SetFontSize(40.0f);
		t->SetColor(Color(1, 1, 1, 1));
		t->rect.position = {407.0f, 441.0f};
		t->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s = std::make_shared<SpriteWidget>("Left_Stick", "UI/512w/Left_Stick.png");
		s->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s->rect.position = {489.0f, 441.0f};
		s->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t2 = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"ダッシュ"), 20.0f, "LINESeedJP");
		t2->SetFontSize(40.0f);
		t2->SetColor(Color(1, 1, 1, 1));
		t2->rect.position = {335.0f, 542.0f};
		t2->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s2 = std::make_shared<SpriteWidget>("Left_Stick_Click", "UI/512w/Left_Stick_Click.png");
		s2->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s2->rect.position = {490.0f, 540.0f};
		s2->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		// 線だけを共用し、文字とアイコンは操作ごとに分ける。
		AddControl({c}, Control::Move, Control::Sprint);
		AddControl({t, s}, Control::Move);
		AddControl({t2, s2}, Control::Sprint);
	}
	{
		auto c = std::make_shared<CustomLineWidget>("Custom Line");
		c->AddPoint({1086.0f, 576.0f});
		c->AddPoint({1300.0f, 784.0f});
		c->AddPoint({1360.0f, 784.0f});
		c->SetLineWidth(2.0f);
		c->AddComponent<WidgetShadow>();

		auto t = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"カメラ回転"), 20.0f, "LINESeedJP");
		t->SetFontSize(40.0f);
		t->SetColor(Color(1, 1, 1, 1));
		t->rect.position = {1483.0f, 635.0f};
		t->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s = std::make_shared<SpriteWidget>("Right_Stick", "UI/512w/Right_Stick.png");
		s->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s->rect.position = {1372.0f, 635.0f};
		s->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t2 = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"しゃがみ"), 20.0f, "LINESeedJP");
		t2->SetFontSize(40.0f);
		t2->SetColor(Color(1, 1, 1, 1));
		t2->rect.position = {1482.0f, 736.0f};
		t2->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s2 = std::make_shared<SpriteWidget>("Right_Stick_Click", "UI/512w/Right_Stick_Click.png");
		s2->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s2->rect.position = {1373.0f, 738.0f};
		s2->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		// 線だけを共用し、文字とアイコンは操作ごとに分ける。
		AddControl({c}, Control::Camera, Control::Crouch);
		AddControl({t, s}, Control::Camera);
		AddControl({t2, s2}, Control::Crouch);
	}
	{
		auto c = std::make_shared<CustomLineWidget>("Custom Line");
		c->AddPoint({1085.0f, 572.0f});
		c->AddPoint({1137.0f, 535.0f});
		c->AddPoint({1196.0f, 535.0f});
		c->SetLineWidth(2.0f);
		c->rect.position = {73.0f, -269.0f};
		c->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"攻撃"), 20.0f, "LINESeedJP");
		t->SetFontSize(40.0f);
		t->SetColor(Color(1, 1, 1, 1));
		t->rect.position = {1376.0f, 215.0f};
		t->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s = std::make_shared<SpriteWidget>("Right_Trigger", "UI/512w/Right_Trigger.png");
		s->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s->rect.position = {1269.0f, 217.0f};
		s->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		AddControl({c, t, s}, Control::Attack);
	}
	{
		auto c = std::make_shared<CustomLineWidget>("Custom Line");
		c->AddPoint({835.0f, 572.0f});
		c->AddPoint({783.0f, 535.0f});
		c->AddPoint({724.0f, 535.0f});
		c->SetLineWidth(2.0f);
		c->rect.position = {-75.0f, -269.0f};
		c->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"回避"), 20.0f, "LINESeedJP");
		t->SetFontSize(40.0f);
		t->SetColor(Color(1, 1, 1, 1));
		t->rect.position = {473.0f, 215.0f};
		t->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s = std::make_shared<SpriteWidget>("Left_Trigger", "UI/512w/Left_Trigger.png");
		s->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s->rect.position = {546.0f, 217.0f};
		s->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto t2 = std::make_shared<TextWidget>(
			"Custom Line Text", JText(u8"カメラリセット"), 20.0f, "LINESeedJP");
		t2->SetFontSize(40.0f);
		t2->SetColor(Color(1, 1, 1, 1));
		t2->rect.position = {288.0f, 309.0f};
		t2->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		auto s2 = std::make_shared<SpriteWidget>("Left_Bumper", "UI/512w/Left_Bumper.png");
		s2->rect.size = Vector2(512.0f, 512.0f) * 0.2f;
		s2->rect.position = {546.0f, 309.0f};
		s2->AddComponent<WidgetShadow>(Vector2{3.5f, 3.5f});

		AddControl({c}, Control::Dodge, Control::AlignCamera);
		AddControl({t, s}, Control::Dodge);
		AddControl({t2, s2}, Control::AlignCamera);
	}
}

void XboxControllerWidget::AddControl(const std::vector<std::shared_ptr<Widget>>& widgets,
	Control control, Control secondaryControl)
{
	for (const auto& widget : widgets) AddChild(widget);
	highlights.push_back({widgets, control, secondaryControl});
}

void XboxControllerWidget::OnUpdate()
{
	WidgetGroup::OnUpdate();
	
	const auto owner = inputOwner.lock(); // プレイヤーが存在するか
	const auto* input = owner ? owner->GetComponent<LocalPlayerInput>() : nullptr;

	for (auto& group : highlights)
	{
		const bool highlighted = input && (input->IsControlHeld(group.control) ||
											  input->IsControlHeld(group.secondaryControl));
		const Color color = (highlighted || group.offTimer > 0.0f) ? Color(0,0,0,1) : Color(1, 1, 1, 1);
		if (highlighted) group.offTimer = 0.1f; // 消灯タイマー(これがないとチカチカする)
		for (const auto& widget : group.widgets)
		{
			// ボタンだけ黒くした方が綺麗だった
			// if (auto* text = dynamic_cast<TextWidget*>(widget.get())) text->SetColor(color);
			//if (auto* line = dynamic_cast<CustomLineWidget*>(widget.get()))
			//	line->SetColor(color);
			if (auto* sprite = dynamic_cast<SpriteWidget*>(widget.get()))
				sprite->SetColor(color);
		}

		group.offTimer -= Game::Time::unscaledDeltaTime;
	}
}

void XboxControllerWidget::OnDrawGUI()
{
	WidgetGroup::OnDrawGUI();

}
