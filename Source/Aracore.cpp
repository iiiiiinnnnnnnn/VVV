// Aracore.cpp

#include "Aracore.h"
#include "DamageHoleComponent.h"
#include "ResourceManager.h"
#include "SceneEffect.h"
#include "ActorManager.h"
#include "NavMeshAgent.h"
#include "AracoreFootGrounder.h"
#include "imgui.h"

Aracore::Aracore() : Entity("Aracore", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f)
{
	// 蜘蛛の部分
    {
        // モデル
        model = ResourceManager::Instance().LoadModel("Data/Model/Spider/animated_spider2.glb");
        shaderParamWithMaterialName =
        {
            {
                "Yeux", // 目
                {
                    {"metalness", 1.0f},
                    {"roughness", 0.0f},
                    {"occlusion", 0.5f},
                    {"occlusionStrength", 1.0f}
                }
            },
            {
                "Spider", // 他
                {
                    {"metalness", 0.0f},
                    {"roughness", 1.0f},
                    {"occlusion", 0.5f},
                    {"occlusionStrength", 1.0f}
                }
            }
        };
		transform.SetScale(0.01f);
        model->UpdateTransform(transform.matrix);
        bodyRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR, shaderParamWithMaterialName);

        // 足接地補正
        //AracoreFootGrounder* footGrounder = AddComponent<AracoreFootGrounder>(model.get());

        // アニメータ
		anim = AddComponent<Animator>(model, 0);
        //anim->Load("Data/Animator/animated_spider.animator");

		// キャラクターコントローラー
		CharacterController* cc = AddComponent<CharacterController>(2.17f, 0.7f);
		cc->SetStepOffset(1.2f);
		cc->SetSlopeLimitDeg(70.0f);
		cc->SetContactOffset(0.2f);
		navMeshAgent = AddComponent<NavMeshAgent>();

		// リジッドボディ
		rb = AddComponent<RigidbodyDynamic>();
		rb->SetKinematic(true);

        // 当たり判定
        //bodyCollider = AddComponent<SphereCollider>(rb, 4.68f, Vector3{0, 3.55f, 0});

        // 足接地補正対象。IK Chainではなくスキニングに効く足ボーンを補正する。
        /*footGrounder->AddLeg("Box09", "Box11");
        footGrounder->AddLeg("Box20", "Box19");
        footGrounder->AddLeg("Box25", "Box23");
        footGrounder->AddLeg("Box26", "Box24");
        footGrounder->AddLeg("Box31", "Box29");
        footGrounder->AddLeg("Box35", "Box36");
        footGrounder->AddLeg("Box37", "Box34");
        footGrounder->AddLeg("Box38", "Box30");*/

        // 足の当たり判定
        std::vector<std::string> ikBoneNames = {
            "Bone.002_L.008_04"
            "Bone.002_R.008_05"
            "Bone.003_L.008_06"
            "Bone.003_R.008_07"
            "Bone.004_L.008_08"
            "Bone.004_R.008_09"
            "Bone_R.007_010"
            "Bone_L.007_011"
		};
        for (const std::string& ikBoneName : ikBoneNames)
        {
            //const int ikNodeIndex = model->GetNodeIndex(ikBoneName.c_str());
            //// 足接触コライダー
            //IKColliders.push_back(AddComponent<BoneCapsuleCollider>(
            //    model.get(),
            //    ikNodeIndex,
            //    1.42f,
            //    2.0f,
            //    Matrix::CreateFromYawPitchRoll(0.0f, RAD(90.0f), 0.0f) * Matrix::CreateTranslation(0.0f, 0.0f, 25.0f),
            //    PhysicsManager::Instance().GetDefaultMaterial(),
            //    false));

            //// 踏みつけ激薄コライダー
            //IKStampColliders.push_back(AddComponent<BoneBoxCollider>(
            //    model.get(),
            //    ikNodeIndex,
            //    Vector3{0.7f, 0.1f, 0.7f}));
        }

    }
}

void Aracore::OnRegistered(ActorManager* actorManager)
{
	actorManager->Register(std::static_pointer_cast<Actor>(std::make_shared<AracoreMachine>(this)));
}

void Aracore::OnUpdate()
{
    Entity::OnUpdate();
    //UpdateChase();
}
Actor* Aracore::FindPlayer() const
{
	ActorManager* actorManager = GetActorManager();
	if (!actorManager) return nullptr;

	for (const std::shared_ptr<Actor>& actor : actorManager->GetActors())
	{
		if (!actor || actor->IsPendingDestroy()) continue;
		if (!actor->CompareTag("Player")) continue;
		return actor.get();
	}

	return nullptr;
}

void Aracore::UpdateChase()
{
	if (!navMeshAgent) return;

	Actor* player = FindPlayer();
	if (!player)
	{
		if (chasingPlayer)
			navMeshAgent->Stop();
		chasingPlayer = false;
		return;
	}

	Vector3 toPlayer = player->transform.position - transform.position;
	toPlayer.y = 0.0f;
	const float distanceSq = toPlayer.LengthSquared();
	const float startDistanceSq = chaseStartDistance * chaseStartDistance;
	const float stopDistanceSq = chaseStopDistance * chaseStopDistance;

	if (!chasingPlayer && distanceSq <= startDistanceSq)
		chasingPlayer = true;
	else if (chasingPlayer && distanceSq >= stopDistanceSq)
	{
		chasingPlayer = false;
		navMeshAgent->Stop();
	}

	if (chasingPlayer)
		navMeshAgent->MoveToTarget(player);
}
void Aracore::OnDrawGUI()
{
    Entity::OnDrawGUI();

    if (!ImGui::TreeNode("Aracore AI"))
        return;

    ImGui::Checkbox("Chasing Player", &chasingPlayer);
    ImGui::DragFloat("Chase Start Distance", &chaseStartDistance, 0.1f, 0.0f, 100.0f);
    ImGui::DragFloat("Chase Stop Distance", &chaseStopDistance, 0.1f, 0.0f, 100.0f);
    ImGui::TreePop();
}

void Aracore::OnCollisionEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{

}

void Aracore::OnTriggerEnter(Collider* self, Collider* other, const Vector3& point, const Vector3& normal)
{
    // 踏みつけ判定に当たったらプレイヤーにダメージ
    {
        bool isFootCollider = false;
        for (Collider* collider : IKStampColliders)
        {
            if (self == collider)
            {
                isFootCollider = true;
                break;
            }
        }
        if (!isFootCollider) return;

        Actor* otherActor = other->GetOwnerActor();
        if (otherActor->CompareTag("Player"))
        {
            static_cast<Entity*>(otherActor)->TakeDamage({
                .damage = 10.0f,
                .knockBackPower = 10.0f,
				.hitColliderSelf = self,
				.hitColliderOther = other,
                .hitPosition = point,
                .hitNormal = normal
                });
        }
    }
}

void Aracore::OnDamaged(const DamageData& damageData)
{
    HitStop::Request(0.15f);
    CameraShake::Request(0.2f, 0.1f);
}

void Aracore::OnDead()
{
    printf("Aracore Dead!\n");
    Destroy(10);
}

AracoreMachine::AracoreMachine(Aracore* ownerAracore)
    : Entity("AracoreMachine", "Enemy", true, Layer::Enemy, 1000.0f, 1000.0f), ownerAracore(ownerAracore)
{
    std::shared_ptr<Model> model =
        ResourceManager::Instance().LoadModel("Data/Model/Prop/turtle_tears_vending_machine.glb");

    // リジッドボディ
    auto rb = AddComponent<RigidbodyDynamic>();
    rb->SetKinematic(true);

    // 当たり判定
    collider = AddComponent<BoxCollider>(
        rb, Vector3{1.84f, 5.95f, 0.9f}, Vector3{0.0f, 0.0f, 0.0f});
    /*collider = AddComponent<BoxCollider>(
        rb, Vector3{7.66f, 11.95f, 5.96f}, Vector3{0.0f, 5.95f, 0.21f});*/

    // Box02に追従
    Transform offset{};
    offset.SetPosition(0, -0.4f, 0);
    offset.SetScale(40.0f, 40.0f, 64.3f);
    /*AddComponent<BoneFollower>(ownerAracore->model.get(), "Box02", offset);*/

    transform.SetPosition(0, 1.8f, 0);

    // モデルレンダラーとダメージホールコンポーネントを追加
    shaderParam =
    {
        {
            {"metalness", 1.0f},
        {"roughness", 0.0f},
        {"occlusion", 0.5f},
        {"occlusionStrength", 1.0f}
        }
    };
    ModelRenderComponent* modelRenderer = AddComponent<ModelRenderComponent>(model, ModelShaderId::PBR);
    modelRenderer->SetShaderParamForAllMaterials(shaderParam);
    damageHoleComponent = AddComponent<DamageHoleComponent>(modelRenderer, 0.85f, 0.18f, 0.9f, 1.35f);
}

void AracoreMachine::OnDamaged(const DamageData& damageData)
{
    // ボコッ
    Actor* hitActor = damageData.hitColliderSelf ? damageData.hitColliderSelf->GetOwnerActor() : nullptr;
    if (damageData.hitColliderOther == collider && hitActor && hitActor->CompareTag("Player"))
    {
        if (damageData.hitPosition.has_value())
        {
            if (damageData.hitNormal.has_value())
                damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value(), damageData.hitNormal.value());
            else
                damageHoleComponent->AddDamageHoleFromPosition(damageData.hitPosition.value());
			ownerAracore->TakeDamage(damageData);
        }
        else
        {
            damageHoleComponent->AddDamageHoleFrom(hitActor);
            ownerAracore->TakeDamage(damageData);
        }
    }
}






