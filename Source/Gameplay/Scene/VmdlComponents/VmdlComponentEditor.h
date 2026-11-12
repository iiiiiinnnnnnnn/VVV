#pragma once

#include "Resource/VMDLModel.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>
#include <ImGuiToolKit/Color.h>
#include <imgui_stdlib.h>
#include <format>

class VmdlEditorScene;

class VmdlComponentEditor
{
public:
	static constexpr float ItemWidth = 240.0f;
	static std::string MakeMeshLabel(const VMDLModel& model, int meshIndex);

protected:
	template<typename T>
	static int Count(const T& values) { return static_cast<int>(values.size()); }
	template<typename T, size_t Size>
	static constexpr int Count(const T (&)[Size]) { return static_cast<int>(Size); }
	static bool DrawTransform(VMDLModel& model, VMDLModel::VmdlComponentTransform& transform);
	static void DrawDistanceMode(bool& attenuate);
};

class VmdlRigidBodyEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlColliderEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlSpringEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlSpringColliderEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlTrailEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlParticleEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlSoundSourceEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlPointLightEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlCameraShakeEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};

class VmdlRadialBlurEditor final : public VmdlComponentEditor
{
public:
	static bool DrawGUI(VmdlEditorScene& scene, int nodeIndex, bool openSelectedComponent);
};
