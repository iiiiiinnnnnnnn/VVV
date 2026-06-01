// Commander.cpp

#include "Commander.h"

Commander::Commander() : Character("Commander", "Commander", true, "Default")
{
}

void Commander::OnUpdate()
{
	Character::OnUpdate();
}

void Commander::OnLateUpdate()
{
	Character::OnLateUpdate();
}

void Commander::OnRender(const RenderContext& rc)
{
	Character::OnRender(rc);
}

void Commander::OnDrawGUI()
{
	Character::OnDrawGUI();
}
