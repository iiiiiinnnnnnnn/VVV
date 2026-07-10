// Stage01.h

#pragma once

#include "Actor.h"

class Stage01 : public Actor
{
public:
	Stage01(ActorManager* actorManager);
	void ApplyEnvironment(class LightManager& lightManager) const;
	void SetCrystalBreakParticleSystem(class ParticleSystem* particleSystem);
	void OnUpdate() override;
	void OnDrawGUI() override;

private:
	float aracoreSpawnTimer = -1.0f;
};
