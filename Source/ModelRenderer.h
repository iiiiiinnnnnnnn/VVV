#pragma once

#include "Common.h"
#include "Model.h"
#include "Shader.h"

enum class ShaderId
{
	Basic,
	Lambert,

	EnumCount
};

class ModelRenderer
{
public:
	ModelRenderer(ID3D11Device* device);
	~ModelRenderer() {}

	// î†ï`âÊ
	void Draw(ShaderId shaderId, std::shared_ptr<Model> model);

	// ï`âÊé¿çs
	void Render(const RenderContext& rc);

private:
	struct CbScene
	{
		Matrix		viewProjection;
		Vector3		lightDirection;
		float DUMMY;
		Vector3		lightColor;
		float DUMMY;
		Vector3		cameraPosition;
		float DUMMY;
	};

	struct CbSkeleton
	{
		Matrix		boneTransforms[256];
	};

	struct DrawInfo
	{
		ShaderId				shaderId;
		std::shared_ptr<Model>	model;
	};

	struct TransparencyDrawInfo
	{
		ShaderId				shaderId;
		const Model::Mesh*		mesh;
		float					distance;
	};

	std::unique_ptr<Shader>					shaders[static_cast<int>(ShaderId::EnumCount)];
	std::vector<DrawInfo>					drawInfos;
	std::vector<TransparencyDrawInfo>		transparencyDrawInfos;

	Microsoft::WRL::ComPtr<ID3D11Buffer>	sceneConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	skeletonConstantBuffer;
};
