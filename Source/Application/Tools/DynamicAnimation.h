// DynamicAnimation.h

#pragma once

#include "Rendering/Core/ShaderParam.h"

#include <string>
#include <vector>

enum class DynamicAnimationTarget
{
	WidgetProperty,
	ShaderParam
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
	ParamValue value = 0.0f;

	// This interpolation mode is used from this key to the next key.
	DynamicInterpolation interpolation = DynamicInterpolation::Linear;
};

struct DynamicAnimationTrack
{
	DynamicAnimationTarget target = DynamicAnimationTarget::WidgetProperty;
	DynamicWidgetProperty widgetProperty = DynamicWidgetProperty::Position;

	// Used only when target is ShaderParam.
	std::string shaderParamName = "color";
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

DynamicValueType GetDynamicValueType(const ParamValue& value);
DynamicValueType GetWidgetPropertyValueType(DynamicWidgetProperty property);
ParamValue MakeDefaultDynamicValue(DynamicValueType type);
bool IsDynamicValueInterpolatable(DynamicValueType type);

ParamValue EvaluateDynamicAnimationTrack(
	const DynamicAnimationTrack& track,
	float time);

ParamValue BlendDynamicAnimationValue(
	const ParamValue& from,
	const ParamValue& to,
	float t);
