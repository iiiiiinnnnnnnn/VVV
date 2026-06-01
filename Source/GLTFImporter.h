// GLTFImporter.h

#pragma once

#include <map>
#include <filesystem>
#include <tiny_gltf.h>
#include "Model.h"
#include "RenderContext.h"

class GLTFImporter
{
private:
	using MeshList = std::vector<Model::Mesh>;
	using MaterialList = std::vector<Model::Material>;
	using NodeList = std::vector<Model::Node>;
	using AnimationList = std::vector<Model::Animation>;

public:
	GLTFImporter(const char* filename);

	// ノードデータを読み込み
	void LoadNodes(NodeList& nodes);

	// メッシュデータを読み込み
	void LoadMeshes(MeshList& meshes, const NodeList& nodes);

	// マテリアルデータを読み込み
	void LoadMaterials(MaterialList& materials, ID3D11Device* device = nullptr);

	// アニメーションデータを読み込み
	void LoadAnimations(AnimationList& animations, const NodeList& nodes, float sampleRate = 60);

	// ライトデータを読み込み
	void LoadLights(LightData& lightData, const NodeList& nodes);

private:
	// 座標系変換
	static void ConvertPositionAxisSystem(Vector3& v);
	static void ConvertPositionAxisSystem(Vector4& v);
	static void ConvertRotationAxisSystem(Quaternion& q);
	static void ConvertMatrixAxisSystem(DirectX::XMFLOAT4X4& m);
	static void ConvertNodeAxisSystem(Model::Node& node);
	static void ConvertMeshAxisSystem(Model::Mesh& mesh);
	static void ConvertAnimationAxisSystem(Model::Animation& animation);

	// タンジェント計算
	static void ComputeTangents(std::vector<Model::Vertex>& vertices, const std::vector<uint32_t>& indices);

private:
	std::filesystem::path			filepath;
	tinygltf::Model					gltfModel;
};
