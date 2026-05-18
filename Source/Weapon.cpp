// Weapon.cpp

#include "Weapon.h"
#include <Graphics.h>
#include <Input.h>
#include "Model.h"
#include "Character.h"
#include "Texture.h"

Weapon::Weapon(Character* character) : Actor("Weapon", "Weapon", "Default"), character(character)
{
	// 武器
	std::shared_ptr<Model> model = std::make_shared<Model>(
		"Data/Model/ToonSoldiers_WW2/models/weapons/weapon_arisaka.glb");

	// テクスチャ読み込み
	Texture::LoadTexture(
		Graphics::Instance().GetDevice(),
		"Data/Model/ToonSoldiers_WW2/models/textures/TS_WW2_weapons.tga",
		model->GetMaterials()[0].baseMap.GetAddressOf());

	// モデルレンダラー生成
	auto mdlRender = AddComponent<ModelRender>(model);
	
	// プレイヤーの手に武器を接続
	mdlRender->AppendNode(character->GetHandNode());

	// 武器の初期回転オフセット
	transform.rotation = Quaternion::CreateFromYawPitchRoll(0, DirectX::XMConvertToRadians(90), 0);
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
