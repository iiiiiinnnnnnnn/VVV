// Light.h

#pragma once

#include "Object.h"
#include "Transform.h"

class LightManager;

class Light : public Object
{
public:
	Light(std::string name = "Light", bool isActive = true, Color color = {1, 1, 1, 1})
		: Object(name, isActive), color(color) {}
	virtual ~Light() = default;

	void Update() override
	{
		transform.Update();
		Object::Update();
	}

	virtual void DrawGUI() override;

	Transform transform;

	void SetColor(const Color& color)
	{
		this->color = color;
	}

	const Color& GetColor() const
	{
		return color;
	}

	const Vector3 GetDirection() const
	{
		return transform.forward;
	}

protected:
	friend class LightManager;
	Color color = { 1, 1, 1, 1 };
	LightManager* lightManager = nullptr;
};

class DirectionalLight : public Light
{
public:
	DirectionalLight(std::string name = "Directional Light", bool isActive = true, Color color = {1, 1, 1, 1})
		: Light(name, isActive, color)
	{
	}

	void DrawGUI() override;
};

class PointLight : public Light
{
public:
	PointLight(std::string name = "Point Light", bool isActive = true, Color color = {1, 1, 1, 1}, float range = 10.0f)
		: Light(name, isActive, color), range(range)
	{
	}

	void DrawGUI() override;

	void SetRange(float range)
	{
		this->range = range;
	}

	float GetRange() const
	{
		return range;
	}

protected:
	float range;
};

class SpotLight : public Light
{
public:
	SpotLight(std::string name = "Spot Light", bool isActive = true, Color color = {1, 1, 1, 1}, float range = 10.0f, float innerConeAngle = 0.9f, float outerConeAngle = 0.8f)
		: Light(name, isActive, color), range(range), innerConeAngle(innerConeAngle), outerConeAngle(outerConeAngle)
	{
	}

	void DrawGUI() override;

	void SetRange(float range)
	{
		this->range = range;
	}

	void SetInnerConeAngle(float angle)
	{
		this->innerConeAngle = angle;
	}

	void SetOuterConeAngle(float angle)
	{
		this->outerConeAngle = angle;
	}

	float GetRange() const
	{
		return range;
	}

	float GetInnerConeAngle() const
	{
		return innerConeAngle;
	}

	float GetOuterConeAngle() const
	{
		return outerConeAngle;
	}

protected:
	float range;
	float innerConeAngle;
	float outerConeAngle;
};

class AreaLight : public Light
{
public:
	AreaLight(std::string name = "Area Light", bool isActive = true, Color color = {1, 1, 1, 1}, float width = 1.0f, float height = 1.0f, float range = 10.0f)
		: Light(name, isActive, color), width(width), height(height), range(range)
	{
	}

	void DrawGUI() override;

	void SetRange(float range)
	{
		this->range = range;
	}

	void SetWidth(float width)
	{
		this->width = width;
	}

	void SetHeight(float height)
	{
		this->height = height;
	}

	float GetRange() const
	{
		return range;
	}

	float GetWidth() const
	{
		return width;
	}

	float GetHeight() const
	{
		return height;
	}

protected:
	float range;
	float width;
	float height;
};