// PBRShader.h

#pragma once
#include <d3d11.h>
#include <wrl.h>

#include "Rendering/Shader/Shader.h"

class PBRShader : public ModelShader
{
public:
	PBRShader(ID3D11Device* device);
	~PBRShader() override = default;

	void Begin(const RenderContext& rc) override;
	void Update(const RenderContext& rc, const VMDLModel::Mesh& mesh) override;
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
		float	DUMMY[2];
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
		int		isFlatShading;
		float	DUMMY;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>	materialConstantBuffer;

	static constexpr int MaxDamageHoles = 8;

	// b2
	struct CbDamageHoles
	{
		Vector4 holes[MaxDamageHoles]; // xyz: world center, w: radius
		Vector4 holeDirections[MaxDamageHoles]; // xyz: world dent direction
		int holeCount;
		float edgeWidth;
		float depth;
		float DUMMY;
	};
	Microsoft::WRL::ComPtr<ID3D11Buffer>	damageHolesConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D11GeometryShader>	geometryShader;
};

