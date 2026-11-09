#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>
#include <d3d11.h>
#include <wrl.h>

#include "Rendering/Shader/Shader.h"

class PBRShader : public ModelShader
{
public:
	PBRShader(ID3D11Device* device);
	~PBRShader() override = default;

	ModelShaderId GetId() const override { return ModelShaderId::PBR; }
	const char* GetName() const override { return "PBR"; }
	bool DrawGUI(VMDLModel::Material& material) override;
	bool DrawGUI(VMDLModel::VmdlMaterialKeyframe& key) override;

	void Begin(const RenderContext& rc, DrawPass pass) override;
	void Update(
		const RenderContext& rc,
		const VMDLModel::Mesh& mesh,
		const PBRRenderParams* params) override;
	void End(const RenderContext& rc) override;

private:
	DrawPass drawPass = DrawPass::Opaque;

	// b0
	struct CbShadowMap
	{
		Matrix	lightViewProjections[ShadowMapData::CascadeCount];
		Vector4	cascadeSplits;
		Vector4	cameraFront;
		Color	shadowColor;			// 影の色
		float	shadowBias;				// 深度比較用のオフセット値
		int		pcfKernelSize;			// ソフトシャドウの行列サイズ

	private:
		float	dummy[2];
	};
	std::unique_ptr<Gpu::ConstantBuffer> shadowMapConstantBuffer;

	// b1
	struct CbMaterial
	{
		Color	baseColor;
		Color	emissiveColor;
		Color	emissionColor;
		Color	fresnelColor;

		float	metalness;
		float	roughness;
		float	occlusion;
		float	occlusionStrength;

		float	shadowStrength;
		float	fresnelPower;
		float	fresnelStrength;
		int		useMetalnessTexture;

		int		useRoughnessTexture;
		int		useOcclusionTexture;
		int 	useEmissiveTexture;
		int		isFlatShading;

		int		useBaseColorTexture;
		float	dissolveAmount;
		float	materialDummy[2];

		float	transmission;
		float	indexOfRefraction;
		float	refractionDistance;
		float	transmissionDummy;

		Vector3 vivraDir;
		float	vivraPower;
	};
	std::unique_ptr<Gpu::ConstantBuffer> materialConstantBuffer;

	std::unique_ptr<Gpu::GeometryShader> geometryShader;
	std::unique_ptr<Gpu::PixelShader> unlitPixelShader;
	std::unique_ptr<Gpu::PixelShader> justDodgeUnlitPixelShader;
	std::unique_ptr<Gpu::ConstantBuffer> justDodgeFadeConstantBuffer;
};
