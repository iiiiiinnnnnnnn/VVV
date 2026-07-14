// DynamicAnimationEditorWindow.h

#pragma once
#include <imgui.h>
#include <imgui_stdlib.h>

#include <utility>
#include <variant>
#include <cstring>

#include "Core/Foundation/Common.h"
#include "Application/Tools/Dialog.h"
#include "Application/Tools/DynamicAnimation.h"
#include "Application/Tools/DynamicAnimationSerializer.h"

#include <algorithm>
#include <string>

class DynamicAnimationEditorWindow
{
public:
	void Draw(bool* pOpen)
	{
		if (!pOpen || !*pOpen)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(980, 680), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Dynamic Animation Editor", pOpen))
		{
			ImGui::End();
			return;
		}

		DrawToolbar();
		ImGui::Separator();
		DrawClipSettings();
		ImGui::Separator();
		DrawTrackList();

		ImGui::End();
	}

private:
	void DrawToolbar()
	{
		if (ImGui::Button("New"))
		{
			NewClip();
		}

		ImGui::SameLine();
		if (ImGui::Button("Save"))
		{
			Save();
		}

		ImGui::SameLine();
		if (ImGui::Button("Save As"))
		{
			SaveAs();
		}

		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			Load();
		}

		ImGui::SameLine();
		ImGui::TextDisabled(
			currentFilePath[0] != '\0'
				? currentFilePath
				: "(unsaved)");

		if (!statusMessage.empty())
		{
			ImGui::TextWrapped("%s", statusMessage.c_str());
		}
	}

	void DrawClipSettings()
	{
		ImGui::SetNextItemWidth(280.0f);
		ImGui::InputText("Clip Name", &clip.name);

		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::DragFloat(
			"Length",
			&clip.length,
			0.01f,
			0.001f,
			9999.0f,
			"%.3f sec"))
		{
			clip.length = std::max(clip.length, 0.001f);
			ClampAllKeyTimes();
		}

		ImGui::TextDisabled(
			"Loop and speed are Animator State settings, not clip settings.");
	}

	void DrawTrackList()
	{
		if (ImGui::Button("+ Widget Property"))
		{
			AddWidgetTrack();
		}

		ImGui::SameLine();
		if (ImGui::Button("+ Shader Param"))
		{
			AddShaderParamTrack();
		}

		ImGui::SameLine();
		ImGui::TextDisabled("Tracks: %d", static_cast<int>(clip.tracks.size()));

		ImGui::Spacing();

		for (int i = 0; i < static_cast<int>(clip.tracks.size()); ++i)
		{
			ImGui::PushID(i);

			DynamicAnimationTrack& track = clip.tracks[i];
			const std::string label = MakeTrackLabel(track, i);

			bool deleteTrack = false;
			if (ImGui::TreeNodeEx(
				"Track",
				ImGuiTreeNodeFlags_DefaultOpen,
				"%s",
				label.c_str()))
			{
				DrawTrackSettings(track);
				ImGui::Separator();
				DrawKeys(track);

				if (ImGui::Button("Delete Track"))
				{
					deleteTrack = true;
				}

				ImGui::TreePop();
			}

			ImGui::PopID();

			if (deleteTrack)
			{
				clip.tracks.erase(clip.tracks.begin() + i);
				break;
			}

			ImGui::Spacing();
		}
	}

	void DrawTrackSettings(DynamicAnimationTrack& track)
	{
		ImGui::Text(
			"Target: %s",
			track.target == DynamicAnimationTarget::WidgetProperty
				? "Widget"
				: "ShaderParam");

		if (track.target == DynamicAnimationTarget::WidgetProperty)
		{
			DrawWidgetPropertyCombo(track);
			ImGui::Text("Value Type: %s", ToString(track.valueType));
		}
		else
		{
			ImGui::SetNextItemWidth(260.0f);
			ImGui::InputText("Param Name", &track.shaderParamName);
			DrawValueTypeCombo(track);
		}
	}

	void DrawWidgetPropertyCombo(DynamicAnimationTrack& track)
	{
		static const DynamicWidgetProperty properties[] =
		{
			DynamicWidgetProperty::Position,
			DynamicWidgetProperty::Angle,
			DynamicWidgetProperty::Size,
			DynamicWidgetProperty::Anchor
		};

		if (ImGui::BeginCombo(
			"Property",
			ToString(track.widgetProperty)))
		{
			for (DynamicWidgetProperty property : properties)
			{
				const bool selected = track.widgetProperty == property;
				if (ImGui::Selectable(ToString(property), selected))
				{
					track.widgetProperty = property;
					track.valueType = GetWidgetPropertyValueType(property);
					ResetTrackValues(track);
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
	}

	void DrawValueTypeCombo(DynamicAnimationTrack& track)
	{
		static const DynamicValueType types[] =
		{
			DynamicValueType::Bool,
			DynamicValueType::Short,
			DynamicValueType::Int,
			DynamicValueType::Float,
			DynamicValueType::Color,
			DynamicValueType::Vector2,
			DynamicValueType::Vector3,
			DynamicValueType::Vector4
		};

		if (ImGui::BeginCombo("Value Type", ToString(track.valueType)))
		{
			for (DynamicValueType type : types)
			{
				const bool selected = track.valueType == type;
				if (ImGui::Selectable(ToString(type), selected))
				{
					track.valueType = type;
					ResetTrackValues(track);
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
	}

	void DrawKeys(DynamicAnimationTrack& track)
	{
		if (ImGui::Button("+ Add Key"))
		{
			DynamicAnimationKey key;
			key.time = track.keys.empty()
				? 0.0f
				: std::min(clip.length, track.keys.back().time + 0.1f);
			key.value = track.keys.empty()
				? MakeDefaultDynamicValue(track.valueType)
				: track.keys.back().value;
			key.interpolation = IsDynamicValueInterpolatable(track.valueType)
				? DynamicInterpolation::Linear
				: DynamicInterpolation::Step;
			track.keys.push_back(key);
		}

		if (track.keys.empty())
		{
			ImGui::TextDisabled("No keys.");
			return;
		}

		ImGui::TextDisabled(
			"Interpolation is applied from each key to the next key.");

		for (int keyIndex = 0;
			 keyIndex < static_cast<int>(track.keys.size());
			 ++keyIndex)
		{
			ImGui::PushID(keyIndex);

			DynamicAnimationKey& key = track.keys[keyIndex];
			bool deleteKey = false;

			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::DragFloat(
				"Time",
				&key.time,
				0.01f,
				0.0f,
				clip.length,
				"%.3f"))
			{
				key.time = std::clamp(key.time, 0.0f, clip.length);
			}

			ImGui::SameLine();
			DrawValueEditor("Value", key.value, track.valueType);

			ImGui::SameLine();
			DrawInterpolationCombo(key, track.valueType);

			ImGui::SameLine();
			if (ImGui::SmallButton("Delete"))
			{
				deleteKey = true;
			}

			ImGui::PopID();

			if (deleteKey)
			{
				track.keys.erase(track.keys.begin() + keyIndex);
				break;
			}
		}

		std::stable_sort(
			track.keys.begin(),
			track.keys.end(),
			[](const DynamicAnimationKey& a, const DynamicAnimationKey& b)
			{
				return a.time < b.time;
			});
	}

	void DrawValueEditor(
		const char* label,
		ParamValue& value,
		DynamicValueType type)
	{
		switch (type)
		{
		case DynamicValueType::Bool:
		{
			bool& v = std::get<bool>(value);
			ImGui::Checkbox(label, &v);
			break;
		}

		case DynamicValueType::Short:
		{
			short& v = std::get<short>(value);
			int temp = v;
			ImGui::SetNextItemWidth(170.0f);
			if (ImGui::DragInt(label, &temp, 1.0f))
			{
				v = static_cast<short>(std::clamp(temp, -32768, 32767));
			}
			break;
		}

		case DynamicValueType::Int:
		{
			int& v = std::get<int>(value);
			ImGui::SetNextItemWidth(170.0f);
			ImGui::DragInt(label, &v, 1.0f);
			break;
		}

		case DynamicValueType::Float:
		{
			float& v = std::get<float>(value);
			ImGui::SetNextItemWidth(170.0f);
			ImGui::DragFloat(label, &v, 0.01f);
			break;
		}

		case DynamicValueType::Color:
		{
			Color& v = std::get<Color>(value);
			ImGui::SetNextItemWidth(230.0f);
			ImGui::ColorEdit4(label, &v.x);
			break;
		}

		case DynamicValueType::Vector2:
		{
			Vector2& v = std::get<Vector2>(value);
			ImGui::SetNextItemWidth(230.0f);
			ImGui::DragFloat2(label, &v.x, 0.01f);
			break;
		}

		case DynamicValueType::Vector3:
		{
			Vector3& v = std::get<Vector3>(value);
			ImGui::SetNextItemWidth(280.0f);
			ImGui::DragFloat3(label, &v.x, 0.01f);
			break;
		}

		case DynamicValueType::Vector4:
		{
			Vector4& v = std::get<Vector4>(value);
			ImGui::SetNextItemWidth(320.0f);
			ImGui::DragFloat4(label, &v.x, 0.01f);
			break;
		}
		}
	}

	void DrawInterpolationCombo(
		DynamicAnimationKey& key,
		DynamicValueType valueType)
	{
		if (!IsDynamicValueInterpolatable(valueType))
		{
			key.interpolation = DynamicInterpolation::Step;
			ImGui::TextDisabled("Step");
			return;
		}

		static const DynamicInterpolation modes[] =
		{
			DynamicInterpolation::Step,
			DynamicInterpolation::Linear,
			DynamicInterpolation::EaseIn,
			DynamicInterpolation::EaseOut,
			DynamicInterpolation::EaseInOut
		};

		ImGui::SetNextItemWidth(130.0f);
		if (ImGui::BeginCombo(
			"Interpolation",
			ToString(key.interpolation)))
		{
			for (DynamicInterpolation mode : modes)
			{
				const bool selected = key.interpolation == mode;
				if (ImGui::Selectable(ToString(mode), selected))
				{
					key.interpolation = mode;
				}

				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}

			ImGui::EndCombo();
		}
	}

	void AddWidgetTrack()
	{
		DynamicAnimationTrack track;
		track.target = DynamicAnimationTarget::WidgetProperty;
		track.widgetProperty = DynamicWidgetProperty::Position;
		track.valueType = DynamicValueType::Vector2;
		track.keys.push_back(MakeDefaultKey(track.valueType));
		clip.tracks.push_back(track);
	}

	void AddShaderParamTrack()
	{
		DynamicAnimationTrack track;
		track.target = DynamicAnimationTarget::ShaderParam;
		track.shaderParamName = "color";
		track.valueType = DynamicValueType::Color;
		track.keys.push_back(MakeDefaultKey(track.valueType));
		clip.tracks.push_back(track);
	}

	DynamicAnimationKey MakeDefaultKey(DynamicValueType type) const
	{
		DynamicAnimationKey key;
		key.time = 0.0f;
		key.value = MakeDefaultDynamicValue(type);
		key.interpolation = IsDynamicValueInterpolatable(type)
			? DynamicInterpolation::Linear
			: DynamicInterpolation::Step;
		return key;
	}

	void ResetTrackValues(DynamicAnimationTrack& track)
	{
		if (track.keys.empty())
		{
			track.keys.push_back(MakeDefaultKey(track.valueType));
			return;
		}

		for (DynamicAnimationKey& key : track.keys)
		{
			key.value = MakeDefaultDynamicValue(track.valueType);
			key.interpolation = IsDynamicValueInterpolatable(track.valueType)
				? DynamicInterpolation::Linear
				: DynamicInterpolation::Step;
		}
	}

	std::string MakeTrackLabel(
		const DynamicAnimationTrack& track,
		int index) const
	{
		std::string label = "Track " + std::to_string(index) + ": ";
		if (track.target == DynamicAnimationTarget::WidgetProperty)
		{
			label += "Widget / ";
			label += ToString(track.widgetProperty);
		}
		else
		{
			label += "ShaderParam / ";
			label += track.shaderParamName.empty()
				? "(unnamed)"
				: track.shaderParamName;
			label += " / ";
			label += ToString(track.valueType);
		}
		return label;
	}

	void NewClip()
	{
		clip = {};
		currentFilePath[0] = '\0';
		statusMessage = "Created a new clip.";
	}

	void Save()
	{
		if (currentFilePath[0] == '\0')
		{
			SaveAs();
			return;
		}

		SaveToPath(currentFilePath);
	}

	void SaveAs()
	{
		char path[MAX_PATH] = {};
		if (currentFilePath[0] != '\0')
		{
			strcpy_s(path, currentFilePath);
		}

		if (Dialog::SaveFileName(
			path,
			MAX_PATH,
			"Dynamic Animation Clip\0*.danim\0All Files\0*.*\0\0",
			"Save Dynamic Animation Clip",
			"danim") != DialogResult::OK)
		{
			return;
		}

		strcpy_s(currentFilePath, path);
		SaveToPath(currentFilePath);
	}

	void Load()
	{
		char path[MAX_PATH] = {};
		if (Dialog::OpenFileName(
			path,
			MAX_PATH,
			"Dynamic Animation Clip\0*.danim\0All Files\0*.*\0\0",
			"Open Dynamic Animation Clip") != DialogResult::OK)
		{
			return;
		}

		DynamicAnimationClip loaded;
		std::string error;
		if (!DynamicAnimationSerializer::Load(path, loaded, &error))
		{
			statusMessage = "Load failed: " + error;
			return;
		}

		clip = std::move(loaded);
		strcpy_s(currentFilePath, path);
		statusMessage = "Loaded the clip.";
	}

	void SaveToPath(const std::string& path)
	{
		std::string error;
		if (!DynamicAnimationSerializer::Save(clip, path, &error))
		{
			statusMessage = "Save failed: " + error;
			return;
		}

		statusMessage = "Saved the clip.";
	}

	void ClampAllKeyTimes()
	{
		for (DynamicAnimationTrack& track : clip.tracks)
		{
			for (DynamicAnimationKey& key : track.keys)
			{
				key.time = std::clamp(key.time, 0.0f, clip.length);
			}
		}
	}

private:
	DynamicAnimationClip clip;
	char currentFilePath[MAX_PATH] = {};
	std::string statusMessage;
};
