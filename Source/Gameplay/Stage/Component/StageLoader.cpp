// StageLoader.cpp
#include "Gameplay/Stage/Component/StageLoader.h"
#include "Application/SettingsAndDebug/PhysicsLayerManager.h"
#include "Gameplay/Stage/Stage.h"
#include "Rendering/Core/Graphics.h"
#include "magic_enum/magic_enum.hpp"
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include "Resource/ResourceManager.h"
#include "Application/Time/GameTime.h"
#include "Gameplay/Actor/ActorManager.h"
#include "Gameplay/Actor/Prop.h"
#include "Gameplay/Actor/CrystalProp.h"
#include "Gameplay/Actor/Spawner.h"
#include "Core\Foundation\Json.h"

static void LoadTransformJson(const json& transformJson, Transform& transform)
{
	if (transformJson.contains("position"))
	{
		transform.position.x = transformJson["position"].value("x", 0.0f);
		transform.position.y = transformJson["position"].value("y", 0.0f);
		transform.position.z = transformJson["position"].value("z", 0.0f);
	}

	if (transformJson.contains("rotation"))
	{
		transform.rotation.x = transformJson["rotation"].value("x", 0.0f);
		transform.rotation.y = transformJson["rotation"].value("y", 0.0f);
		transform.rotation.z = transformJson["rotation"].value("z", 0.0f);
		transform.rotation.w = transformJson["rotation"].value("w", 1.0f);
	}

	if (transformJson.contains("scale"))
	{
		transform.scale.x = transformJson["scale"].value("x", 1.0f);
		transform.scale.y = transformJson["scale"].value("y", 1.0f);
		transform.scale.z = transformJson["scale"].value("z", 1.0f);
	}
}

void StageLoader::DrawDestroyGUI(PropData& propData)
{
	// 破壊設定
	ImGui::Checkbox((const char*)u8"破壊可能", &propData.useDestroy);
	if (propData.useDestroy)
	{
		ImGui::DragFloat((const char*)u8"耐久値", &propData.destroyLife, 0.1f, 0.0f, 100000.0f);
		if (propData.destroyLife < 0.0f) propData.destroyLife = 0.0f;

		// ダメージを受ける物理レイヤー
		if (ImGui::TreeNode((const char*)u8"ダメージを受けるレイヤー"))
		{
			PhysicsLayerManager& layerManager = PhysicsLayerManager::Instance();
			for (int layer = 0; layer < EditableLayerCount; ++layer)
			{
				if (layerManager.GetLayerName(static_cast<LayerId>(layer)).empty()) continue;
				bool enabled = (propData.destroyLayerMask & (1u << layer)) != 0;
				const std::string label =
					layerManager.GetLayerDisplayName(static_cast<LayerId>(layer));
				if (ImGui::Checkbox(label.c_str(), &enabled))
				{
					if (enabled) propData.destroyLayerMask |= 1u << layer;
					else propData.destroyLayerMask &= ~(1u << layer);
				}
			}
			ImGui::TreePop();
		}
	}
}

uint32_t StageLoader::GetDefaultDestroyLayerMask()
{
	uint32_t mask = 0;
	const LayerId playerAttack = Layers::Get("PlayerAtk");
	const LayerId enemyAttack = Layers::Get("EnemyAtk");
	if (playerAttack < EditableLayerCount) mask |= 1u << playerAttack;
	if (enemyAttack < EditableLayerCount) mask |= 1u << enemyAttack;
	return mask;
}
StageLoader::StageLoader(Object* owner, Stage* stage, std::filesystem::path jsonPath)
	: Component(owner), stage(stage), jsonPath(jsonPath)
{
	LoadJson();
}

StageLoader::StageLoader(Object* owner, Stage* stage, const std::string& jsonText, bool fromMemory)
	: Component(owner), stage(stage), jsonText(jsonText)
{
	LoadJson();
}

std::vector<StageLoader::EditorObjectReference> StageLoader::GetEditorObjects()
{
	std::vector<EditorObjectReference> objects;
	objects.reserve(propDataList.size() + (hasPlayerStart ? 1 : 0));
	if (hasPlayerStart)
		objects.push_back({EditorObjectType::PlayerStart, -1, &playerStartTransform});
	for (int i = 0; i < static_cast<int>(propDataList.size()); ++i)
		objects.push_back({EditorObjectType::Prop, i, &propDataList[i].transform});
	return objects;
}

bool StageLoader::SelectEditorObject(EditorObjectType type, int index)
{
	const bool valid = (type == EditorObjectType::PlayerStart && hasPlayerStart) ||
		(type == EditorObjectType::Prop && index >= 0 &&
		 index < static_cast<int>(propDataList.size()));
	if (!valid) return false;
	selectedEditorObjectType = type;
	selectedEditorObjectIndex = index;
	return true;
}

bool StageLoader::SelectEditorActor(const Actor* actor)
{
	if (!actor) return false;
	for (int i = 0; i < static_cast<int>(addedPropActors.size()); ++i)
		if (addedPropActors[i] == actor) return SelectEditorObject(EditorObjectType::Prop, i);
	return false;
}

bool StageLoader::SelectEditorObjectAtRay(const Vector3& origin, const Vector3& direction)
{
	if (direction.LengthSquared() < 0.000001f) return false;
	Vector3 normalizedDirection = direction;
	normalizedDirection.Normalize();
	const DirectX::SimpleMath::Ray ray(origin, normalizedDirection);

	float nearestDistance = std::numeric_limits<float>::max();
	int nearestIndex = -1;
	for (int propIndex = 0; propIndex < static_cast<int>(propDataList.size()); ++propIndex)
	{
		PropData& propData = propDataList[propIndex];
		if (!propData.model) propData.model = LoadPropModel(propData.modelPath);
		if (!propData.model) continue;

		propData.transform.Update();
		auto bounds = editorSelectionBounds.find(propData.modelPath);
		if (bounds == editorSelectionBounds.end())
		{
			propData.model->UpdateTransform(
				Matrix::CreateTranslation(propData.model->GetVmdlExtensionData().rootOffset));
			const Matrix renderScale = propData.model->GetRenderScaleTransform();
			Vector3 minimum(
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max(),
				std::numeric_limits<float>::max());
			Vector3 maximum(
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest(),
				std::numeric_limits<float>::lowest());
			bool hasVertex = false;
			for (const VMDLModel::Mesh& mesh : propData.model->GetMeshes())
			{
				if (!mesh.isDraw || !mesh.node) continue;
				const Matrix vertexTransform = mesh.node->worldTransform * renderScale;
				for (const VMDLModel::Vertex& vertex : mesh.vertices)
				{
					const Vector3 position =
						Vector3::Transform(vertex.position, vertexTransform);
					minimum = Vector3::Min(minimum, position);
					maximum = Vector3::Max(maximum, position);
					hasVertex = true;
				}
			}
			if (!hasVertex) continue;

			DirectX::BoundingBox localBounds;
			localBounds.Center = (minimum + maximum) * 0.5f;
			localBounds.Extents = (maximum - minimum) * 0.5f;
			bounds = editorSelectionBounds.emplace(propData.modelPath, localBounds).first;
		}

		DirectX::BoundingBox worldBounds;
		bounds->second.Transform(worldBounds, propData.transform.matrix);
		float boundsDistance = 0.0f;
		if (!ray.Intersects(worldBounds, boundsDistance) || boundsDistance >= nearestDistance)
			continue;

		const Matrix modelTransform =
			Matrix::CreateTranslation(propData.model->GetVmdlExtensionData().rootOffset) *
			propData.transform.matrix;
		propData.model->UpdateTransform(modelTransform);
		const Matrix renderScale = propData.model->GetRenderScaleTransform();

		for (const VMDLModel::Mesh& mesh : propData.model->GetMeshes())
		{
			if (!mesh.isDraw || mesh.indices.size() < 3) continue;
			const Matrix vertexTransform =
				(mesh.node ? mesh.node->worldTransform : modelTransform) * renderScale;
			for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
			{
				const uint32_t index0 = mesh.indices[index];
				const uint32_t index1 = mesh.indices[index + 1];
				const uint32_t index2 = mesh.indices[index + 2];
				if (index0 >= mesh.vertices.size() || index1 >= mesh.vertices.size() ||
					index2 >= mesh.vertices.size())
					continue;

				const Vector3 vertex0 =
					Vector3::Transform(mesh.vertices[index0].position, vertexTransform);
				const Vector3 vertex1 =
					Vector3::Transform(mesh.vertices[index1].position, vertexTransform);
				const Vector3 vertex2 =
					Vector3::Transform(mesh.vertices[index2].position, vertexTransform);
				float distance = 0.0f;
				if (!ray.Intersects(vertex0, vertex1, vertex2, distance) ||
					distance >= nearestDistance)
					continue;

				nearestDistance = distance;
				nearestIndex = propIndex;
			}
		}
	}

	return nearestIndex >= 0 && SelectEditorObject(EditorObjectType::Prop, nearestIndex);
}

void StageLoader::ClearEditorSelection()
{
	selectedEditorObjectType = EditorObjectType::None;
	selectedEditorObjectIndex = -1;
}

Transform* StageLoader::GetSelectedEditorTransform()
{
	if (selectedEditorObjectType == EditorObjectType::PlayerStart && hasPlayerStart)
	{
		playerStartTransform.Update();
		return &playerStartTransform;
	}
	if (selectedEditorObjectType == EditorObjectType::Prop && selectedEditorObjectIndex >= 0 &&
		selectedEditorObjectIndex < static_cast<int>(propDataList.size()))
	{
		PropData& propData = propDataList[selectedEditorObjectIndex];
		if (!propData.model)
			propData.model = LoadPropModel(propData.modelPath);
		if (!propData.model) return nullptr;
		selectedEditorTransform = propData.transform;
		selectedEditorTransform.position =
			Vector3::Transform(GetPropPlacementOffset(*propData.model), propData.transform.matrix);
		selectedEditorTransform.Update();
		return &selectedEditorTransform;
	}
	ClearEditorSelection();
	return nullptr;
}

void StageLoader::RefreshSelectedEditorObject()
{
	if (selectedEditorObjectType == EditorObjectType::PlayerStart && hasPlayerStart)
	{
		playerStartTransform.Update();
		return;
	}
	if (selectedEditorObjectType != EditorObjectType::Prop || selectedEditorObjectIndex < 0 ||
		selectedEditorObjectIndex >= static_cast<int>(propDataList.size()) ||
		selectedEditorObjectIndex >= static_cast<int>(addedPropActors.size()) ||
		!addedPropActors[selectedEditorObjectIndex])
		return;

	selectedEditorTransform.Update();
	PropData& propData = propDataList[selectedEditorObjectIndex];
	propData.transform.scale = selectedEditorTransform.scale;
	propData.transform.rotation = selectedEditorTransform.rotation;
	const Matrix rotationScale = Matrix::CreateScale(propData.transform.scale) *
								 Matrix::CreateFromQuaternion(propData.transform.rotation);
	propData.transform.position =
		selectedEditorTransform.position -
		Vector3::TransformNormal(GetPropPlacementOffset(*propData.model), rotationScale);
	propData.transform.Update();
	if (Prop* prop = dynamic_cast<Prop*>(addedPropActors[selectedEditorObjectIndex]))
	{
		prop->ApplyStageData(propData);
	}
	else if (CrystalProp* crystal =
				 dynamic_cast<CrystalProp*>(addedPropActors[selectedEditorObjectIndex]))
	{
		crystal->ApplyStageData(propData);
	}
}

void StageLoader::Update()
{
	for (int propIndex = 0; propIndex < static_cast<int>(propDataList.size()); ++propIndex)
	{
		auto& prop = propDataList[propIndex];
		if (!prop.model)
		{
			prop.model = LoadPropModel(prop.modelPath);
		}
		if (!prop.model) continue;

		prop.transform.Update();
		prop.model->UpdateTransform(prop.transform.matrix);

		if (propIndex < static_cast<int>(addedPropActors.size()))
		{
			Actor* addedActor = addedPropActors[propIndex];
			if (addedActor && !stage->GetActorManager().Contains(addedActor))
			{
				for (Actor*& actor : addedRealActors)
					if (actor == addedActor) actor = nullptr;
				addedPropActors[propIndex] = nullptr;
				addedActor = nullptr;
			}
			if (Prop* actor = dynamic_cast<Prop*>(addedActor))
				actor->ApplyStageData(prop);
			else if (CrystalProp* actor = dynamic_cast<CrystalProp*>(addedActor))
				actor->ApplyStageData(prop);
			else if (addedActor)
			{
				addedActor->SetName(prop.name);
				addedActor->SetTag(prop.tag);
				addedActor->transform = prop.transform;
				addedActor->transform.Update();
			}
			ConfigureSpawner(addedActor, prop);
		}
	}
}

void StageLoader::SetCrystalBreakParticleSystem(ParticleSystem* particleSystem)
{
	crystalBreakParticleSystem = particleSystem;

	for (Actor* actor : addedPropActors)
	{
		CrystalProp* crystalActor = dynamic_cast<CrystalProp*>(actor);
		if (crystalActor)
		{
			crystalActor->SetDestroyedCallback([this](CrystalProp* destroyedCrystal) {
				for (Actor*& addedActor : addedRealActors)
				{
					if (addedActor == destroyedCrystal) addedActor = nullptr;
				}
				for (Actor*& addedActor : addedPropActors)
				{
					if (addedActor == destroyedCrystal) addedActor = nullptr;
				}
			});
			crystalActor->SetBreakParticleSystem(crystalBreakParticleSystem);
		}
	}
}

void StageLoader::RegisterSpawnerFactory(const std::string& entityName, SpawnerFactory factory)
{
	if (entityName.empty() || !factory) return;
	spawnerFactories[entityName] = std::move(factory);
	for (int i = 0; i < static_cast<int>(propDataList.size()) &&
		 i < static_cast<int>(addedPropActors.size()); ++i)
	{
		ConfigureSpawner(addedPropActors[i], propDataList[i]);
	}
}

std::vector<Spawner*> StageLoader::GetSpawners() const
{
	std::vector<Spawner*> result;
	for (Actor* actor : addedPropActors)
	{
		if (!actor) continue;
		Spawner* spawner = actor->GetComponent<Spawner>();
		if (spawner && spawner->IsActive()) result.push_back(spawner);
	}
	return result;
}

void StageLoader::DrawGUI()
{
	DrawEditorGUI();
}

Actor* StageLoader::CreatePropActor(PropData& propData)
{
	std::shared_ptr<Actor> actor;
	if (propData.isSpawner && !propData.editorPreview)
	{
		actor = std::make_shared<Actor>(propData.name, propData.tag, true);
		actor->transform = propData.transform;
		actor->transform.Update();
	}
	else if (propData.type == PropType::Crystal)
	{
		auto crystal = std::make_shared<CrystalProp>(propData);
		crystal->SetDestroyedCallback([this](CrystalProp* destroyedCrystal) {
			for (Actor*& addedActor : addedRealActors)
				if (addedActor == destroyedCrystal) addedActor = nullptr;
			for (Actor*& addedActor : addedPropActors)
				if (addedActor == destroyedCrystal) addedActor = nullptr;
		});
		crystal->SetBreakParticleSystem(crystalBreakParticleSystem);
		actor = std::move(crystal);
	}
	else actor = std::make_shared<Prop>(propData);

	Actor* result = actor.get();
	ConfigureSpawner(result, propData);
	stage->GetActorManager().Register(actor);
	return result;
}

void StageLoader::ConfigureSpawner(Actor* actor, const PropData& propData)
{
	if (!actor) return;
	Spawner* spawner = actor->GetComponent<Spawner>();
	if (!spawner && propData.isSpawner)
		spawner = actor->AddComponent<Spawner>(propData.spawnerEntityName);
	if (!spawner) return;

	spawner->SetEntityName(propData.spawnerEntityName);
	spawner->SetActorManager(&stage->GetActorManager());
	spawner->SetActive(propData.isSpawner);
	spawner->SetEditorPreview(propData.isSpawner && propData.editorPreview);
	Transform summonTransform = propData.transform;
	if (propData.model)
	{
		const Matrix rotationScale = Matrix::CreateScale(propData.transform.scale) *
			Matrix::CreateFromQuaternion(propData.transform.rotation);
		summonTransform.position +=
			Vector3::TransformNormal(GetPropPlacementOffset(*propData.model), rotationScale);
		summonTransform.Update();
	}
	spawner->SetSummonTransform(summonTransform);
	const auto factory = spawnerFactories.find(propData.spawnerEntityName);
	spawner->SetFactory(factory == spawnerFactories.end() ? SpawnerFactory{} : factory->second);
}

std::shared_ptr<VMDLModel> StageLoader::LoadPropModel(const std::string& modelPath) const
{
	if (editorModels)
	{
		const auto found = editorModels->find(modelPath);
		if (found != editorModels->end() && found->second) return found->second->Clone();
		return nullptr;
	}
	return ResourceManager::Instance().LoadModel(modelPath);
}

Vector3 StageLoader::GetPropPlacementOffset(VMDLModel& model)
{
	model.UpdateTransform(Matrix::CreateTranslation(model.GetVmdlExtensionData().rootOffset));
	for (const VMDLModel::VmdlCollider& collider : model.GetVmdlExtensionData().colliders)
	{
		if (collider.name != PlacementColliderName || collider.nodeIndex < 0 ||
			collider.nodeIndex >= static_cast<int>(model.GetNodes().size()))
			continue;

		const Matrix offset = Matrix::CreateFromYawPitchRoll(RAD(collider.rotation.y),
								  RAD(collider.rotation.x), RAD(collider.rotation.z)) *
							  Matrix::CreateTranslation(collider.center);
		return model
			.GetScaledAttachmentTransform(
				offset * model.GetNodes()[collider.nodeIndex].worldTransform)
			.Translation();
	}
	return Vector3::Zero;
}

bool StageLoader::AddEditorProp(const std::string& modelPath, const Vector3& terrainPoint)
{
	PropData propData;
	propData.modelPath = modelPath;
	propData.name = std::filesystem::path(modelPath).stem().string();
	propData.destroyLayerMask = GetDefaultDestroyLayerMask();
	propData.editorPreview = editorModels != nullptr;
	propData.model = LoadPropModel(modelPath);
	if (!propData.model) return false;
	const Matrix rotationScale = Matrix::CreateScale(propData.transform.scale) *
								 Matrix::CreateFromQuaternion(propData.transform.rotation);
	propData.transform.position =
		terrainPoint -
		Vector3::TransformNormal(GetPropPlacementOffset(*propData.model), rotationScale);
	propData.transform.Update();

	propDataList.push_back(std::move(propData));
	Actor* propActor = CreatePropActor(propDataList.back());
	addedRealActors.push_back(propActor);
	addedPropActors.push_back(propActor);
	selectedEditorObjectType = EditorObjectType::Prop;
	selectedEditorObjectIndex = static_cast<int>(propDataList.size()) - 1;
	return true;
}

void StageLoader::SetEditorPlayerStart(const Vector3& terrainPoint)
{
	hasPlayerStart = true;
	playerStartTransform.position = terrainPoint;
	playerStartTransform.scale = Vector3::One;
	playerStartTransform.Update();
	selectedEditorObjectType = EditorObjectType::PlayerStart;
	selectedEditorObjectIndex = -1;
}

std::vector<std::string> StageLoader::GetMissingModelPaths() const
{
	std::set<std::string> uniquePaths;
	for (const PropData& propData : propDataList)
		if (!propData.model && !propData.modelPath.empty()) uniquePaths.insert(propData.modelPath);
	return {uniquePaths.begin(), uniquePaths.end()};
}

bool StageLoader::ReplaceMissingModelPath(
	const std::string& missingPath, const std::string& replacementPath)
{
	if (missingPath.empty() || replacementPath.empty()) return false;
	std::shared_ptr<VMDLModel> replacement = LoadPropModel(replacementPath);
	if (!replacement) return false;

	bool replaced = false;
	for (int index = 0; index < static_cast<int>(propDataList.size()); ++index)
	{
		PropData& propData = propDataList[index];
		if (propData.modelPath != missingPath) continue;
		propData.modelPath = replacementPath;
		propData.model = replacement->Clone();
		if (propData.name.empty() || propData.name == std::filesystem::path(missingPath).stem().string())
			propData.name = std::filesystem::path(replacementPath).stem().string();

		if (index < static_cast<int>(addedPropActors.size()) && addedPropActors[index])
			addedPropActors[index]->Destroy();
		Actor* actor = CreatePropActor(propData);
		if (index < static_cast<int>(addedPropActors.size())) addedPropActors[index] = actor;
		if (index < static_cast<int>(addedRealActors.size())) addedRealActors[index] = actor;
		replaced = true;
	}
	return replaced;
}

void StageLoader::RemovePropsWithModelPath(const std::string& modelPath)
{
	for (int index = static_cast<int>(propDataList.size()) - 1; index >= 0; --index)
	{
		if (propDataList[index].modelPath != modelPath) continue;
		if (index < static_cast<int>(addedPropActors.size()) && addedPropActors[index])
			addedPropActors[index]->Destroy();
		propDataList.erase(propDataList.begin() + index);
		if (index < static_cast<int>(addedPropActors.size()))
			addedPropActors.erase(addedPropActors.begin() + index);
		if (index < static_cast<int>(addedRealActors.size()))
			addedRealActors.erase(addedRealActors.begin() + index);
	}
	ClearEditorSelection();
}

bool StageLoader::BuildEditorPropTransform(
	VMDLModel& model, const Vector3& placementPoint, Transform& transform)
{
	const Matrix rotationScale =
		Matrix::CreateScale(transform.scale) * Matrix::CreateFromQuaternion(transform.rotation);
	transform.position =
		placementPoint - Vector3::TransformNormal(GetPropPlacementOffset(model), rotationScale);
	transform.Update();
	return true;
}

void StageLoader::DrawEditorGUI()
{
	// プレイヤー初期位置は通常のVMDLと区別し、常に先頭へ表示する。
	if (hasPlayerStart)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(125, 82, 8, 255));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(170, 112, 12, 255));
		const bool selected = selectedEditorObjectType == EditorObjectType::PlayerStart;
		const bool open = ImGui::TreeNodeEx(
			ICON_FA_MAP_MARKER_ALT " プレイヤー初期位置###PlayerStart",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
			(selected ? ImGuiTreeNodeFlags_Selected : 0));
		if (ImGui::IsItemClicked()) SelectEditorObject(EditorObjectType::PlayerStart, -1);
		ImGui::PopStyleColor(2);
		if (open)
		{
			playerStartTransform.DrawGUI(true);
			if (ImGui::Button((const char*)u8"初期位置を削除"))
			{
				hasPlayerStart = false;
				ClearEditorSelection();
			}
			ImGui::TreePop();
		}
	}

	std::map<std::string, std::vector<int>> groups;
	for (int index = 0; index < static_cast<int>(propDataList.size()); ++index)
		groups[propDataList[index].modelPath].push_back(index);
	for (const auto& [modelPath, indices] : groups)
	{
		const std::string modelName = modelPath.empty()
			? (const char*)u8"モデルなし"
			: std::filesystem::path(modelPath).stem().string();
		const std::string groupLabel = std::string(ICON_FA_CUBE "  ") + modelName +
			" (" + std::to_string(indices.size()) + ")###ModelGroup" + modelPath;
		if (!ImGui::TreeNodeEx(groupLabel.c_str(),
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
			continue;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", modelPath.c_str());
		bool changed = false;
		for (int index : indices)
		{
			if (DrawPropEditor(index))
			{
				changed = true;
				break;
			}
		}
		ImGui::TreePop();
		if (changed) break;
	}
}

bool StageLoader::DrawPropEditor(int index)
{
	PropData& propData = propDataList[index];
	ImGui::PushID(index);
	const bool selected = selectedEditorObjectType == EditorObjectType::Prop &&
		selectedEditorObjectIndex == index;
	const std::string label =
		(propData.name.empty() ? (const char*)u8"名前なし" : propData.name) + "###Prop";
	const bool open = ImGui::TreeNodeEx(
		label.c_str(), selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);
	if (ImGui::IsItemClicked()) SelectEditorObject(EditorObjectType::Prop, index);
	if (!open)
	{
		ImGui::PopID();
		return false;
	}

	ImGui::InputText((const char*)u8"名前", &propData.name);
	ImGui::InputText((const char*)u8"タグ", &propData.tag);
	propData.transform.DrawGUI();
	ImGui::Checkbox((const char*)u8"スポナー", &propData.isSpawner);
	if (propData.isSpawner)
		ImGui::InputText((const char*)u8"生成対象", &propData.spawnerEntityName);
	if (propData.type == PropType::Standard)
	{
		propData.rigidbodyData.DrawGUI();
		DrawDestroyGUI(propData);
	}

	if (ImGui::Button((const char*)u8"複製"))
	{
		PropData copy = propData;
		copy.name += " Copy";
		propDataList.insert(propDataList.begin() + index + 1, std::move(copy));
		Actor* actor = CreatePropActor(propDataList[index + 1]);
		addedRealActors.insert(addedRealActors.begin() + index + 1, actor);
		addedPropActors.insert(addedPropActors.begin() + index + 1, actor);
		SelectEditorObject(EditorObjectType::Prop, index + 1);
		ImGui::TreePop();
		ImGui::PopID();
		return true;
	}
	ImGui::SameLine();
	if (ImGui::Button((const char*)u8"削除"))
	{
		if (selectedEditorObjectType == EditorObjectType::Prop)
		{
			if (selectedEditorObjectIndex == index) ClearEditorSelection();
			else if (selectedEditorObjectIndex > index) --selectedEditorObjectIndex;
		}
		if (index < static_cast<int>(addedPropActors.size()) && addedPropActors[index])
			addedPropActors[index]->Destroy();
		if (index < static_cast<int>(addedRealActors.size()))
			addedRealActors.erase(addedRealActors.begin() + index);
		if (index < static_cast<int>(addedPropActors.size()))
			addedPropActors.erase(addedPropActors.begin() + index);
		propDataList.erase(propDataList.begin() + index);
		ImGui::TreePop();
		ImGui::PopID();
		return true;
	}

	ImGui::TreePop();
	ImGui::PopID();
	return false;
}

void StageLoader::LoadJson()
{
	std::ifstream file;
	std::istringstream memory(jsonText);
	std::istream* input = &memory;
	if (jsonText.empty())
	{
		if (!std::filesystem::exists(jsonPath)) return;
		file.open(jsonPath);
		if (!file) return;
		input = &file;
	}

	json root;

	try
	{
		*input >> root;
	}
	catch (const json::parse_error&)
	{
		return;
	}

	propDataList.clear();
	hasPlayerStart = false;
	playerStartTransform = Transform{};
	ClearEditorSelection();

	for (auto addedActor : addedRealActors)
	{
		if (addedActor) addedActor->Destroy();
	}
	addedRealActors.clear();
	addedPropActors.clear();
	if (root.contains("playerStart") && root["playerStart"].is_object())
	{
		LoadTransformJson(root["playerStart"], playerStartTransform);
		playerStartTransform.scale = Vector3::One;
		playerStartTransform.Update();
		hasPlayerStart = true;
	}

	if (root.contains("props") && root["props"].is_array())
	{
		for (const auto& propJson : root["props"])
		{
			PropData propData;
			propData.name = propJson.value("name", std::string());
			propData.tag = propJson.value("tag", std::string("Prop"));
			const auto type =
				magic_enum::enum_cast<PropType>(propJson.value("type", std::string("Standard")));
			if (type.has_value()) propData.type = type.value();

			if (propJson.contains("transform"))
			{
				const auto& transformJson = propJson["transform"];

				if (transformJson.contains("position"))
				{
					propData.transform.position.x = transformJson["position"].value("x", 0.0f);
					propData.transform.position.y = transformJson["position"].value("y", 0.0f);
					propData.transform.position.z = transformJson["position"].value("z", 0.0f);
				}

				if (transformJson.contains("rotation"))
				{
					propData.transform.rotation.x = transformJson["rotation"].value("x", 0.0f);
					propData.transform.rotation.y = transformJson["rotation"].value("y", 0.0f);
					propData.transform.rotation.z = transformJson["rotation"].value("z", 0.0f);
					propData.transform.rotation.w = transformJson["rotation"].value("w", 1.0f);
				}

				if (transformJson.contains("scale"))
				{
					propData.transform.scale.x = transformJson["scale"].value("x", 1.0f);
					propData.transform.scale.y = transformJson["scale"].value("y", 1.0f);
					propData.transform.scale.z = transformJson["scale"].value("z", 1.0f);
				}
			}

			if (propJson.contains("rigidbody"))
			{
				const auto& rigidbodyJson = propJson["rigidbody"];
				propData.rigidbodyData.isDynamic = rigidbodyJson.value("isDynamic", false);
			}

			propData.useDestroy = propJson.value("useDestroy", false);
			propData.destroyLife = propJson.value("destroyLife", 0.0f);
			propData.destroyLayerMask =
				propJson.value("destroyLayerMask", GetDefaultDestroyLayerMask());
			propData.isSpawner = propJson.value("isSpawner", false);
			propData.spawnerEntityName =
				propJson.value("spawnerEntityName", std::string("EnemySmall"));
			propData.editorPreview = editorModels != nullptr;

			propData.modelPath = propJson.value("modelPath", "");
			std::string lowerModelPath = propData.modelPath;
			std::transform(lowerModelPath.begin(), lowerModelPath.end(), lowerModelPath.begin(),
				[](unsigned char value) { return static_cast<char>(std::tolower(value)); });
			if (lowerModelPath.starts_with("data/"))
				propData.modelPath.replace(0, 4, "Resources");
			if (propData.name.empty())
				propData.name = std::filesystem::path(propData.modelPath).stem().string();
			propData.model = LoadPropModel(propData.modelPath);

			propDataList.push_back(std::move(propData));
			Actor* propActor = propDataList.back().model
				? CreatePropActor(propDataList.back()) : nullptr;
			addedRealActors.push_back(propActor);
			addedPropActors.push_back(propActor);
		}
	}

	if (root.contains("crystals") && root["crystals"].is_array())
	{
		for (const auto& crystalJson : root["crystals"])
		{
			std::vector<Transform> transforms;
			if (crystalJson.contains("transforms") && crystalJson["transforms"].is_array())
			{
				Transform parentTransform;
				if (crystalJson.contains("transform"))
					LoadTransformJson(crystalJson["transform"], parentTransform);
				parentTransform.Update();

				for (const auto& transformJson : crystalJson["transforms"])
				{
					Transform transform;
					LoadTransformJson(transformJson, transform);
					transform.Update();
					transforms.emplace_back(transform.matrix * parentTransform.matrix);
				}
			}
			else
			{
				Transform transform;
				if (crystalJson.contains("transform"))
					LoadTransformJson(crystalJson["transform"], transform);
				transforms.push_back(transform);
			}

			for (const Transform& transform : transforms)
			{
				PropData propData;
				propData.name = "Crystal";
				propData.tag = "CrystalProp";
				propData.type = PropType::Crystal;
				propData.modelPath = "Resources/Model/Prop/crystals_from_space";
				propData.transform = transform;
				propData.editorPreview = editorModels != nullptr;
				propData.model = LoadPropModel(propData.modelPath);
				propDataList.push_back(std::move(propData));
				Actor* crystalActor = propDataList.back().model
					? CreatePropActor(propDataList.back()) : nullptr;
				addedRealActors.push_back(crystalActor);
				addedPropActors.push_back(crystalActor);
			}
		}
	}

}

void StageLoader::LoadJsonText(const std::string& text)
{
	jsonText = text;
	jsonPath.clear();
	LoadJson();
}

std::string StageLoader::SaveJsonText()
{
	const std::filesystem::path previousPath = jsonPath;
	jsonPath.clear();
	SaveJson();
	jsonPath = previousPath;
	return jsonText;
}

void StageLoader::SaveJson()
{
	json root;
	if (hasPlayerStart)
	{
		root["playerStart"]["position"] = {
			{"x", playerStartTransform.position.x}, {"y", playerStartTransform.position.y},
			{"z", playerStartTransform.position.z}};
		root["playerStart"]["rotation"] = {
			{"x", playerStartTransform.rotation.x}, {"y", playerStartTransform.rotation.y},
			{"z", playerStartTransform.rotation.z}, {"w", playerStartTransform.rotation.w}};
		root["playerStart"]["scale"] = {{"x", 1.0f}, {"y", 1.0f}, {"z", 1.0f}};
	}

	root["props"] = json::array();

	for (const auto& propData : propDataList)
	{
		json propJson;
		propJson["name"] = propData.name;
		propJson["tag"] = propData.tag;
		propJson["type"] = std::string(magic_enum::enum_name(propData.type));

		propJson["transform"]["position"]["x"] = propData.transform.position.x;
		propJson["transform"]["position"]["y"] = propData.transform.position.y;
		propJson["transform"]["position"]["z"] = propData.transform.position.z;

		propJson["transform"]["rotation"]["x"] = propData.transform.rotation.x;
		propJson["transform"]["rotation"]["y"] = propData.transform.rotation.y;
		propJson["transform"]["rotation"]["z"] = propData.transform.rotation.z;
		propJson["transform"]["rotation"]["w"] = propData.transform.rotation.w;

		propJson["transform"]["scale"]["x"] = propData.transform.scale.x;
		propJson["transform"]["scale"]["y"] = propData.transform.scale.y;
		propJson["transform"]["scale"]["z"] = propData.transform.scale.z;

		propJson["rigidbody"]["isDynamic"] = propData.rigidbodyData.isDynamic;

		propJson["useDestroy"] = propData.useDestroy;
		propJson["destroyLife"] = propData.destroyLife;
		propJson["destroyLayerMask"] = propData.destroyLayerMask;
		propJson["isSpawner"] = propData.isSpawner;
		propJson["spawnerEntityName"] = propData.spawnerEntityName;

		propJson["modelPath"] = propData.modelPath;

		root["props"].push_back(propJson);
	}

	jsonText = root.dump(4);
	if (jsonPath.empty()) return;
	if (jsonPath.has_parent_path())
	{
		std::filesystem::create_directories(jsonPath.parent_path());
	}

	std::ofstream ofs(jsonPath);

	if (!ofs)
	{
		return;
	}

	ofs << jsonText;
}
