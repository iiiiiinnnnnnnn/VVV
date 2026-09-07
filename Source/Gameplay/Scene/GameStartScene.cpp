// GameStartScene.cpp
#include "Gameplay/Scene/GameStartScene.h"

#include "Gameplay/Scene/SceneManager.h"
#include "Gameplay/Scene/TestPlayScene.h"
#include "Gameplay/Scene/VmdlEditorScene.h"
#include "Gameplay/Scene/VstgEditorScene.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Component/SpriteRenderComponent.h"
#include "Rendering/Renderer/ImGuiTheme.h"
#include "Resource/ResourceManager.h"
#include "Resource/Texture.h"
#include "Application/Time/GameTime.h"
#include "UI/SpriteWidget.h"
#include "UI/Widget.h"
#include <algorithm>
#include <cmath>
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#include <cctype>
#include <functional>
#include <map>
#endif

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
namespace
{
std::string LowerText(std::string value)
{
	// パス比較用に英字を小文字へ揃える
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

std::string LogicalSoundPath(std::filesystem::path path)
{
	// 連番付きWAVを同じ論理トラックへまとめる
	std::string stem = path.stem().string();
	const size_t bracket = stem.rfind('[');
	if (bracket != std::string::npos && !stem.empty() && stem.back() == ']')
	{
		const std::string number = stem.substr(bracket + 1, stem.size() - bracket - 2);
		if (!number.empty() && std::all_of(number.begin(), number.end(),
			[](unsigned char c) { return std::isdigit(c) != 0; })) stem.resize(bracket);
	}
	path.replace_filename(stem + ".wav");
	return path.lexically_normal().generic_string();
}

}
#endif

GameStartScene::GameStartScene()
{
	// 起動画面のヘッダー画像を生成
	const std::string headerPath = "Resources/UI/header.png";

	headerWidget = std::make_shared<SpriteWidget>(
		headerPath, SpriteShaderId::Vignette,
		Color(0, 0, 0, headerVignetteStrength));
	headerWidget->SetName("Launcher Header");
	headerWidget->GetComponent<SpriteRenderComponent>()->SetVignetteParameters(
		headerVignetteRange, headerVignetteSoftness);
	headerWidget->SetAffectedByPostProcess(false);
	widgetManager.Register(headerWidget);
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	// デバッグ用サウンド一覧を準備
	ReloadSoundTracks();
#endif
}

GameStartScene::~GameStartScene() = default;

void GameStartScene::UpdateLauncherWidgetLayout()
{
	// ウィンドウ幅に合わせてヘッダーの基準サイズを計算
	const float screenWidth = Game::Graphics::ScreenWidth;
	const float screenHeight = Game::Graphics::ScreenHeight;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f) return;
	const float contentWidth = std::min(860.0f, std::max(320.0f, screenWidth - 80.0f));
	float headerWidth = contentWidth;
	headerLayoutHeight = 176.0f;
	if (headerWidget)
	{
		// 元画像の縦横比を維持
		if (auto* sprite = headerWidget->GetComponent<SpriteRenderComponent>())
		{
			if (Texture* texture = sprite->GetTexture();
				texture && texture->GetWidth() > 0 && texture->GetHeight() > 0)
			{
				const float aspect = static_cast<float>(texture->GetWidth()) /
					static_cast<float>(texture->GetHeight());
				headerLayoutHeight = headerWidth / aspect;
				if (headerLayoutHeight > 320.0f)
				{
					headerLayoutHeight = 320.0f;
					headerWidth = headerLayoutHeight * aspect;
				}
			}
		}
		// 移動しても基準領域の端が露出しないよう画像を少し拡大
		constexpr float headerOverscan = 1.065f;
		const Vector2 baseSize(headerWidth, headerLayoutHeight);
		const Vector2 displaySize = baseSize * headerOverscan;
		const Vector2 headerPosition(
			(screenWidth - headerWidth) * 0.5f - (displaySize.x - baseSize.x) * 0.5f,
			headerTop - (displaySize.y - baseSize.y) * 0.5f);
		headerWidget->rect.position = headerPosition + headerParallaxOffset;
		headerWidget->rect.anchor = Vector2::Zero;
		headerWidget->rect.size = displaySize;
	}
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
		if (!windowConfigured) return;
	}
	// ヘッダーの動きとレイアウトを更新
	UpdateHeaderParallax();
	UpdateLauncherWidgetLayout();
	if (loadRequested) return;
}

void GameStartScene::OnDrawGUI()
{
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	// 管理画面ではヘッダーを隠す
	const bool showLauncherHeader = !showCacheManager && !showSoundManager;
	if (headerWidget) headerWidget->SetActive(showLauncherHeader);
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
#endif
	// ヘッダー下へ横一列の起動ボタンを配置
	const ImVec2 windowSize = ImGui::GetWindowSize();

	const float contentWidth = std::min(860.0f, std::max(320.0f, windowSize.x - 80.0f));
	const float contentX = (windowSize.x - contentWidth) * 0.5f;

	const float headerHeight = headerLayoutHeight;

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	constexpr int buttonCount = 5;
#else
	constexpr int buttonCount = 3;
#endif
	constexpr float buttonGap = 12.0f;
	const float buttonWidth = (contentWidth - buttonGap * (buttonCount - 1)) / buttonCount;
	const ImVec2 buttonSize(buttonWidth, 68.0f);
	const float buttonY = headerTop + headerHeight + 34.0f;
	ImGui::SetCursorPos(ImVec2(contentX, buttonY));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(buttonGap, 10.0f));
	auto menuButton = [&](const char* label, const ImVec4& normal, const ImVec4& hovered,
		const ImVec4& active) {
		ImGui::PushStyleColor(ImGuiCol_Button, normal);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(
			std::min(1.0f, hovered.x + 0.15f), std::min(1.0f, hovered.y + 0.15f),
			std::min(1.0f, hovered.z + 0.15f), 0.82f));
		const bool clicked = ImGui::Button(label, buttonSize);
		ImGui::PopStyleColor(4);
		return clicked;
	};

	// 各ボタンから対応するシーンへ移動
	if (menuButton((const char*)u8"PLAY\nゲーム開始", ImGuiTheme::YellowButton,
		ImGuiTheme::YellowButtonHovered, ImGuiTheme::YellowButtonActive))
		loadRequested = SceneManager::Instance().LoadScene<TestPlayScene>();
	ImGui::SameLine();
	if (menuButton((const char*)u8"VMDL\nモデル編集", ImGuiTheme::RedButton,
		ImGuiTheme::RedButtonHovered, ImGuiTheme::RedButtonActive))
		loadRequested = SceneManager::Instance().LoadScene<VmdlEditorScene>();
	ImGui::SameLine();
	if (menuButton((const char*)u8"VSTG\nステージ編集", ImGuiTheme::StartVstgButton,
		ImGuiTheme::StartVstgButtonHovered, ImGuiTheme::StartVstgButtonActive))
		loadRequested = SceneManager::Instance().LoadScene<VstgEditorScene>();
#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
	const ImVec4 utilityNormal(0.12f, 0.16f, 0.22f, 0.96f);
	const ImVec4 utilityHovered(0.20f, 0.29f, 0.39f, 1.0f);
	const ImVec4 utilityActive(0.28f, 0.40f, 0.53f, 1.0f);
	ImGui::SameLine();
	if (menuButton((const char*)u8"CACHE\nキャッシュ管理", utilityNormal,
		utilityHovered, utilityActive))
	{
		showCacheManager = true;
		windowConfigured = false;
		ReloadCacheList();
	}
	ImGui::SameLine();
	if (menuButton((const char*)u8"SOUND\nサウンド管理", utilityNormal,
		utilityHovered, utilityActive))
	{
		showSoundManager = true;
		windowConfigured = false;
		ReloadSoundTracks();
	}
#endif
	ImGui::PopStyleVar(3);
	ImGui::End();
	ImGui::PopStyleVar();
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
	const bool managerOpen = showCacheManager || showSoundManager;
	const int clientWidth = managerOpen ? 900 : 960;
	const int clientHeight = managerOpen ? 640 : 550;
#else
	constexpr int clientWidth = 960;
	constexpr int clientHeight = 550;
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
	try
	{
		const auto source = ResourceManager::FindSourceResourceRoot();
		if (source.empty()) throw std::runtime_error("Source Resources was not found");
		cacheSettings.Load(source / "ResourceSettings.ini");
		cachePaths = CacheBuilder::ListResources(source);
		cacheMessage.clear();
	}
	catch (const std::exception& error) { cacheMessage = error.what(); }
}

void GameStartScene::ReloadSoundTracks()
{
	// 登録済みトラックとWAVファイルを同期
	try
	{
		const auto source = ResourceManager::FindSourceResourceRoot();
		if (source.empty()) throw std::runtime_error("Source Resources was not found");
		const auto registryPath = source / "Sound" / "tracks.ini";
		SoundTrackRegistry loaded;
		loaded.Load(registryPath);

		// Resources以下のWAVを収集
		std::map<std::string, std::string> discovered;
		const auto soundRoot = source / "Sound";
		if (std::filesystem::exists(soundRoot))
		{
			for (const auto& item : std::filesystem::recursive_directory_iterator(soundRoot))
			{
				if (!item.is_regular_file() || LowerText(item.path().extension().string()) != ".wav") continue;
				const auto relative = item.path().lexically_relative(source);
				const std::string logical = LogicalSoundPath(std::filesystem::path("Resources") / relative);
				discovered.try_emplace(LowerText(logical), logical);
			}
		}

		// 消えたトラックを除外して既存番号を維持
		int nextTrack = 0;
		if (!loaded.GetEntries().empty()) nextTrack = loaded.GetEntries().rbegin()->first + 1;
		SoundTrackRegistry synchronized;
		std::set<std::string> registered;
		for (const auto& [track, entry] : loaded.GetEntries())
		{
			const auto found = discovered.find(LowerText(entry.path));
			if (found == discovered.end() || registered.contains(found->first)) continue;
			SoundTrackRegistry::Entry current = entry;
			current.path = found->second;
			synchronized.Set(track, current);
			registered.insert(found->first);
		}

		// 未登録WAVへ新しいトラック番号を割り当て
		int added = 0;
		for (const auto& [key, path] : discovered)
		{
			if (registered.contains(key)) continue;
			while (synchronized.Find(nextTrack)) ++nextTrack;
			if (nextTrack > SoundTrackRegistry::MaximumTrack)
				throw std::runtime_error("Sound track number exceeded 10000");
			synchronized.Set(nextTrack++, {path});
			++added;
		}

		bool changed = synchronized.GetEntries().size() != loaded.GetEntries().size();
		if (!changed)
		{
			auto oldEntry = loaded.GetEntries().begin();
			auto newEntry = synchronized.GetEntries().begin();
			for (; oldEntry != loaded.GetEntries().end(); ++oldEntry, ++newEntry)
			{
				if (oldEntry->first != newEntry->first ||
					oldEntry->second.path != newEntry->second.path)
				{
					changed = true;
					break;
				}
			}
		}
		// 設定と列挙ヘッダーを更新
		if (changed) synchronized.Save(registryPath);
		synchronized.GenerateHeader(
			source.parent_path() / "Source" / "Audio" / "SoundTracks.generated.h");
		soundTracks = std::move(synchronized);
		soundMessage = added > 0
			? std::to_string(added) + (const char*)u8"トラックを自動登録しました"
			: std::string{};
	}
	catch (const std::exception& error) { soundMessage = error.what(); }
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
	if (!soundMessage.empty()) ImGui::TextWrapped("%s", soundMessage.c_str());

	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("SoundTrackList", 2, flags, ImVec2(0, -1))) return;
	ImGui::TableSetupColumn((const char*)u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Enum", ImGuiTableColumnFlags_WidthFixed, 360.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	struct SoundTreeNode
	{
		std::string name;
		std::string path;
		bool file = false;
		SoundTrackRegistry::Entry entry;
		std::map<std::string, SoundTreeNode> children;
	};
	SoundTreeNode root{"Resources", "Resources"};
	for (const auto& [track, sound] : soundTracks.GetEntries())
	{
		std::vector<std::string> parts;
		for (const auto& part : std::filesystem::path(sound.path)) parts.push_back(part.string());
		if (!parts.empty() && LowerText(parts.front()) == "resources") parts.erase(parts.begin());
		if (parts.empty()) continue;
		SoundTreeNode* parent = &root;
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
		auto [item, inserted] = parent->children.try_emplace(key);
		item->second.name = parts.back();
		item->second.path = sound.path;
		item->second.file = true;
		item->second.entry = sound;
	}

	// ツリーの枝線と簡易アイコンを描画
	auto drawTreeLabel = [](const char* label, int depth, const std::vector<bool>& guides,
		bool last, bool folder)
	{
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
			if (!guides[level]) continue;
			const float x = cursor.x + static_cast<float>(level) * step + 8.0f;
			drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, rowBottom), lineColor, 1.0f);
		}
		if (depth > 0)
		{
			const float x = cursor.x + static_cast<float>(depth - 1) * step + 8.0f;
			drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, last ? centerY : rowBottom), lineColor, 1.0f);
			drawList->AddLine(ImVec2(x, centerY), ImVec2(x + 10.0f, centerY), lineColor, 1.0f);
		}
		const float iconX = cursor.x + static_cast<float>(depth) * step;
		if (folder)
		{
			const ImU32 color = IM_COL32(225, 178, 70, 255);
			drawList->AddRectFilled(ImVec2(iconX + 1.0f, cursor.y + 4.0f),
				ImVec2(iconX + 17.0f, cursor.y + 15.0f), color, 2.0f);
			drawList->AddRectFilled(ImVec2(iconX + 2.0f, cursor.y + 1.0f),
				ImVec2(iconX + 9.0f, cursor.y + 6.0f), color, 2.0f);
		}
		else
		{
			const ImU32 color = ImGui::GetColorU32(ImGuiCol_TextDisabled);
			drawList->AddRect(ImVec2(iconX + 3.0f, cursor.y + 1.0f),
				ImVec2(iconX + 15.0f, cursor.y + 16.0f), color, 1.0f, 0, 1.0f);
			drawList->AddLine(ImVec2(iconX + 6.0f, cursor.y + 6.0f),
				ImVec2(iconX + 12.0f, cursor.y + 6.0f), color);
			drawList->AddLine(ImVec2(iconX + 6.0f, cursor.y + 10.0f),
				ImVec2(iconX + 12.0f, cursor.y + 10.0f), color);
		}
		ImGui::SetCursorScreenPos(ImVec2(iconX + 22.0f, cursor.y));
		ImGui::TextUnformatted(label);
	};

	std::function<void(SoundTreeNode&, int, const std::vector<bool>&, bool)> drawNode;
	drawNode = [&](SoundTreeNode& node, int depth, const std::vector<bool>& guides, bool last)
	{
		ImGui::PushID(node.path.c_str());
		ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);
		if (!node.file)
		{
			const ImU32 background = depth == 0 ? IM_COL32(47, 61, 78, 255) : IM_COL32(39, 45, 54, 255);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, background);
		}
		ImGui::TableNextColumn();
		drawTreeLabel(node.name.c_str(), depth, guides, last, !node.file);
		if (node.file)
		{
			ImGui::TableNextColumn();
			const std::string constant =
				"SoundTrack::" + SoundTrackRegistry::ConstantName(node.entry.path);
			ImGui::TextUnformatted(constant.c_str());
		}
		size_t childIndex = 0;
		for (auto& [key, child] : node.children)
		{
			const bool childLast = ++childIndex == node.children.size();
			std::vector<bool> childGuides = guides;
			if (depth > 0) childGuides.push_back(!last);
			drawNode(child, depth + 1, childGuides, childLast);
		}
		ImGui::PopID();
	};
	drawNode(root, 0, {}, true);
	ImGui::EndTable();
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
	if (!cacheMessage.empty()) ImGui::TextWrapped("%s", cacheMessage.c_str());
	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("CacheList", 4, flags, ImVec2(0, -1))) return;
	ImGui::TableSetupColumn((const char*)u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn((const char*)u8"状態", ImGuiTableColumnFlags_WidthFixed, 80);
	ImGui::TableSetupColumn((const char*)u8"除外", ImGuiTableColumnFlags_WidthFixed, 45);
	ImGui::TableSetupColumn((const char*)u8"先読み", ImGuiTableColumnFlags_WidthFixed, 55);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	struct CacheTreeNode
	{
		std::string name;
		std::string path;
		bool file = false;
		std::map<std::string, CacheTreeNode> children;
	};
	CacheTreeNode root{ "Resources", "Resources" };
	for (const auto& path : cachePaths)
	{
		std::vector<std::string> parts;
		for (const auto& part : std::filesystem::path(path)) parts.push_back(part.string());
		if (!parts.empty() && parts.front() == "Resources") parts.erase(parts.begin());
		if (parts.empty()) continue;

		CacheTreeNode* parent = &root;
		for (size_t index = 0; index + 1 < parts.size(); ++index)
		{
			const std::string key = "0:" + parts[index];
			auto [entry, inserted] = parent->children.try_emplace(key);
			if (inserted)
			{
				entry->second.name = parts[index];
				entry->second.path = parent->path + '/' + parts[index];
			}
			parent = &entry->second;
		}
		const std::string key = "1:" + parts.back();
		auto [entry, inserted] = parent->children.try_emplace(key);
		entry->second.name = parts.back();
		entry->second.path = path;
		entry->second.file = true;
	}

	// ツリーの枝線と簡易アイコンを描画
	auto drawTreeLabel = [](const char* label, int depth, const std::vector<bool>& guides,
		bool last, bool folder)
	{
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
			if (!guides[level]) continue;
			const float x = cursor.x + static_cast<float>(level) * step + 8.0f;
			drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, rowBottom), lineColor, 1.0f);
		}
		if (depth > 0)
		{
			const float x = cursor.x + static_cast<float>(depth - 1) * step + 8.0f;
			drawList->AddLine(ImVec2(x, rowTop), ImVec2(x, last ? centerY : rowBottom), lineColor, 1.0f);
			drawList->AddLine(ImVec2(x, centerY), ImVec2(x + 10.0f, centerY), lineColor, 1.0f);
		}

		const float iconX = cursor.x + static_cast<float>(depth) * step;
		if (folder)
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
		ImGui::TextUnformatted(label);
	};

	std::function<void(CacheTreeNode&, int, const std::vector<bool>&, bool)> drawNode;
	drawNode = [&](CacheTreeNode& node, int depth, const std::vector<bool>& guides, bool last)
	{
		ImGui::PushID(node.path.c_str());
		ImGui::TableNextRow(ImGuiTableRowFlags_None, 24.0f);
		if (!node.file)
		{
			const ImU32 background = depth == 0 ? IM_COL32(47, 61, 78, 255) : IM_COL32(39, 45, 54, 255);
			ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, background);
		}
		ImGui::TableNextColumn();
		drawTreeLabel(node.name.c_str(), depth, guides, last, !node.file);

		// 除外と先読み設定を一覧から編集
		if (node.file)
		{
			const std::string& path = node.path;
			auto options = cacheSettings.Get(path);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(options.excluded ? (const char*)u8"除外" :
				std::filesystem::exists(path) ? (const char*)u8"生成済み" : (const char*)u8"未生成");
			ImGui::TableNextColumn();
			bool changed = ImGui::Checkbox("##Exclude", &options.excluded);
			if (options.excluded) options.preload = false;
			ImGui::TableNextColumn();
			ImGui::BeginDisabled(options.excluded);
			changed |= ImGui::Checkbox("##Preload", &options.preload);
			ImGui::EndDisabled();
			if (changed)
			{
				try
				{
					auto next = cacheSettings;
					next.Set(path, options);
					const auto source = ResourceManager::FindSourceResourceRoot();
					if (source.empty()) throw std::runtime_error("Source Resources was not found");
					next.Save(source / "ResourceSettings.ini");
					cacheSettings = std::move(next);
					cacheMessage.clear();
				}
				catch (const std::exception& error) { cacheMessage = error.what(); }
			}
		}

		size_t childIndex = 0;
		for (auto& [key, child] : node.children)
		{
			const bool childLast = ++childIndex == node.children.size();
			std::vector<bool> childGuides = guides;
			if (depth > 0) childGuides.push_back(!last);
			drawNode(child, depth + 1, childGuides, childLast);
		}
		ImGui::PopID();
	};
	drawNode(root, 0, {}, true);
	ImGui::EndTable();
}
#endif
