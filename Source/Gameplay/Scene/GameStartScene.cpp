// GameStartScene.cpp
#include "Gameplay/Scene/GameStartScene.h"

#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TitleScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Scene/VstgEditorScene.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"
#include "Application/Input/Input.h"
#include "Application/Time/GameTime.h"
#include "UI/SpriteWidget.h"
#include "UI/ColorWidget.h"
#include "UI/TextWidget.h"
#include "UI/Widget.h"
#include "UI/WidgetShadow.h"
#include <algorithm>
#include <cmath>
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#include <cctype>
#endif
#include "TestPlayScene.h"
#include "Rendering/Renderer/ImGuiBinding.h"

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
static std::string LowerText(std::string value)
{
	// パス比較用に英字を小文字へ揃える
	for (char& character : value)
	{
		const unsigned char unsignedCharacter = static_cast<unsigned char>(character);
		character = static_cast<char>(std::tolower(unsignedCharacter));
	}
	return value;
}

#endif

GameStartScene::GameStartScene()
{
	// 起動画面のヘッダー画像を生成
	const std::string headerPath = "Resources/UI/header.png";

	headerWidget = std::make_shared<SpriteWidget>(headerPath, SpriteShaderId::Basic);
	headerWidget->SetName("Launcher Header");
	headerWidget->SetAffectedByPostProcess(false);
	widgetManager.Register(headerWidget);

	headerText =
		std::make_shared<TextWidget>("Header Text", (const char*)u8"VEER", 36.0f);
	headerText->rect.position = Vector2(50, 280.0f);
	headerText->rect.size = Vector2(860, 100);
	headerText->rect.anchor = Vector2::Zero;
	headerText->SetColor(Color(1, 1, 1, 1));
	headerText->SetAlignment(UITextAlignment::Left);
	// 文字本体と同じ情報を使って影を先に描画
	headerText->AddComponent<WidgetShadow>(
		Vector2(3.0f, 6.0f), Color(0.0f, 0.0f, 0.0f, 1.0f));
	widgetManager.Register(headerText);
}

GameStartScene::~GameStartScene() = default;

void GameStartScene::UpdateLauncherWidgetLayout()
{
	// 横幅を基準に画像を拡大し、縦横比を保ったまま画面全体を覆う
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	SpriteRenderComponent* headerSprite = headerWidget->GetComponent<SpriteRenderComponent>();
	Texture* texture = headerSprite->GetTexture();
	if (screenWidth <= 0.0f || screenHeight <= 0.0f || texture == nullptr)
	{
		return;
	}

	const float textureWidth = static_cast<float>(texture->GetWidth());
	const float textureHeight = static_cast<float>(texture->GetHeight());
	const float windowAspect = screenWidth / screenHeight;
	const float textureAspect = textureWidth / textureHeight;

	// カーソル移動分を確保するため画像の切り出し範囲を少し狭める
	constexpr float overscan = 1.065f;
	float cropWidth = textureWidth / overscan;
	float cropHeight = cropWidth / windowAspect;
	if (windowAspect < textureAspect)
	{
		cropHeight = textureHeight / overscan;
		cropWidth = cropHeight * windowAspect;
	}

	const float sourcePixelsPerScreenX = cropWidth / screenWidth;
	const float sourcePixelsPerScreenY = cropHeight / screenHeight;
	const float parallaxSourceX = headerParallaxOffset.x * sourcePixelsPerScreenX;
	const float parallaxSourceY = headerParallaxOffset.y * sourcePixelsPerScreenY;
	const float centeredCropX = (textureWidth - cropWidth) * 0.5f;
	const float centeredCropY = (textureHeight - cropHeight) * 0.5f;
	const Vector2 cropPosition(
		centeredCropX - parallaxSourceX,
		centeredCropY - parallaxSourceY);

	headerWidget->rect.position = Vector2::Zero;
	headerWidget->rect.anchor = Vector2::Zero;
	headerWidget->rect.size = Vector2(screenWidth, screenHeight);
	headerSprite->SetSourceRect(cropPosition, Vector2(cropWidth, cropHeight));
}

void GameStartScene::UpdateHeaderParallax()
{
	// カーソル位置を画面中央基準の値へ変換
	Vector2 target = Vector2::Zero;
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	if (screenWidth > 0.0f && screenHeight > 0.0f && Game::Input::IsFocusedWindow(true))
	{
		const Mouse& mouse = Game::Input::Instance().GetMouse();
		const float normalizedX = std::clamp(
			static_cast<float>(mouse.GetPositionX()) / screenWidth * 2.0f - 1.0f,
			-1.0f, 1.0f);
		const float normalizedY = std::clamp(
			static_cast<float>(mouse.GetPositionY()) / screenHeight * 2.0f - 1.0f,
			-1.0f, 1.0f);
		// カーソルと逆方向へ画像をずらして奥行きを出す
		target = Vector2(-normalizedX * 14.0f, -normalizedY * 7.0f);
	}

	// フレームレートに依存しない補間で滑らかに追従
	const float deltaTime = std::clamp(Game::Time::unscaledDeltaTime, 0.0f, 0.1f);
	const float blend = 1.0f - std::exp(-7.5f * deltaTime);
	headerParallaxOffset = Vector2::Lerp(headerParallaxOffset, target, blend);
}

void GameStartScene::OnUpdate()
{
	// 起動画面用のウィンドウ状態を適用
	if (!windowConfigured)
	{
		ConfigureWindow();
		if (!windowConfigured)
		{
			return;
		}
	}
	// ヘッダーの動きとレイアウトを更新
	UpdateHeaderParallax();
	UpdateLauncherWidgetLayout();
}

void GameStartScene::OnDrawGUI()
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	// 管理画面ではヘッダーを隠す
	const bool showLauncherHeader = !showCacheManager && !showSoundManager && !showEffectManager;
	headerWidget->SetActive(showLauncherHeader);
	headerText->SetActive(showLauncherHeader);
#endif

	// ウィンドウ全体を起動メニューの操作領域として使用
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(viewport->WorkSize, ImGuiCond_Always);
	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBackground;
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
	if (!ImGui::Begin((const char*)u8"起動メニュー", nullptr, flags))
	{
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	// 選択中の管理画面へ切り替え
	if (showCacheManager)
	{
		DrawCacheManager();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}
	if (showSoundManager)
	{
		DrawSoundManager();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}
	if (showEffectManager)
	{
		DrawEffectManager();
		ImGui::End();
		ImGui::PopStyleVar();
		return;
	}
#endif
	// ヘッダー下へ横一列の起動ボタンを配置
	const ImVec2 windowSize = ImGui::GetWindowSize();

	const float contentWidth = std::min(860.0f, std::max(320.0f, windowSize.x - 80.0f));
	const float contentX = (windowSize.x - contentWidth) * 0.5f;

	// リリース版は3つだけ(というかそもそも起動画面すらない)
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	constexpr int buttonCount = 6;
#else
	constexpr int buttonCount = 3;
#endif
	constexpr float buttonGap = 12.0f;
	const float buttonWidth = (contentWidth - buttonGap * (buttonCount - 1)) / buttonCount;
	const ImVec2 buttonSize(buttonWidth, 68.0f);
	constexpr float bottomMargin = 34.0f;
	const float buttonY = windowSize.y - buttonSize.y - bottomMargin;
	ImGui::SetCursorPos(ImVec2(contentX, buttonY));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(buttonGap, 10.0f));

	// 各ボタンから対応するシーンへ移動
	if (DrawMenuButton((const char*)u8"PLAY\nゲーム開始\n(LShiftで直ゲーム)",
		ImGuiTheme::YellowButton, ImGuiTheme::YellowButtonHovered,
		ImGuiTheme::YellowButtonActive, buttonSize))
	{
		if (ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftShift))
		{
			loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
		}
		else
		{
			loadRequested = SceneManager::Instance().LoadScene<TitleScene>();
		}
	}
	ImGui::SameLine();
	if (DrawMenuButton((const char*)u8"VMDL\nモデル編集", ImGuiTheme::RedButton,
		ImGuiTheme::RedButtonHovered, ImGuiTheme::RedButtonActive, buttonSize))
	{
		loadRequested = SceneManager::Instance().LoadScene<VmdlEditorScene>();
	}
	ImGui::SameLine();
	if (DrawMenuButton((const char*)u8"VSTG\nステージ編集", ImGuiTheme::StartVstgButton,
		ImGuiTheme::StartVstgButtonHovered, ImGuiTheme::StartVstgButtonActive, buttonSize))
	{
		loadRequested = SceneManager::Instance().LoadScene<VstgEditorScene>();
	}
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	const ImVec4 utilityNormal(0.12f, 0.16f, 0.22f, 0.96f);
	const ImVec4 utilityHovered(0.20f, 0.29f, 0.39f, 1.0f);
	const ImVec4 utilityActive(0.28f, 0.40f, 0.53f, 1.0f);
	ImGui::SameLine();
	if (DrawMenuButton((const char*)u8"CACHE\nキャッシュ管理", utilityNormal,
		utilityHovered, utilityActive, buttonSize))
	{
		showCacheManager = true;
		windowConfigured = false;
		ReloadCacheList();
	}
	ImGui::SameLine();
	if (DrawMenuButton((const char*)u8"SOUND\nサウンド管理", utilityNormal,
		utilityHovered, utilityActive, buttonSize))
	{
		showSoundManager = true;
		windowConfigured = false;
	}
	ImGui::SameLine();
	if (DrawMenuButton((const char*)u8"EFFECT\nエフェクト管理", utilityNormal,
		utilityHovered, utilityActive, buttonSize))
	{
		showEffectManager = true;
		windowConfigured = false;
	}
#endif
	ImGui::PopStyleVar(3);
	ImGui::End();
	ImGui::PopStyleVar();
}

bool GameStartScene::DrawMenuButton(const char* label, const ImVec4& normal,
	const ImVec4& hovered, const ImVec4& active, const ImVec2& size)
{
	// ボタンごとの色を適用してクリック状態だけを返す
	ImGui::PushStyleColor(ImGuiCol_Button, normal);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
	const float borderRed = std::min(1.0f, hovered.x + 0.15f);
	const float borderGreen = std::min(1.0f, hovered.y + 0.15f);
	const float borderBlue = std::min(1.0f, hovered.z + 0.15f);
	const ImVec4 borderColor(borderRed, borderGreen, borderBlue, 0.82f);
	ImGui::PushStyleColor(ImGuiCol_Border, borderColor);

	const bool clicked = ImGui::Button(label, size);
	ImGui::PopStyleColor(4);
	return clicked;
}

void GameStartScene::ConfigureWindow()
{
	// 起動画面用のタイトルと固定クライアントサイズを設定
	Game::Graphics& graphics = Game::Graphics::Instance();
	if (graphics.IsBorderlessFullscreen())
	{
		graphics.SetBorderlessFullscreen(false);
		return;
	}

	HWND window = graphics.GetWindowHandle();
	SetWindowTextW(window, L"TPS V Editor");
	constexpr LONG_PTR style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	const bool managerOpen = showCacheManager || showSoundManager || showEffectManager;
	int clientWidth = 960;
	int clientHeight = 490;
	if (managerOpen)
	{
		clientWidth = 900;
		clientHeight = 640;
	}
	// ヘッダーとボタン列の下に、上端と同程度の余白だけを残す
#else
	constexpr int clientWidth = 960;
	constexpr int clientHeight = 490;
#endif
	// 使用中のモニター中央へ配置
	RECT rect{0, 0, clientWidth, clientHeight};
	AdjustWindowRect(&rect, static_cast<DWORD>(style), FALSE);
	MONITORINFO monitorInfo{};
	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo);

	SetWindowLongPtr(window, GWL_STYLE, style);
	const int workWidth = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
	const int workHeight = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
	const int windowWidth = rect.right - rect.left;
	const int windowHeight = rect.bottom - rect.top;
	const int x = monitorInfo.rcWork.left + std::max(0, (workWidth - windowWidth) / 2);
	const int y = monitorInfo.rcWork.top + std::max(0, (workHeight - windowHeight) / 2);
	SetWindowPos(window, HWND_TOP, x, y,
		windowWidth, windowHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	// タイトルバーからウィンドウを移動可能にする
	graphics.SetWindowMovementLocked(false);
	windowConfigured = true;
}

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
void GameStartScene::ReloadCacheList()
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

void GameStartScene::AddResourceTreePath(ResourceTreeNode& root,
	const std::string& path, const std::string& enumName)
{
	// パスをフォルダー単位へ分けてツリーへ追加
	std::vector<std::string> parts;
	for (const auto& part : std::filesystem::path(path))
	{
		parts.push_back(part.string());
	}
	if (!parts.empty() && LowerText(parts.front()) == "resources")
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
	item->second.enumName = enumName;
	item->second.file = true;
}

void GameStartScene::DrawTreeLabel(const ResourceTreeNode& node, int depth,
	const std::vector<bool>& guides, bool last)
{
	// 階層が追いやすいように枝線と簡易アイコンを描画
	const ImVec2 cursor = ImGui::GetCursorScreenPos();
	const ImGuiStyle& style = ImGui::GetStyle();
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	constexpr float step = 21.0f;
	const float rowTop = cursor.y - style.CellPadding.y;
	const float rowBottom = rowTop + 24.0f;
	const float centerY = cursor.y + ImGui::GetTextLineHeight() * 0.5f;
	const ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_Border);

	for (size_t level = 0; level < guides.size(); ++level)
	{
		if (guides[level])
		{
			const float x = cursor.x + static_cast<float>(level) * step + 8.0f;
			drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, rowBottom), lineColor, 1.0f);
		}
	}
	if (depth > 0)
	{
		const float x = cursor.x + static_cast<float>(depth - 1) * step + 8.0f;
		float branchBottom = rowBottom;
		if (last)
		{
			branchBottom = centerY;
		}
		drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, branchBottom), lineColor, 1.0f);
		drawList->AddLine(ImVec2(x, centerY), ImVec2(x + 10.0f, centerY), lineColor, 1.0f);
	}

	const float iconX = cursor.x + static_cast<float>(depth) * step;
	if (!node.file)
	{
		const ImU32 folderColor = IM_COL32(225, 178, 70, 255);
		drawList->AddRectFilled(ImVec2(iconX + 1.0f, cursor.y + 4.0f),
			ImVec2(iconX + 17.0f, cursor.y + 15.0f), folderColor, 2.0f);
		drawList->AddRectFilled(ImVec2(iconX + 2.0f, cursor.y + 1.0f),
			ImVec2(iconX + 9.0f, cursor.y + 6.0f), folderColor, 2.0f);
	}
	else
	{
		const ImU32 fileColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		drawList->AddRect(ImVec2(iconX + 3.0f, cursor.y + 1.0f),
			ImVec2(iconX + 15.0f, cursor.y + 16.0f), fileColor, 1.0f, 0, 1.0f);
		drawList->AddLine(ImVec2(iconX + 6.0f, cursor.y + 6.0f),
			ImVec2(iconX + 12.0f, cursor.y + 6.0f), fileColor);
		drawList->AddLine(ImVec2(iconX + 6.0f, cursor.y + 10.0f),
			ImVec2(iconX + 12.0f, cursor.y + 10.0f), fileColor);
	}
	ImGui::SetCursorScreenPos(ImVec2(iconX + 22.0f, cursor.y));
	ImGui::TextUnformatted(node.name.c_str());
}

void GameStartScene::DrawEnumTreeNode(ResourceTreeNode& node,
	const std::string& enumPrefix, int depth, const std::vector<bool>& guides, bool last)
{
	ImGui::PushID(node.path.c_str());
	ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);
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
	if (node.file)
	{
		ImGui::TableNextColumn();
		const std::string constant = enumPrefix + node.enumName;
		ImGui::TextUnformatted(constant.c_str());
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
		DrawEnumTreeNode(child, enumPrefix, depth + 1, childGuides, childLast);
	}
	ImGui::PopID();
}

void GameStartScene::DrawEffectManager()
{
	// エフェクトをフォルダー階層へまとめて表示
	ImGui::TextUnformatted((const char*)u8"エフェクト管理");
	if (ImGui::Button((const char*)u8"戻る"))
	{
		showEffectManager = false;
		windowConfigured = false;
		return;
	}
	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("EffectList", 2, flags, ImVec2(0, -1)))
	{
		return;
	}
	ImGui::TableSetupColumn((const char*)u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Enum", ImGuiTableColumnFlags_WidthFixed, 360.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ResourceTreeNode root{"Resources", "Resources"};
	for (const auto& effect : EffectDefinitions)
	{
		AddResourceTreePath(root, std::string(effect.path), std::string(effect.name));
	}
	DrawEnumTreeNode(root, "EffectId::", 0, {}, true);
	ImGui::EndTable();
}

void GameStartScene::DrawSoundManager()
{
	// サウンドをフォルダー階層へまとめて表示
	ImGui::TextUnformatted((const char*)u8"サウンド管理");
	if (ImGui::Button((const char*)u8"戻る"))
	{
		showSoundManager = false;
		windowConfigured = false;
		return;
	}
	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("SoundTrackList", 2, flags, ImVec2(0, -1)))
	{
		return;
	}
	ImGui::TableSetupColumn((const char*)u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Enum", ImGuiTableColumnFlags_WidthFixed, 360.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ResourceTreeNode root{"Resources", "Resources"};
	for (const auto& sound : SoundDefinitions)
	{
		AddResourceTreePath(root, std::string(sound.path), std::string(sound.name));
	}
	DrawEnumTreeNode(root, "SoundTrack::", 0, {}, true);
	ImGui::EndTable();
}

void GameStartScene::DrawCacheTreeNode(ResourceTreeNode& node, int depth,
	const std::vector<bool>& guides, bool last)
{
	ImGui::PushID(node.path.c_str());
	ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);
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

void GameStartScene::DrawCacheManager()
{
	// キャッシュ対象をフォルダー階層へまとめて表示
	ImGui::TextUnformatted((const char*)u8"キャッシュ管理");
	if (ImGui::Button((const char*)u8"戻る"))
	{
		showCacheManager = false;
		windowConfigured = false;
		return;
	}
	if (!cacheMessage.empty())
	{
		ImGui::TextWrapped("%s", cacheMessage.c_str());
	}
	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("CacheList", 4, flags, ImVec2(0, -1)))
	{
		return;
	}
	ImGui::TableSetupColumn((const char*)u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn((const char*)u8"状態", ImGuiTableColumnFlags_WidthFixed, 80);
	ImGui::TableSetupColumn((const char*)u8"除外", ImGuiTableColumnFlags_WidthFixed, 45);
	ImGui::TableSetupColumn((const char*)u8"先読み", ImGuiTableColumnFlags_WidthFixed, 55);
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
