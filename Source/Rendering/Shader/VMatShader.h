// VMatShader.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Rendering/Shader/Shader.h"

class VMatShader : public ModelShader
{
public:
	VMatShader(ID3D11Device* device);
	~VMatShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(
		const RenderContext& rc,
		const VMDLModel::Mesh& mesh,
		const VMatRenderParams* params) override;
	void End(const RenderContext& rc) override;

private:

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
	Microsoft::WRL::ComPtr<ID3D11Buffer>	shadowMapConstantBuffer;

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

	private:
		int		dummy[3];
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>	materialConstantBuffer;

	// b2
	struct CbDamageHoles
	{
		Vector4 holes[VMatDamageHoleParams::MaxCount]; // xyz: world center, w: radius
		Vector4 directions[VMatDamageHoleParams::MaxCount]; // xyz: world dent direction
		int count;
		float edgeWidth;
		float depth;

	private:
		float dummy;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>	damageHolesConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader>	geometryShader;
};
