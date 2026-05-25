// Commander.h

#pragma once

#include "Character.h"
#include "Squad.h"

class Commander : public Character
{
public:
	Commander();
	~Commander() = default;
	void OnUpdate(float elapsedTime) override;
	void OnLateUpdate(float elapsedTime) override;
	void OnRender(const RenderContext& rc, float elapsedTime) override;
	void OnDrawGUI(float elapsedTime) override;
private:
	std::unique_ptr<Squad> squad;
};
