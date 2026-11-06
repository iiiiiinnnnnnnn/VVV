#pragma once
#include "Rendering/Core/ConstantBuffer.h"
#include <d3d11.h>
#include <wrl.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Resource/VMDLModel.h"
#include "Rendering/Shader/Shader.h"
#include "Gameplay/Lighting/CbLightData.h"

class MeshCache;

class ModelRenderer
{
public:
	ModelRenderer(ID3D11Device* device);
	~ModelRenderer() {}

	void Draw(
		ModelShaderId shaderId,
		std::shared_ptr<VMDLModel> model,
		const PBRRenderParams* params = nullptr);
	void DrawMeshCache(
		ModelShaderId shaderId,
		std::shared_ptr<MeshCache> meshCache,
		std::shared_ptr<VMDLModel> skeleton,
		const PBRRenderParams* params = nullptr);

	// シェーダーは各マテリアルのIDから選択する。
	void Draw(std::shared_ptr<VMDLModel> model, const PBRRenderParams* params = nullptr);
	void DrawMeshCache(std::shared_ptr<MeshCache> meshCache, std::shared_ptr<VMDLModel> skeleton,
		const PBRRenderParams* params = nullptr);
	ModelShader* GetShader(ModelShaderId id) const;

	void Render(const RenderContext& rc);

private:
	
	struct CbScene
	{
		Matrix		viewProjection;
		Vector3		viewPosition;
		float DUMMY;
		CbLightData lightData;
		Color distanceFogColor;
		Vector4 distanceFogParams;
	};

	struct CbSkeleton
	{
		Matrix		boneTransforms[256];
	};

	struct DrawInfo
	{
		ModelShaderId				shaderId;
		std::shared_ptr<VMDLModel>	model;
		std::shared_ptr<MeshCache>	meshCache;
		const PBRRenderParams*		params;
	};

	struct TransparencyDrawInfo
	{
		bool refractive = false;
		ModelShaderId				shaderId;
		const VMDLModel::Mesh*		mesh;
		Matrix					renderScaleTransform;
		float					distance;
		const PBRRenderParams*		params;
	};

	std::unique_ptr<ModelShader>			shaders[static_cast<int>(ModelShaderId::EnumCount)];
	std::vector<DrawInfo>					drawInfos;
	std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

	std::unique_ptr<Gpu::ConstantBuffer> sceneConstantBuffer;
	std::unique_ptr<Gpu::ConstantBuffer> skeletonConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> transmissionBackground;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> transmissionBackgroundView;
};
