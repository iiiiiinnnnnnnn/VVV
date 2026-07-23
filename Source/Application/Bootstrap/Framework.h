// Framework.h
#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Core/Foundation/Common.h"
#include "Application/Time/HighResolutionTimer.h"
#include "Gameplay/Scene/Scene.h"

class Framework
{
public:
	Framework(HWND hWnd);
	~Framework();

private:
	void Update(float elapsedTime);
	void Render(float elapsedTime);

	void CalculateFrameStats();

public:
	int Run();
	LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	const HWND				hWnd;
	HighResolutionTimer		timer;
};
