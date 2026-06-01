// Commander.h

#pragma once

#include "Character.h"
#include "Squad.h"

class Commander : public Character
{
public:
	Commander();
	~Commander() = default;
	void OnUpdate() override;
	void OnLateUpdate() override;
	void OnRender(const RenderContext& rc) override;
	void OnDrawGUI() override;
private:
	std::unique_ptr<Squad> squad;
};
