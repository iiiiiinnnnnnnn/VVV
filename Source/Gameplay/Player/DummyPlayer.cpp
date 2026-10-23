#include "Gameplay/Player/DummyPlayer.h"

DummyPlayer::DummyPlayer() : Entity("DummyPlayer", "DummyPlayer", true, Transform(), 100.0f, 100.0f)
{}

void DummyPlayer::OnInit()
{
	transform.SetRotation(Quaternion::Identity);

	vmdl = AddComponent<VMDL>("Player/CombatGirls_Sword_Shield");
	vmdl->SetModelYawOffset(RAD(180.0f));

	anim = AddComponent<Animator>(vmdl->GetSharedModel());
	vmdl->BuildFootIK(anim);

	anim->Load("DummyPlayer");
	//anim->Load("Player");
}

DummyPlayer::~DummyPlayer()
{
	Uninit();
}

void DummyPlayer::OnUninit()
{
	vmdl = nullptr;
	anim = nullptr;
}
