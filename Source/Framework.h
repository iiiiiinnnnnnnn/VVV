#pragma once

#include "Common.h"
#include "HighResolutionTimer.h"
#include "Scene.h"

class Framework
{
public:
	Framework(HWND hWnd);
	~Framework();

private:
	void Update(float elapsedTime);
	void Render(float elapsedTime);

	void CalculateFrameStats();

	void SetVSyncEnabled(bool enabled) { vsyncEnabled = enabled; }

public:
	int Run();
	LRESULT CALLBACK HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	const HWND				hWnd;
	HighResolutionTimer		timer;
	std::unique_ptr<Scene>	scene;

	bool vsyncEnabled = true;
};

