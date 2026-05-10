// Commander.cpp

#include "Commander.h"

Commander::Commander() : Character("Commander", "Commander", "Default")
{
	// プレイヤー
	model = std::make_shared<Model>(
		"Data/Model/ToonSoldiers_WW2/models/ToonSoldier_WW2_Japan_Soldier.glb");

	// キャラクター初期化
	InitCharacter();

	// 指揮官：歩兵体＋将校帽＋頭
	SetSkin((uint32_t)SkinParts::Head_Officer);
}

void Commander::OnUpdate(float elapsedTime)
{
	Character::OnUpdate(elapsedTime);
}

void Commander::OnRender(const RenderContext& rc, float elapsedTime)
{
	Character::OnRender(rc, elapsedTime);
}
