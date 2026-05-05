#pragma once

#include "Camera.h"
#include "RenderState.h"
#include "Light.h"

struct RenderSettings
{
	bool showDebug = false;
	bool wireframe = false;
};

struct RenderContext
{
	ID3D11DeviceContext*	deviceContext;
	const RenderState*		renderState = nullptr;
	const Camera*			camera = nullptr;
	const LightManager*		lightManager = nullptr;
	const RenderSettings*	renderSettings = nullptr;
};
