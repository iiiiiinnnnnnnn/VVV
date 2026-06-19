// Prop.h

// ‚½‚¾‚Ì’u‚­‹¤’Êƒ‚ƒfƒ‹

#pragma once

#include "Actor.h"

class Prop : public Actor
{
public:
	Prop(const std::filesystem::path& path, const Transform& transform = {}, bool isDynamic = false, int meshColliderConvex = 1280, int animationIndex = -1, const std::string& tag = "Prop", bool isActive = true, int layer = Layer::Default);
	Prop(std::shared_ptr<Model> model, const Transform& transform = {}, bool isDynamic = false, int meshColliderConvex = 1280, int animationIndex = -1, const std::string& tag = "Prop", bool isActive = true, int layer = Layer::Default);
	~Prop() = default;
private:
	void Build(std::shared_ptr<Model> model, const Transform& transform, bool isDynamic, int meshColliderConvex, int animationIndex, const std::string& tag, bool isActive, int layer);
};
