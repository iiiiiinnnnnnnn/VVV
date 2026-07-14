// DynamicAnimationSerializer.cpp

#include "Application/Tools/DynamicAnimationSerializer.h"

#include "nlohmann/json.hpp"

#include <fstream>

using json = nlohmann::json;

namespace
{
	json SerializeValue(const ParamValue& value)
	{
		if (const bool* v = std::get_if<bool>(&value))
			return *v;
		if (const short* v = std::get_if<short>(&value))
			return *v;
		if (const int* v = std::get_if<int>(&value))
			return *v;
		if (const float* v = std::get_if<float>(&value))
			return *v;
		if (const Color* v = std::get_if<Color>(&value))
			return json::array({ v->x, v->y, v->z, v->w });
		if (const Vector2* v = std::get_if<Vector2>(&value))
			return json::array({ v->x, v->y });
		if (const Vector3* v = std::get_if<Vector3>(&value))
			return json::array({ v->x, v->y, v->z });

		const Vector4& v = std::get<Vector4>(value);
		return json::array({ v.x, v.y, v.z, v.w });
	}

	bool DeserializeValue(
		const json& source,
		DynamicValueType type,
		ParamValue& value)
	{
		try
		{
			switch (type)
			{
			case DynamicValueType::Bool:
				value = source.get<bool>();
				return true;

			case DynamicValueType::Short:
				value = static_cast<short>(source.get<int>());
				return true;

			case DynamicValueType::Int:
				value = source.get<int>();
				return true;

			case DynamicValueType::Float:
				value = source.get<float>();
				return true;

			case DynamicValueType::Color:
				if (!source.is_array() || source.size() != 4)
					return false;
				value = Color(
					source[0].get<float>(),
					source[1].get<float>(),
					source[2].get<float>(),
					source[3].get<float>());
				return true;

			case DynamicValueType::Vector2:
				if (!source.is_array() || source.size() != 2)
					return false;
				value = Vector2(
					source[0].get<float>(),
					source[1].get<float>());
				return true;

			case DynamicValueType::Vector3:
				if (!source.is_array() || source.size() != 3)
					return false;
				value = Vector3(
					source[0].get<float>(),
					source[1].get<float>(),
					source[2].get<float>());
				return true;

			case DynamicValueType::Vector4:
				if (!source.is_array() || source.size() != 4)
					return false;
				value = Vector4(
					source[0].get<float>(),
					source[1].get<float>(),
					source[2].get<float>(),
					source[3].get<float>());
				return true;
			}
		}
		catch (...)
		{
			return false;
		}

		return false;
	}

	void SetError(std::string* output, const std::string& message)
	{
		if (output)
		{
			*output = message;
		}
	}
}

bool DynamicAnimationSerializer::Save(
	const DynamicAnimationClip& clip,
	const std::string& path,
	std::string* errorMessage)
{
	json root;
	root["name"] = clip.name;
	root["length"] = clip.length;
	root["tracks"] = json::array();

	for (const DynamicAnimationTrack& track : clip.tracks)
	{
		json trackJson;
		trackJson["target"] = ToString(track.target);
		trackJson["widgetProperty"] = ToString(track.widgetProperty);
		trackJson["shaderParamName"] = track.shaderParamName;
		trackJson["valueType"] = ToString(track.valueType);
		trackJson["keys"] = json::array();

		for (const DynamicAnimationKey& key : track.keys)
		{
			json keyJson;
			keyJson["time"] = key.time;
			keyJson["value"] = SerializeValue(key.value);
			keyJson["interpolation"] = ToString(key.interpolation);
			trackJson["keys"].push_back(keyJson);
		}

		root["tracks"].push_back(trackJson);
	}

	std::ofstream stream(path);
	if (!stream)
	{
		SetError(errorMessage, "Failed to open the output file.");
		return false;
	}

	stream << root.dump(4);
	if (!stream.good())
	{
		SetError(errorMessage, "Failed while writing the file.");
		return false;
	}

	if (errorMessage)
		errorMessage->clear();
	return true;
}

bool DynamicAnimationSerializer::Load(
	const std::string& path,
	DynamicAnimationClip& clip,
	std::string* errorMessage)
{
	std::ifstream stream(path);
	if (!stream)
	{
		SetError(errorMessage, "Failed to open the input file.");
		return false;
	}

	json root;
	try
	{
		stream >> root;
	}
	catch (const std::exception& exception)
	{
		SetError(
			errorMessage,
			std::string("Failed to parse the file: ") + exception.what());
		return false;
	}

	DynamicAnimationClip loaded;

	try
	{
		loaded.name = root.value("name", std::string("Dynamic Animation"));
		loaded.length = std::max(root.value("length", 1.0f), 0.0f);

		if (root.contains("tracks"))
		{
			for (const json& trackJson : root["tracks"])
			{
				DynamicAnimationTrack track;

				DynamicAnimationTarget target;
				if (!TryParseDynamicAnimationTarget(
					trackJson.value("target", std::string("WidgetProperty")),
					target))
				{
					SetError(errorMessage, "The file contains an invalid target.");
					return false;
				}
				track.target = target;

				DynamicWidgetProperty widgetProperty;
				if (!TryParseDynamicWidgetProperty(
					trackJson.value("widgetProperty", std::string("Position")),
					widgetProperty))
				{
					SetError(errorMessage, "The file contains an invalid widget property.");
					return false;
				}
				track.widgetProperty = widgetProperty;

				track.shaderParamName =
					trackJson.value("shaderParamName", std::string("color"));

				DynamicValueType valueType;
				if (!TryParseDynamicValueType(
					trackJson.value("valueType", std::string("Float")),
					valueType))
				{
					SetError(errorMessage, "The file contains an invalid value type.");
					return false;
				}
				track.valueType = valueType;

				if (track.target == DynamicAnimationTarget::WidgetProperty)
				{
					track.valueType = GetWidgetPropertyValueType(track.widgetProperty);
				}

				if (trackJson.contains("keys"))
				{
					for (const json& keyJson : trackJson["keys"])
					{
						DynamicAnimationKey key;
						key.time = keyJson.value("time", 0.0f);

						DynamicInterpolation interpolation;
						if (!TryParseDynamicInterpolation(
							keyJson.value("interpolation", std::string("Linear")),
							interpolation))
						{
							interpolation = DynamicInterpolation::Linear;
						}
						key.interpolation = interpolation;

						if (!keyJson.contains("value") ||
							!DeserializeValue(
								keyJson["value"],
								track.valueType,
								key.value))
						{
							SetError(errorMessage, "The file contains an invalid key value.");
							return false;
						}

						if (!IsDynamicValueInterpolatable(track.valueType))
						{
							key.interpolation = DynamicInterpolation::Step;
						}

						track.keys.push_back(key);
					}
				}

				std::sort(
					track.keys.begin(),
					track.keys.end(),
					[](const DynamicAnimationKey& a, const DynamicAnimationKey& b)
					{
						return a.time < b.time;
					});

				loaded.tracks.push_back(track);
			}
		}
	}
	catch (const std::exception& exception)
	{
		SetError(
			errorMessage,
			std::string("The file structure is invalid: ") + exception.what());
		return false;
	}

	clip = std::move(loaded);
	if (errorMessage)
		errorMessage->clear();
	return true;
}
