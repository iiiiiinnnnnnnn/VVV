// Commander.cpp

#include "Commander.h"

Commander::Commander() : Character("Commander", "Commander", true, "Default", Country::Japan, SkinParts::Head_Officer)
{
}

void Commander::OnUpdate(float elapsedTime)
{
	Character::OnUpdate(elapsedTime);
}

void Commander::OnLateUpdate(float elapsedTime)
{
	Character::OnLateUpdate(elapsedTime);
}

void Commander::OnRender(const RenderContext& rc, float elapsedTime)
{
	Character::OnRender(rc, elapsedTime);
}

void Commander::OnDrawGUI(float elapsedTime)
{
	Character::OnDrawGUI(elapsedTime);
}
