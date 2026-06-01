// Weapon.cpp

#include "Weapon.h"
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
	auto mdlRender = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);
	
	// プレイヤーの手に武器を接続
	mdlRender->AppendNode(owner->GetHandNode());

	transform = Transform::FromAngle({ DirectX::XMConvertToRadians(90), 0, 0});
}

void Weapon::OnUpdate()
{

}

void Weapon::OnLateUpdate()
{

}

void Weapon::OnRender(const RenderContext& rc)
{

}

void Weapon::OnDrawGUI()
{

}

std::string Weapon::GetModel(WeaponType type)
{
	switch (type)
	{
		case WeaponType::Colt1911: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_colt1911.glb";
		case WeaponType::Luger: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_luger.glb";
		case WeaponType::Nambu: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_nambu.glb";
		case WeaponType::Tokarev: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_tokarev.glb";
		case WeaponType::Webley: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_webley.glb";
		case WeaponType::Greasgun: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_greasegun.glb";
		case WeaponType::Mp40: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_mp40.glb";
		case WeaponType::Ppsh41: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_ppsh41.glb";
		case WeaponType::Sten: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_sten.glb";
		case WeaponType::Thompson: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_thompson.glb";
		case WeaponType::Type100: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_type100.glb";
		case WeaponType::Kar98k: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_98k.glb";
		case WeaponType::Arisaka: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_arisaka.glb";
		case WeaponType::G43: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_g43.glb";
		case WeaponType::GarandM1: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_garandM1.glb";
		case WeaponType::MosinNagant: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_mosin_nagant.glb";
		case WeaponType::M1Carbine: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_m1carbine.glb";
		case WeaponType::Springfield: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_springfield.glb";
		case WeaponType::Svt40: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_svt40.glb";
		case WeaponType::Bar: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_bar.glb";
		case WeaponType::Bren: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_bren.glb";
		case WeaponType::Dp28: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_dp28.glb";
		case WeaponType::Fg42: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_fg42.glb";
		case WeaponType::Stg44: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_stg44.glb";
		case WeaponType::Type99: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_type99.glb";
		case WeaponType::Cal30: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_30cal.glb";
		case WeaponType::Mg42: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_mg42.glb";
		case WeaponType::Bazooka: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_bazooka.glb";
		case WeaponType::Panzerschreck: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_panzerschreck.glb";
		case WeaponType::Piat: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_piat.glb";
		case WeaponType::Flamethrower: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_flamethrower.glb";
		case WeaponType::Knife: return "Data/Model/ToonSoldiers_WW2/models/weapons/weapon_knife.glb";
		default: return "";
	}
}
