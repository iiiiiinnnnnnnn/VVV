// Terrain.cpp

#include "Terrain.h"
#include "Actor.h"

Terrain::Terrain(Object* owner) : Component(owner)
{
	// ƒGƒ‰[—p
	Component::GetOwnerAsActor();
}

Terrain::~Terrain()
{
}

void Terrain::Update()
{
}

void Terrain::Render(const RenderContext& rc)
{
}

void Terrain::DrawGUI()
{
}
