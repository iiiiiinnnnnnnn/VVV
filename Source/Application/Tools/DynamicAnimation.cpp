// DynamicAnimation.cpp

#include "Application/Tools/DynamicAnimation.h"

#include <algorithm>
#include <cmath>

namespace
{
	float ApplyInterpolation(
		float t,
		DynamicInterpolation interpolation)
	{
		t = std::clamp(t, 0.0f, 1.0f);

		switch (interpolation)
		{
		case DynamicInterpolation::Step:
			return 0.0f;

		case DynamicInterpolation::Linear:
			return t;

		case DynamicInterpolation::EaseIn:
			return t * t;

		case DynamicInterpolation::EaseOut:
			return 1.0f - (1.0f - t) * (1.0f - t);

		case DynamicInterpolation::EaseInOut:
			return t * t * (3.0f - 2.0f * t);
		}

		return t;
	}

	DynamicValue InterpolateValue(
		const DynamicValue& from,
		const DynamicValue& to,
		float t)
	{
		if (from.index() != to.index())
		{
			return from;
		}

		if (const float* a = std::get_if<float>(&from))
		{
			const float b = std::get<float>(to);
			return *a + (b - *a) * t;
		}

		if (const Color* a = std::get_if<Color>(&from))
		{
			const Color& b = std::get<Color>(to);
			return Color(
				a->x + (b.x - a->x) * t,
				a->y + (b.y - a->y) * t,
				a->z + (b.z - a->z) * t,
				a->w + (b.w - a->w) * t);
		}

		if (const Vector2* a = std::get_if<Vector2>(&from))
		{
			const Vector2& b = std::get<Vector2>(to);
			return Vector2::Lerp(*a, b, t);
		}

		if (const Vector3* a = std::get_if<Vector3>(&from))
		{
			const Vector3& b = std::get<Vector3>(to);
			return Vector3::Lerp(*a, b, t);
		}

		if (const Vector4* a = std::get_if<Vector4>(&from))
		{
			const Vector4& b = std::get<Vector4>(to);
			return Vector4::Lerp(*a, b, t);
		}

		// bool, short and int are discrete values.
		return from;
	}
}

const char* ToString(DynamicAnimationTarget value)
{
	switch (value)
	{
	case DynamicAnimationTarget::WidgetProperty:
		return "WidgetProperty";
	case DynamicAnimationTarget::SpriteColor:
		return "SpriteColor";
	}
	return "WidgetProperty";
}

const char* ToString(DynamicWidgetProperty value)
{
	switch (value)
	{
	case DynamicWidgetProperty::Position:
		return "Position";
	case DynamicWidgetProperty::Angle:
		return "Angle";
	case DynamicWidgetProperty::Size:
		return "Size";
	case DynamicWidgetProperty::Anchor:
		return "Anchor";
	}
	return "Position";
}

const char* ToString(DynamicInterpolation value)
{
	switch (value)
	{
	case DynamicInterpolation::Step:
		return "Step";
	case DynamicInterpolation::Linear:
		return "Linear";
	case DynamicInterpolation::EaseIn:
		return "EaseIn";
	case DynamicInterpolation::EaseOut:
		return "EaseOut";
	case DynamicInterpolation::EaseInOut:
		return "EaseInOut";
	}
	return "Linear";
}

const char* ToString(DynamicValueType value)
{
	switch (value)
	{
	case DynamicValueType::Bool:
		return "Bool";
	case DynamicValueType::Short:
		return "Short";
	case DynamicValueType::Int:
		return "Int";
	case DynamicValueType::Float:
		return "Float";
	case DynamicValueType::Color:
		return "Color";
	case DynamicValueType::Vector2:
		return "Vector2";
	case DynamicValueType::Vector3:
		return "Vector3";
	case DynamicValueType::Vector4:
		return "Vector4";
	}
	return "Float";
}

bool TryParseDynamicAnimationTarget(
	const std::string& text,
	DynamicAnimationTarget& value)
{
	if (text == "WidgetProperty")
	{
		value = DynamicAnimationTarget::WidgetProperty;
		return true;
	}
	if (text == "SpriteColor" || text == "ShaderParam")
	{
		value = DynamicAnimationTarget::SpriteColor;
		return true;
	}
	return false;
}

bool TryParseDynamicWidgetProperty(
	const std::string& text,
	DynamicWidgetProperty& value)
{
	if (text == "Position")
	{
		value = DynamicWidgetProperty::Position;
		return true;
	}
	if (text == "Angle")
	{
		value = DynamicWidgetProperty::Angle;
		return true;
	}
	if (text == "Size")
	{
		value = DynamicWidgetProperty::Size;
		return true;
	}
	if (text == "Anchor")
	{
		value = DynamicWidgetProperty::Anchor;
		return true;
	}
	return false;
}

bool TryParseDynamicInterpolation(
	const std::string& text,
	DynamicInterpolation& value)
{
	if (text == "Step")
	{
		value = DynamicInterpolation::Step;
		return true;
	}
	if (text == "Linear")
	{
		value = DynamicInterpolation::Linear;
		return true;
	}
	if (text == "EaseIn")
	{
		value = DynamicInterpolation::EaseIn;
		return true;
	}
	if (text == "EaseOut")
	{
		value = DynamicInterpolation::EaseOut;
		return true;
	}
	if (text == "EaseInOut")
	{
		value = DynamicInterpolation::EaseInOut;
		return true;
	}
	return false;
}

bool TryParseDynamicValueType(
	const std::string& text,
	DynamicValueType& value)
{
	if (text == "Bool")
	{
		value = DynamicValueType::Bool;
		return true;
	}
	if (text == "Short")
	{
		value = DynamicValueType::Short;
		return true;
	}
	if (text == "Int")
	{
		value = DynamicValueType::Int;
		return true;
	}
	if (text == "Float")
	{
		value = DynamicValueType::Float;
		return true;
	}
	if (text == "Color")
	{
		value = DynamicValueType::Color;
		return true;
	}
	if (text == "Vector2")
	{
		value = DynamicValueType::Vector2;
		return true;
	}
	if (text == "Vector3")
	{
		value = DynamicValueType::Vector3;
		return true;
	}
	if (text == "Vector4")
	{
		value = DynamicValueType::Vector4;
		return true;
	}
	return false;
}

DynamicValueType GetDynamicValueType(const DynamicValue& value)
{
	if (std::holds_alternative<bool>(value))
		return DynamicValueType::Bool;
	if (std::holds_alternative<short>(value))
		return DynamicValueType::Short;
	if (std::holds_alternative<int>(value))
		return DynamicValueType::Int;
	if (std::holds_alternative<float>(value))
		return DynamicValueType::Float;
	if (std::holds_alternative<Color>(value))
		return DynamicValueType::Color;
	if (std::holds_alternative<Vector2>(value))
		return DynamicValueType::Vector2;
	if (std::holds_alternative<Vector3>(value))
		return DynamicValueType::Vector3;
	return DynamicValueType::Vector4;
}

DynamicValueType GetWidgetPropertyValueType(DynamicWidgetProperty property)
{
	switch (property)
	{
	case DynamicWidgetProperty::Position:
	case DynamicWidgetProperty::Size:
	case DynamicWidgetProperty::Anchor:
		return DynamicValueType::Vector2;

	case DynamicWidgetProperty::Angle:
		return DynamicValueType::Float;
	}

	return DynamicValueType::Float;
}

DynamicValue MakeDefaultDynamicValue(DynamicValueType type)
{
	switch (type)
	{
	case DynamicValueType::Bool:
		return false;
	case DynamicValueType::Short:
		return static_cast<short>(0);
	case DynamicValueType::Int:
		return 0;
	case DynamicValueType::Float:
		return 0.0f;
	case DynamicValueType::Color:
		return Color(1.0f, 1.0f, 1.0f, 1.0f);
	case DynamicValueType::Vector2:
		return Vector2::Zero;
	case DynamicValueType::Vector3:
		return Vector3::Zero;
	case DynamicValueType::Vector4:
		return Vector4::Zero;
	}

	return 0.0f;
}

bool IsDynamicValueInterpolatable(DynamicValueType type)
{
	return type == DynamicValueType::Float ||
		type == DynamicValueType::Color ||
		type == DynamicValueType::Vector2 ||
		type == DynamicValueType::Vector3 ||
		type == DynamicValueType::Vector4;
}

DynamicValue BlendDynamicAnimationValue(
	const DynamicValue& from,
	const DynamicValue& to,
	float t)
{
	t = std::clamp(t, 0.0f, 1.0f);

	if (from.index() != to.index())
		return t < 0.5f ? from : to;

	const DynamicValueType type = GetDynamicValueType(from);
	if (!IsDynamicValueInterpolatable(type))
		return t < 0.5f ? from : to;

	return InterpolateValue(from, to, t);
}

DynamicValue EvaluateDynamicAnimationTrack(
	const DynamicAnimationTrack& track,
	float time)
{
	if (track.keys.empty())
	{
		return MakeDefaultDynamicValue(track.valueType);
	}

	if (time <= track.keys.front().time)
	{
		return track.keys.front().value;
	}

	if (time >= track.keys.back().time)
	{
		return track.keys.back().value;
	}

	for (std::size_t i = 0; i + 1 < track.keys.size(); ++i)
	{
		const DynamicAnimationKey& from = track.keys[i];
		const DynamicAnimationKey& to = track.keys[i + 1];

		if (time >= to.time)
		{
			continue;
		}

		const float duration = to.time - from.time;
		if (duration <= eps)
		{
			return to.value;
		}

		if (!IsDynamicValueInterpolatable(track.valueType) ||
			from.interpolation == DynamicInterpolation::Step)
		{
			return from.value;
		}

		float t = (time - from.time) / duration;
		t = ApplyInterpolation(t, from.interpolation);
		return InterpolateValue(from.value, to.value, t);
	}

	return track.keys.back().value;
}
