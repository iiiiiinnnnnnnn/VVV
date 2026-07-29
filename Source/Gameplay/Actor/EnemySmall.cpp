// EnemySmall.cpp

#include "EnemySmall.h"


EnemySmall::EnemySmall(const Vector3& position)
{
	vmdl = AddComponent<VMDL>("Data/Model/Enemy/deer");
	anim = vmdl->GetAnimator();
	anim->Load("Data/Animator/deer.animator");

	// キャラクターコントローラー
	cc = AddComponent<CharacterController>(
		Layers::Get("Enemy"), 0.59f, 1.05f);
	cc->SetStepOffset(1.2f);
	cc->SetSlopeLimitDeg(70.0f);
	cc->SetContactOffset(0.1f);

	cc->SetPosition(position);
}
