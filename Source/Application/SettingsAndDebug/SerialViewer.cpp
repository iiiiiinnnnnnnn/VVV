#include "Application/SettingsAndDebug/SerialViewer.h"
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
	ImGui::TextUnformatted(node.kind == SerialKind::Deleted ? (const char*)u8"削除指定" :
		warning ? (const char*)u8"未対応・保持" : (const char*)u8"読込対応");
	ImGui::TableSetColumnIndex(6);
	const bool essential = !depth || (depth == 1 && node.name == "geometry");
	const bool positional = arrayElement && node.understood && !node.stableIdentity;
	ImGui::BeginDisabled(essential || positional || node.kind == SerialKind::Deleted);
	const bool remove = ImGui::Button((const char*)u8"削除");
	ImGui::EndDisabled();
	if (positional && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip((const char*)u8"位置で参照する配列要素です。親の項目全体、または通常のエディタで削除してください。");
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
	if (!ImGui::Begin((const char*)(ICON_FA_VIAL u8"シリアルビュアー"), open))
	{
		ImGui::End();
		return false;
	}
	bool changed = false;
	if (ImGui::BeginTable("##SerialItems", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY, {0, ImGui::GetContentRegionAvail().y * 0.7f}))
	{
		for (const auto* label : {(const char*)u8"項目", "ID", (const char*)u8"型", (const char*)u8"バイト数",
			(const char*)u8"値", (const char*)u8"状態", (const char*)u8"操作"}) ImGui::TableSetupColumn(label);
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableHeadersRow();
		std::vector<uint64_t> path;
		changed = DrawNode(document.root, 0, path, false);
		ImGui::EndTable();
	}
	SerialNode* selection = &document.root;
	for (size_t i = 1; selection && i < selected.size(); ++i) selection = selection->Find(selected[i]);
	if (selection && !selected.empty())
	{
		ImGui::SeparatorText(selection->name.c_str());
		if (!selection->error.empty()) ImGui::TextWrapped("%s", selection->error.c_str());
		const size_t pages = std::max<size_t>(1, (selection->data.size() + 511) / 512);
		bytePage = std::min(bytePage, pages - 1);
		if (ImGui::Button("<") && bytePage) --bytePage;
		ImGui::SameLine();
		ImGui::Text("%zu / %zu", bytePage + 1, pages);
		ImGui::SameLine();
		if (ImGui::Button(">") && bytePage + 1 < pages) ++bytePage;
		if (ImGui::BeginChild("##Hex", {0, 0}, ImGuiChildFlags_Borders))
		{
			for (size_t offset = bytePage * 512; offset < std::min(selection->data.size(), (bytePage + 1) * 512); offset += 16)
			{
				std::string line = std::format("{:08X}  ", offset);
				for (size_t i = offset; i < std::min(offset + 16, selection->data.size()); ++i)
					line += std::format("{:02X} ", selection->data[i]);
				ImGui::TextUnformatted(line.c_str());
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
	return changed;
}
