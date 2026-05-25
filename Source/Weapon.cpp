// Weapon.cpp

#include "Weapon.h"
#include "Graphics.h"
#include "Model.h"
#include "Character.h"
#include "Texture.h"
#include "ResourceManager.h"

Weapon::Weapon(Character* owner)
	: Actor("Weapon", "Weapon", "Default"), owner(owner)
{
	// 武器
	std::shared_ptr<Model> model = ResourceManager::Instance().LoadModel(
		"Data/Model/ToonSoldiers_WW2/models/weapons/weapon_arisaka.glb");

	// テクスチャ読み込み
	std::shared_ptr<Texture> texture = ResourceManager::Instance().LoadTexture(
		"Data/Model/ToonSoldiers_WW2/models/textures/TS_WW2_weapons.tga");
	model->GetMaterials()[0].baseMap = texture->GetShaderResourceView();

	// モデルレンダラー生成
	auto mdlRender = AddComponent<ModelRenderComponent>(model);
	
	// プレイヤーの手に武器を接続
	mdlRender->AppendNode(owner->GetHandNode());

	transform = Transform::FromAngle({ DirectX::XMConvertToRadians(90), 0, 0});
}

void Weapon::OnUpdate(float elapsedTime)
{

}

void Weapon::OnLateUpdate(float elapsedTime)
{

}

void Weapon::OnRender(const RenderContext& rc, float elapsedTime)
{

}

void Weapon::OnDrawGUI(float elapsedTime)
{

}
