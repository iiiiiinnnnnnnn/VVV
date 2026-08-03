// ModelRenderer.h
// ModelRenderer.h

#pragma once
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

enum class ModelShaderId
{
	VMat,

	EnumCount
};

class ModelRenderer
{
public:
	ModelRenderer(ID3D11Device* device);
	~ModelRenderer() {}

	void Draw(
		ModelShaderId shaderId,
		std::shared_ptr<VMDLModel> model,
		const VMatRenderParams* params = nullptr);

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
		const VMatRenderParams*		params;
	};

	struct TransparencyDrawInfo
	{
		ModelShaderId				shaderId;
		const VMDLModel::Mesh*		mesh;
		Matrix					renderScaleTransform;
		float					distance;
		const VMatRenderParams*		params;
	};

	std::unique_ptr<ModelShader>			shaders[static_cast<int>(ModelShaderId::EnumCount)];
	std::vector<DrawInfo>					drawInfos;
	std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	skeletonConstantBuffer;
};
