#include "Resource/ResourceManager.h"

#if defined(_DEBUG) || defined(VVV_DEVELOPMENT)
#include "Resource/CacheBuilder.h"
#include "Core/Foundation/Common.h"
#include <ImGuiToolKit/Widgets.h>

void ResourceManager::ReloadResourceList()
{
	resourceGUIInitialized = true;
	// リソース設定とキャッシュ対象を読み直す
	const auto source = ResourceManager::FindSourceResourceRoot();
	if (source.empty())
	{
		resourceGUIMessage = "Source Resources was not found";
		return;
	}

	try
	{
		resourceGUISettings.Load(source / "ResourceSettings.ini");
		resourceGUIPaths = CacheBuilder::ListResources(source);
		resourceGUIMessage.clear();
	}
	catch (const std::exception& error)
	{
		resourceGUIMessage = error.what();
	}
}

void ResourceManager::AddResourceTreePath(ResourceTreeNode& root,
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

void ResourceManager::DrawTreeLabel(const ResourceTreeNode& node, int depth,
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

void ResourceManager::DrawCacheTreeNode(ResourceTreeNode& node, int depth,
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
		auto options = resourceGUISettings.Get(path);
		const bool cacheExists = std::filesystem::exists(path);
		const char* statusText = JText(u8"未生成");
		if (cacheExists)
		{
			statusText = JText(u8"生成済み");
		}
		if (options.excluded)
		{
			statusText = JText(u8"除外");
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
				resourceGUIMessage = "Source Resources was not found";
			}
			else
			{
				try
				{
					auto nextSettings = resourceGUISettings;
					nextSettings.Set(path, options);
					nextSettings.Save(source / "ResourceSettings.ini");
					resourceGUISettings = std::move(nextSettings);
					resourceGUIMessage.clear();
				}
				catch (const std::exception& error)
				{
					resourceGUIMessage = error.what();
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

void ResourceManager::DrawGUI()
{
	if (!resourceGUIInitialized) ReloadResourceList();
	// キャッシュ対象をフォルダー階層へまとめて表示
	ImGuiToolKit::TextUnformatted(u8"リソース設定");
	if (!resourceGUIMessage.empty())
	{
		ImGui::TextWrapped("%s", resourceGUIMessage.c_str());
	}
	constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("CacheList", 4, flags, {0, -1})) return;
	ImGuiToolKit::TableSetupColumn(u8"名前", ImGuiTableColumnFlags_WidthStretch);
	ImGuiToolKit::TableSetupColumn(u8"状態", ImGuiTableColumnFlags_WidthFixed, 80.0f);
	ImGuiToolKit::TableSetupColumn(u8"除外", ImGuiTableColumnFlags_WidthFixed, 45.0f);
	ImGuiToolKit::TableSetupColumn(u8"先読み", ImGuiTableColumnFlags_WidthFixed, 55.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	ResourceTreeNode root{"Resources", "Resources"};
	for (const auto& path : resourceGUIPaths)
	{
		AddResourceTreePath(root, path);
	}
	DrawCacheTreeNode(root, 0, {}, true);
	ImGui::EndTable();
}
#endif
