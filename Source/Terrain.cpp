// Terrain.cpp

#include "Terrain.h"
#include "Actor.h"

Terrain::Terrain(Object* owner) : Component(owner)
{
	// ÉGÉâÅ[óp
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
	if (ImGui::TreeNode("Terrain"))
	{
		if (ImGui::Button("terrain texture clear"))
			is_terrain_texture_clear_color = true;
		ImGui::DragFloat4("terrain texture clear color", &terrain_texture_clear_color.x);
		ImGui::Separator();
		ImGui::Checkbox("use brush", &use_brush);
		ImGui::SliderInt("brush size", &brush_size, 0, 100);
		ImGui::ColorEdit3("brush color", &brush_color.x);
		ImGui::SliderFloat("brush intensity", &brush_intensity, -10, +10);
		ImGui::SliderFloat("brush blend rate", &brush_color.w, 0, +1);

		ImGui::Checkbox("wire", &use_wire);
		ImGui::SliderFloat("edge", &tesselation_constant.edge_factor, 0.0f, +64.0f);
		ImGui::SliderFloat("inner", &tesselation_constant.inner_factor, 0.0f, +64.0f);
		ImGui::SliderFloat("height scaler", &tesselation_constant.height_scaler, -5.0f, +5.0f);
		ImGui::SliderFloat("tilling scale", &tesselation_constant.tilling_scale, 0.01f, +10.0f);

		ImGui::Separator();
		ImGui::TreePop();
	}
}
