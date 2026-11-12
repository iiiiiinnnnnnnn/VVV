#include "Application/SettingsAndDebug/SerialViewer.h"
#include <ImGuiToolKit/Widgets.h>
#include "IconsFontAwesome5.h"
#include <imgui.h>
#include <format>

class SerialViewerText
{
public:
	static const char* Kind(SerialKind kind)
	{
		switch (kind)
		{
		case SerialKind::Object:
			return "Object";
		case SerialKind::Array:
			return "Array";
		case SerialKind::PackedArray:
			return "Packed Array";
		case SerialKind::Bytes:
			return "Bytes";
		case SerialKind::String:
			return "String";
		case SerialKind::Boolean:
			return "Bool";
		case SerialKind::Signed:
			return "Int";
		case SerialKind::Unsigned:
			return "UInt";
		case SerialKind::Float:
			return "Float";
		case SerialKind::Double:
			return "Double";
		case SerialKind::Float2:
			return "Float2";
		case SerialKind::Float3:
			return "Float3";
		case SerialKind::Float4:
			return "Float4";
		case SerialKind::UInt4:
			return "UInt4";
		case SerialKind::Matrix:
			return "Matrix";
		case SerialKind::Json:
			return "JSON Value";
		case SerialKind::Deleted:
			return "Deleted";
		default:
			return "Unknown";
		}
	}
	static std::string Value(const SerialNode& node)
	{
		if (node.kind == SerialKind::Object || node.kind == SerialKind::Array)
			return std::format("{} items", node.children.size());
		if (node.kind == SerialKind::String || node.kind == SerialKind::Json)
			return std::string(node.data.begin(), node.data.begin() + std::min<size_t>(node.data.size(), 120));
		if (node.data.empty()) return {};
		if ((node.kind == SerialKind::Signed || node.kind == SerialKind::Unsigned || node.kind == SerialKind::Boolean) && node.data.size() == 8)
		{
			uint64_t value;
			std::memcpy(&value, node.data.data(), 8);
			if (node.kind == SerialKind::Boolean) return value ? "true" : "false";
			if (node.kind == SerialKind::Signed) return std::to_string(static_cast<int64_t>(value));
			return std::to_string(value);
		}
		if (node.kind == SerialKind::Float && node.data.size() == 4)
		{
			float value;
			std::memcpy(&value, node.data.data(), 4);
			return std::format("{:.6g}", value);
		}
		if (node.kind == SerialKind::Double && node.data.size() == 8)
		{
			double value;
			std::memcpy(&value, node.data.data(), 8);
			return std::format("{:.6g}", value);
		}
		if (node.kind == SerialKind::Float2 || node.kind == SerialKind::Float3 ||
			node.kind == SerialKind::Float4 || node.kind == SerialKind::Matrix || node.kind == SerialKind::UInt4)
		{
			std::string text;
			for (size_t offset = 0; offset + 4 <= node.data.size(); offset += 4)
			{
				if (offset) text += ", ";
				if (node.kind == SerialKind::UInt4)
				{
					uint32_t value;
					std::memcpy(&value, node.data.data() + offset, 4);
					text += std::to_string(value);
				}
				else
				{
					float value;
					std::memcpy(&value, node.data.data() + offset, 4);
					text += std::format("{:.6g}", value);
				}
			}
			return text;
		}
		if (node.kind == SerialKind::PackedArray && node.data.size() >= 16)
		{
			uint64_t count;
			std::memcpy(&count, node.data.data() + 8, 8);
			return std::format("{} values", count);
		}
		return std::format("{} bytes", node.data.size());
	}
};

bool SerialViewer::DrawNode(SerialNode& node, size_t depth, std::vector<uint64_t>& path, bool arrayElement)
{
	path.push_back(node.id);
	ImGui::PushID(&node);
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	const bool warning = !node.understood || !node.error.empty();
	std::string label = warning ? ICON_FA_EXCLAMATION_TRIANGLE " " : "";
	label += node.name;
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
	if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (selected == path) flags |= ImGuiTreeNodeFlags_Selected;
	if (!depth) flags |= ImGuiTreeNodeFlags_DefaultOpen;
	if (warning) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
	const bool expanded = ImGui::TreeNodeEx("##Node", flags, "%s", label.c_str());
	if (warning) ImGui::PopStyleColor();
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) { selected = path; bytePage = 0; }
	ImGui::TableSetColumnIndex(1);
	ImGui::Text("%016llX", static_cast<unsigned long long>(node.id));
	ImGui::TableSetColumnIndex(2);
	ImGui::TextUnformatted(SerialViewerText::Kind(node.kind));
	ImGui::TableSetColumnIndex(3);
	ImGui::Text("%llu", static_cast<unsigned long long>(node.ByteSize()));
	ImGui::TableSetColumnIndex(4);
	ImGui::TextUnformatted(SerialViewerText::Value(node).c_str());
	ImGui::TableSetColumnIndex(5);
	ImGui::TextUnformatted(node.kind == SerialKind::Deleted ? JText(ICON_FA_TIMES u8" 削除") :
		warning ? JText(ICON_FA_EXCLAMATION_TRIANGLE u8" 未対応") : JText(ICON_FA_CHECK u8" 読込"));
	ImGui::TableSetColumnIndex(6);
	const bool essential = !depth || (depth == 1 && node.name == "geometry");
	const bool positional = arrayElement && node.understood && !node.stableIdentity;
	ImGui::BeginDisabled(essential || positional || node.kind == SerialKind::Deleted);
	const bool remove = ImGuiToolKit::Button(ICON_FA_TIMES);
	ImGui::EndDisabled();
	if (positional && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGuiToolKit::SetTooltip(u8"位置で参照する配列要素です。親の項目全体、または通常のエディタで削除してください。");
	bool changed = false;
	if (remove) { node.Delete(); changed = true; }
	else if (expanded && !node.children.empty())
		for (auto& child : node.children) changed |= DrawNode(child, depth + 1, path, node.kind == SerialKind::Array);
	if (expanded && !(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen)) ImGui::TreePop();
	ImGui::PopID();
	path.pop_back();
	return changed;
}

bool SerialViewer::Draw(SerialDocument& document, bool* open)
{
	ImGui::SetNextWindowSize({1150, 680}, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowBgAlpha(1.0f);
	if (!ImGuiToolKit::Begin(ICON_FA_SITEMAP u8" シリアルビュアー", open))
	{
		ImGui::End();
		return false;
	}
	bool changed = false;
	if (ImGui::BeginTable("##SerialItems", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, {0, ImGui::GetContentRegionAvail().y}))
	{
		for (const auto* label : {JText(u8"項目"), "ID", JText(u8"型"), JText(u8"バイト数"),
			JText(u8"値"), JText(u8"状態"), JText(u8"操作")}) ImGui::TableSetupColumn(label);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		std::vector<uint64_t> path;
		changed = DrawNode(document.root, 0, path, false);
		ImGui::EndTable();
	}
	ImGui::End();
	return changed;
}
