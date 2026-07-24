// DynamicAnimation.h

#pragma once

#include "Core/Foundation/Common.h"

#include <string>
#include <variant>
#include <vector>

using DynamicValue = std::variant<bool, short, int, float, Color, Vector2, Vector3, Vector4>;

enum class DynamicAnimationTarget
{
	WidgetProperty,
	SpriteColor
};

enum class DynamicWidgetProperty
{
	Position,
	Angle,
	Size,
	Anchor
};

enum class DynamicInterpolation
{
	Step,
	Linear,
	EaseIn,
	EaseOut,
	EaseInOut
};

enum class DynamicValueType
{
	Bool,
	Short,
	Int,
	Float,
	Color,
	Vector2,
	Vector3,
	Vector4
};

struct DynamicAnimationKey
{
	float time = 0.0f;
	DynamicValue value = 0.0f;

	// This interpolation mode is used from this key to the next key.
	DynamicInterpolation interpolation = DynamicInterpolation::Linear;
};

struct DynamicAnimationTrack
{
	DynamicAnimationTarget target = DynamicAnimationTarget::WidgetProperty;
	DynamicWidgetProperty widgetProperty = DynamicWidgetProperty::Position;

	DynamicValueType valueType = DynamicValueType::Vector2;

	std::vector<DynamicAnimationKey> keys;
};

struct DynamicAnimationClip
{
	std::string name = "New Dynamic Animation";
	float length = 1.0f;
	std::vector<DynamicAnimationTrack> tracks;
};

const char* ToString(DynamicAnimationTarget value);
const char* ToString(DynamicWidgetProperty value);
const char* ToString(DynamicInterpolation value);
const char* ToString(DynamicValueType value);

bool TryParseDynamicAnimationTarget(
	const std::string& text,
	DynamicAnimationTarget& value);

bool TryParseDynamicWidgetProperty(
	const std::string& text,
	DynamicWidgetProperty& value);

bool TryParseDynamicInterpolation(
	const std::string& text,
	DynamicInterpolation& value);

bool TryParseDynamicValueType(
	const std::string& text,
	DynamicValueType& value);

DynamicValueType GetDynamicValueType(const DynamicValue& value);
DynamicValueType GetWidgetPropertyValueType(DynamicWidgetProperty property);
DynamicValue MakeDefaultDynamicValue(DynamicValueType type);
bool IsDynamicValueInterpolatable(DynamicValueType type);

DynamicValue EvaluateDynamicAnimationTrack(
	const DynamicAnimationTrack& track,
	float time);

DynamicValue BlendDynamicAnimationValue(
	const DynamicValue& from,
	const DynamicValue& to,
	float t);
