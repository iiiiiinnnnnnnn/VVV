#include <ImGuiToolKit/Alert.h>
#include <imgui.h>
#include <deque>
#include <cfloat>

namespace ImGuiToolKit
{
struct AlertMessage
{
	std::string message;
	std::string title;
};

static std::deque<AlertMessage> alerts;

void Alert(const std::string& message, const std::string& title)
{
	alerts.push_back({message, title});
}

void DrawAlerts()
{
	if (alerts.empty()) return;

	const auto& alert = alerts.front();
	const std::string popup = alert.title + "###ImGuiToolKitAlert";
	if (!ImGui::IsPopupOpen(popup.c_str())) ImGui::OpenPopup(popup.c_str());

	const auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSizeConstraints(ImVec2(280, 0), ImVec2(600, FLT_MAX));
	if (ImGui::BeginPopupModal(popup.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 550.0f);
		ImGui::TextUnformatted(alert.message.c_str());
		ImGui::PopTextWrapPos();
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(100, 0)))
		{
			ImGui::CloseCurrentPopup();
			alerts.pop_front();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

void ClearAlerts()
{
	alerts.clear();
}
} // namespace ImGuiToolKit
