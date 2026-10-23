#include "UI/TitleHUD.h"

#include <algorithm>
#include <cmath>
#include "Application/Time/GameTime.h"
#include "Application/Input/Input.h"
#include "Core/Foundation/Common.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Core/Graphics.h"
#include "UI/TitleLogo.h"
#include "UI/PressAnyButton.h"
#include "UI/Button.h"
#include "UI/EventTrigger.h"
#include "UI/UILayout.h"
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#include "Resource/ResourceManager.h"
#endif

// 単色の全面UIを生成する。配置はUpdateLayoutで毎フレーム反映する。
static std::shared_ptr<Widget> MakeTitleFill(const char* name, const Color& color)
{
	auto widget = std::make_shared<Widget>(name);
	widget->AddComponent<SpriteRenderComponent>(
		std::make_shared<Texture>(Color(1, 1, 1, 1)), SpriteShaderId::Basic, color);
	widget->SetAffectedByPostProcess(false);
	return widget;
}

TitleHUD::TitleHUD(std::function<void(int)> onSelected)
	: WidgetGroup("HUD / Title Menu"), onSelected(std::move(onSelected))
{
	mainHud = std::make_shared<WidgetGroup>("Play / Exit");
	toolsHud = std::make_shared<WidgetGroup>("Editor Labels");
	dimmer = MakeTitleFill("Background Dimmer", Color(0.01f, 0.02f, 0.04f, 0.34f));
	AddChild(dimmer);

	// タイトルロゴと、既存素材のPress Any Button。
	titleLogo = AddChild(std::make_shared<TitleLogo>());
	pressAnyButton = AddChild(std::make_shared<PressAnyButton>("Resources/UI/pressAnyButton.png"));
	AddChild(mainHud);
	AddChild(toolsHud);

	// PLAY・EXITは画像付き、開発ツールは左端に並べるラベルだけのボタン。
	const char* labels[] = {
		"Play",
		"Exit",
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
		"VMDL Editor",
		"VSTG Editor",
		"RESOURCES",
#endif
	};
	for (const char* label : labels)
	{
		const int action = static_cast<int>(menuButtons.size());
		auto button = std::make_shared<Button>(label, label);
		if (action < 2) button->SetMenuStyle(); // メニュースタイルを適用
		else button->SetLabelOnly(); // ラベルだけ
		auto* trigger = button->AddComponent<EventTrigger>();
		trigger->SetNavigation(UINavigation::Automatic);
		trigger->SetWrapNavigation(true);
		trigger->SetInteractable(false);
		trigger->AddActivationListener([this, action] { OnMenuSelected(action); });
		if (action < 2) mainHud->AddChild(button);
		else toolsHud->AddChild(button);
		menuButtons.push_back(button);
	}
	SetMenuPage(MenuPage::Prompt);

	// 起動直後の黒フェード。メニューより後に描画して画面全体を覆う。
	fadeOverlay = MakeTitleFill("Title Fade", Color(0, 0, 0, 1));
	AddChild(fadeOverlay);
	UpdateLayout();
}

void TitleHUD::OnMenuSelected(int index)
{
	if (!inputEnabled || menuPage != MenuPage::Main || menuTransition < 1.0f) return;
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	if (index == 4)
	{
		ReloadCacheList();
		SetMenuPage(MenuPage::Resources);
		return;
	}
#endif
	if (onSelected) onSelected(index);
}

void TitleHUD::UpdateLayout()
{
	// デバッグ調整用にtimescaleが0ならアップデートを通さないようにする
	if (Game::Time::scale <= 0.0f) return;

	const float width = Game::Graphics::ScreenWidth;
	const float height = Game::Graphics::ScreenHeight;
	if (width <= 0 || height <= 0) return;

	// 入力後だけメニューを画面外から滑らせる。
	const float ease = 1.0f - std::pow(1.0f - menuTransition, 3.0f);
	const Vector2 menuOffset((1.0f - ease) * width * 0.65f, 0);
	const Vector2 toolsOffset(-(1.0f - ease) * width * 0.24f, 0);

	// 背景の暗幕と、起動直後の黒フェード。
	UILayout::Apply(dimmer, {0, 0}, {1, 1});
	UILayout::Apply(fadeOverlay, {0, 0}, {1, 1});
	// ロゴはメニュー表示に合わせて少し上へ移動し、元画像の縦横比を保つ。
	// 入力待ち画像は最初の画面だけに表示する。
	UILayout::Image(pressAnyButton, {0.50f, 0.91f}, {0.32f, 0.18f}, {}, {0.5f, 0.5f});

	// ゲーム開始と終了。入力待ちを閉じてから、右側から滑ってくる。
	UILayout::MenuButton(menuButtons[0], {0.5f, 0.55f}, 38, menuOffset); // PLAY
	UILayout::MenuButton(menuButtons[1], {0.5f, 0.67f}, 32, menuOffset); // EXIT
	// 開発用の入口。画像・下線・光の装飾を付けず、左端にラベルだけを表示する。
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	UILayout::Apply(menuButtons[2], {0.02f, 0.80f}, {0.20f, 0.04f}, 21, toolsOffset); // VMDL
	UILayout::Apply(menuButtons[3], {0.02f, 0.86f}, {0.20f, 0.04f}, 21, toolsOffset); // VSTG
	UILayout::Apply(menuButtons[4], {0.02f, 0.92f}, {0.20f, 0.04f}, 19, toolsOffset); // RESOURCES
#endif
}

void TitleHUD::SetMenuPage(MenuPage page)
{
	const bool fromPrompt = menuPage == MenuPage::Prompt;
	menuPage = page;
	titleLogo->SetActive(page != MenuPage::Resources);
	dimmer->SetActive(page != MenuPage::Resources);
	pressAnyButton->SetActive(page == MenuPage::Prompt);
	mainHud->SetActive(page == MenuPage::Main);
	toolsHud->SetActive(page == MenuPage::Main);
	if (page == MenuPage::Prompt || (fromPrompt && page == MenuPage::Main)) menuTransition = 0;
	for (const auto& button : menuButtons)
		button->GetComponent<EventTrigger>()->SetInteractable(false);
	if (page == MenuPage::Main) menuButtons.front()->GetComponent<EventTrigger>()->Select();
}

static bool IsTitleInputPressed()
{
	auto& input = Game::Input::Instance();
	if (input.GetMouse().GetButtonDown() || (input.GetGamePad().GetButtonDown() & 0xffff)) return true;
	for (int button = 0; button < 5; ++button)
		if (ImGui::IsMouseClicked(button)) return true;
	for (int key = ImGuiKey_Tab; key < ImGuiKey_GamepadStart; ++key)
	{
		if (key >= ImGuiKey_F1 && key <= ImGuiKey_F12) continue;
		if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false)) return true;
	}
	return false;
}

void TitleHUD::UpdateMenu()
{
	// デバッグ調整用にtimescaleが0ならアップデートを通さないようにする
	if (Game::Time::scale <= 0.0f) return;

	// 入力とフォーカス受け取る
	const bool pressed = IsTitleInputPressed();
	const bool focused = Game::Input::IsFocusedWindow(true);
	if (!focused || !inputEnabled) return;
	if (menuPage == MenuPage::Prompt)
	{
		if (waitForCancelRelease)
		{
			const auto held = Game::Input::Instance().GetGamePad().GetButton();
			if (!(held & (GamePad::BTN_ESCAPE | GamePad::BTN_B)) &&
				!ImGui::IsKeyDown(ImGuiKey_Escape) && !ImGui::IsKeyDown(ImGuiKey_X))
				waitForCancelRelease = false;
			return;
		}
		if (pressed) SetMenuPage(MenuPage::Main);
		return;
	}

	// ESCで戻る
	const auto buttons = Game::Input::Instance().GetGamePad().GetButtonDown();
	if (buttons & (GamePad::BTN_ESCAPE | GamePad::BTN_B))
	{
		waitForCancelRelease = true;
		SetMenuPage(MenuPage::Prompt);
		return;
	}
}

void TitleHUD::OnUpdate()
{
	// デバッグ調整用にtimescaleが0ならアップデートを通さないようにする
	if (Game::Time::scale <= 0.0f) return;

	const float delta = std::max(Game::Time::unscaledDeltaTime, 0.0f);
	if (inputEnabled && menuPage == MenuPage::Main)
		menuTransition = std::clamp(menuTransition + delta * 2.5f, 0.0f, 1.0f);
	fadeAlpha = std::max(0.0f, fadeAlpha - delta / 0.8f);
	if (auto* sprite = fadeOverlay->GetComponent<SpriteRenderComponent>())
		sprite->SetColor(Color(0, 0, 0, fadeAlpha));
	UpdateLayout();
	for (const auto& button : menuButtons)
		button->GetComponent<EventTrigger>()->SetInteractable(
			inputEnabled && menuPage == MenuPage::Main && menuTransition >= 1.0f);
	if (inputEnabled) UpdateMenu();
	WidgetGroup::OnUpdate();
}

void TitleHUD::DrawResources()
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	if (menuPage != MenuPage::Resources) return;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	// リソース設定ウィンドウ：作業領域からの相対位置とサイズ比率。
	ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;
	if (ImGui::Begin("Resources", nullptr, flags)) DrawResourceSettings();
	ImGui::End();
#endif
}

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
void TitleHUD::ReloadCacheList()
{
	// リソース設定とキャッシュ対象を読み直す
	const auto source = ResourceManager::FindSourceResourceRoot();
	if (source.empty())
	{
		cacheMessage = "Source Resources was not found";
		return;
	}

	try
	{
		cacheSettings.Load(source / "ResourceSettings.ini");
		cachePaths = CacheBuilder::ListResources(source);
		cacheMessage.clear();
	}
	catch (const std::exception& error)
	{
		cacheMessage = error.what();
	}
}

void TitleHUD::AddResourceTreePath(ResourceTreeNode& root,
	const std::string& path)
{
	// パスをフォルダー単位へ分けてツリーへ追加
	std::vector<std::string> parts;
	for (const auto& part : std::filesystem::path(path))
	{
		parts.push_back(part.string());
	}
	if (!parts.empty() && ToLowerString(parts.front()) == "resources")
	{
		parts.erase(parts.begin());
	}
	if (parts.empty())
	{
		return;
	}

	ResourceTreeNode* parent = &root;
	for (size_t index = 0; index + 1 < parts.size(); ++index)
	{
		const std::string key = "0:" + parts[index];
		auto [item, inserted] = parent->children.try_emplace(key);
		if (inserted)
		{
			item->second.name = parts[index];
			item->second.path = parent->path + '/' + parts[index];
		}
		parent = &item->second;
	}

	const std::string key = "1:" + parts.back();
	auto item = parent->children.try_emplace(key).first;
	item->second.name = parts.back();
	item->second.path = path;
	item->second.file = true;
}

void TitleHUD::DrawTreeLabel(const ResourceTreeNode& node, int depth,
	const std::vector<bool>& guides, bool last)
{
	// リソースツリーの枝線。間隔・行高・中央位置・線幅はピクセル単位。
	const float step = 21.0f;
	const float rowHeight = 24.0f;
	const float centerRatio = 0.5f;
	const float branchOffset = 8.0f;
	const float branchLength = 10.0f;
	const float lineWidth = 1.0f;
	const float textOffset = 22.0f;

	// フォルダーアイコンの本体とタブの左上・右下座標、角丸。
	const Vector2 folderBodyStart = Vector2(1, 4);
	const Vector2 folderBodyEnd = Vector2(17, 15);
	const Vector2 folderTabStart = Vector2(2, 1);
	const Vector2 folderTabEnd = Vector2(9, 6);
	const float folderRounding = 2.0f;
	// ファイルアイコンの外枠と、内部の2本の横線。
	const Vector2 fileStart = Vector2(3, 1);
	const Vector2 fileEnd = Vector2(15, 16);
	const Vector2 fileLineStart = Vector2(6, 6);
	const Vector2 fileLineEnd = Vector2(12, 6);
	const float fileLineSpacing = 4.0f;
	const float fileRounding = 1.0f;

	const ImVec2 cursor = ImGui::GetCursorScreenPos();
	const ImGuiStyle& style = ImGui::GetStyle();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const float rowTop = cursor.y - style.CellPadding.y;
	const float rowBottom = rowTop + rowHeight;
	const float centerY = cursor.y + ImGui::GetTextLineHeight() * centerRatio;
	const ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_Border);
	for (size_t level = 0; level < guides.size(); ++level)
	{
		if (!guides[level]) continue;
		const float x = cursor.x + static_cast<float>(level) * step + branchOffset;
		drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, rowBottom), lineColor, lineWidth);
	}
	if (depth > 0)
	{
		const float x = cursor.x + static_cast<float>(depth - 1) * step + branchOffset;
		drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, last ? centerY : rowBottom), lineColor, lineWidth);
		drawList->AddLine(ImVec2(x, centerY), ImVec2(x + branchLength, centerY), lineColor, lineWidth);
	}

	const float iconX = cursor.x + static_cast<float>(depth) * step;
	if (!node.file)
	{
		const ImU32 color = IM_COL32(225, 178, 70, 255);
		drawList->AddRectFilled(ImVec2(iconX + folderBodyStart.x, cursor.y + folderBodyStart.y),
			ImVec2(iconX + folderBodyEnd.x, cursor.y + folderBodyEnd.y), color, folderRounding);
		drawList->AddRectFilled(ImVec2(iconX + folderTabStart.x, cursor.y + folderTabStart.y),
			ImVec2(iconX + folderTabEnd.x, cursor.y + folderTabEnd.y), color, folderRounding);
	}
	else
	{
		const ImU32 color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		drawList->AddRect(ImVec2(iconX + fileStart.x, cursor.y + fileStart.y),
			ImVec2(iconX + fileEnd.x, cursor.y + fileEnd.y), color, fileRounding, 0, lineWidth);
		drawList->AddLine(ImVec2(iconX + fileLineStart.x, cursor.y + fileLineStart.y),
			ImVec2(iconX + fileLineEnd.x, cursor.y + fileLineEnd.y), color, lineWidth);
		drawList->AddLine(ImVec2(iconX + fileLineStart.x, cursor.y + fileLineStart.y + fileLineSpacing),
			ImVec2(iconX + fileLineEnd.x, cursor.y + fileLineEnd.y + fileLineSpacing), color, lineWidth);
	}
	ImGui::SetCursorScreenPos(ImVec2(iconX + textOffset, cursor.y));
	ImGui::TextUnformatted(node.name.c_str());
}

void TitleHUD::DrawCacheTreeNode(ResourceTreeNode& node, int depth,
	const std::vector<bool>& guides, bool last)
{
	ImGui::PushID(node.path.c_str());
	const float rowHeight = 24.0f;
	ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
	if (!node.file)
	{
		ImU32 background = IM_COL32(39, 45, 54, 255);
		if (depth == 0)
		{
			background = IM_COL32(47, 61, 78, 255);
		}
		ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, background);
	}
	ImGui::TableNextColumn();
	DrawTreeLabel(node, depth, guides, last);

	// 除外と先読み設定を一覧から編集
	if (node.file)
	{
		const std::string& path = node.path;
		auto options = cacheSettings.Get(path);
		const bool cacheExists = std::filesystem::exists(path);
		const char* statusText = (const char*)u8"未生成";
		if (cacheExists)
		{
			statusText = (const char*)u8"生成済み";
		}
		if (options.excluded)
		{
			statusText = (const char*)u8"除外";
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(statusText);
		ImGui::TableNextColumn();
		bool changed = ImGui::Checkbox("##Exclude", &options.excluded);
		if (options.excluded)
		{
			options.preload = false;
		}
		ImGui::TableNextColumn();
		ImGui::BeginDisabled(options.excluded);
		const bool preloadChanged = ImGui::Checkbox("##Preload", &options.preload);
		ImGui::EndDisabled();
		if (preloadChanged)
		{
			changed = true;
		}

		if (changed)
		{
			const auto source = ResourceManager::FindSourceResourceRoot();
			if (source.empty())
			{
				cacheMessage = "Source Resources was not found";
			}
			else
			{
				try
				{
					auto nextSettings = cacheSettings;
					nextSettings.Set(path, options);
					nextSettings.Save(source / "ResourceSettings.ini");
					cacheSettings = std::move(nextSettings);
					cacheMessage.clear();
				}
				catch (const std::exception& error)
				{
					cacheMessage = error.what();
				}
			}
		}
	}

	size_t childIndex = 0;
	for (auto& childEntry : node.children)
	{
		ResourceTreeNode& child = childEntry.second;
		++childIndex;
		const bool childLast = childIndex == node.children.size();
		std::vector<bool> childGuides = guides;
		if (depth > 0)
		{
			childGuides.push_back(!last);
		}
		DrawCacheTreeNode(child, depth + 1, childGuides, childLast);
	}
	ImGui::PopID();
}

void TitleHUD::DrawResourceSettings()
{
	// キャッシュ対象をフォルダー階層へまとめて表示
	ImGui::TextUnformatted((const char*)u8"リソース設定");
	if (ImGui::Button((const char*)u8"戻る"))
	{
		SetMenuPage(MenuPage::Main);
		return;
	}
	if (!cacheMessage.empty())
	{
		ImGui::TextWrapped("%s", cacheMessage.c_str());
	}
	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("CacheList", 4, flags, {0, -1})) return;
	ImGui::TableSetupColumn((const char*)u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn((const char*)u8"状態", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGui::TableSetupColumn((const char*)u8"除外", ImGuiTableColumnFlags_WidthFixed, 45.0f);
	ImGui::TableSetupColumn((const char*)u8"先読み", ImGuiTableColumnFlags_WidthFixed, 55.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ResourceTreeNode root{"Resources", "Resources"};
	for (const auto& path : cachePaths)
	{
		AddResourceTreePath(root, path);
	}
	DrawCacheTreeNode(root, 0, {}, true);
	ImGui::EndTable();
}
#endif
