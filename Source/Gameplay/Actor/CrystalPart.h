#pragma once

#include "Application/Time/Timer.h"
#include "Gameplay/Actor/Actor.h"
#include <memory>

class VMDL;
class VMDLModel;
class VMDLColliderComponent;

class CrystalPart : public Actor
{
public:
	CrystalPart(const Transform& placement, std::shared_ptr<VMDLModel> model);
	~CrystalPart() override;

	/// <summary>
	/// 指定したワールド位置に破片を1個生成し、初速度と回転速度を設定する。
	/// </summary>
	/// <param name="position">生成位置（ワールド座標）。</param>
	/// <param name="velocity">初速度（ワールド座標系、距離/秒）。方向に速さを掛けたベクトルを指定する。</param>
	/// <param name="size">破片の最大辺の長さ（ワールド単位）。正の値を指定する。</param>
	/// <param name="rotationSpeed">各軸の初期角速度（ラジアン/秒）。</param>
	/// <param name="source">生成元。生成元との接触では破片を消滅させない。省略可能。</param>
	/// <returns>生成した破片。生成できなかった場合は空のshared_ptr。</returns>
	static std::shared_ptr<CrystalPart> Spawn(const Vector3& position, const Vector3& velocity,
		float size = 0.6f, const Vector3& rotationSpeed = {0.9f, 1.2f, 0.6f}, Actor* source = nullptr);

	/// <summary>
	/// 中心の周囲に破片を等間隔で配置し、XZ平面の放射状に飛ばす。
	/// 各破片の初速度はvelocityに外向きの速度を加算した値になる。
	/// </summary>
	/// <param name="position">放射状に配置する中心位置（ワールド座標）。</param>
	/// <param name="velocity">全破片に共通の初速度（ワールド座標系）。上向きの速度などを指定する。</param>
	/// <param name="count">生成数。0以下の場合は生成しない。</param>
	/// <param name="size">各破片の最大辺の長さ（ワールド単位）。</param>
	/// <param name="outwardSpeed">中心から外向きに加算する速さ（距離/秒）。</param>
	/// <param name="radius">中心から各破片の生成位置までの水平距離（ワールド単位）。</param>
	/// <param name="source">生成元。生成元との接触では破片を消滅させない。省略可能。</param>
	static void Spawn(const Vector3& position, const Vector3& velocity, int count,
		float size, float outwardSpeed, float radius, Actor* source = nullptr);

	/// <summary>
	/// ローカル座標の範囲を各軸最大2分割し、各セルの中心に破片を生成する（最大8個）。
	/// 配置にはtransformを適用し、中心から水平方向に飛ばす。上下のセルで上向き速度を調整する。
	/// </summary>
	/// <param name="transform">ローカル座標をワールド座標へ変換する位置・回転・スケール。</param>
	/// <param name="center">分割する範囲の中心（ローカル座標）。ワールド位置は指定しない。</param>
	/// <param name="size">分割する範囲の各軸の長さ（ローカル座標系）。破片自体の大きさではない。</param>
	/// <param name="velocity">基準の初速度（ワールド座標系）。Y成分はセルの高さに応じて0.8～1.2倍の範囲で調整する。</param>
	/// <param name="outwardSpeed">範囲の中心から水平外向きに加算する速さ（距離/秒）。</param>
	/// <param name="fragmentSize">各破片の最大辺の長さ（ワールド単位）。</param>
	/// <param name="source">生成元。生成元との接触では破片を消滅させない。省略可能。</param>
	static void Spawn(const Transform& transform, const Vector3& center, const Vector3& size,
		const Vector3& velocity, float outwardSpeed = 4.0f, float fragmentSize = 0.6f,
		Actor* source = nullptr);

private:
	void OnAwake() override;
	void OnUpdate() override;
	void OnCollisionEnter(PhysicsComponent* self, PhysicsComponent* other,
		const Vector3& point, const Vector3& normal) override;
	void StartMoving(const Vector3& velocity, const Vector3& rotationSpeed, Actor* source);

	std::shared_ptr<VMDLModel> model;
	VMDL* vmdl = nullptr;
	VMDLColliderComponent* body = nullptr;
	Vector3 initialScale;
	std::weak_ptr<Actor> sourceActor;
	Game::Timer lifetimeTimer{6.0f};
	bool contactPending = false;
	bool shrinking = false;
};
