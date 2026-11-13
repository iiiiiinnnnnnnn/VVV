#pragma once
#include <memory>
#include <Dx11Kit/Dx11Kit.h>
#include <d3d11.h>
#include <wrl.h>

#include <vector>

#include "Core/Foundation/Common.h"
#include "Rendering/Core/RenderContext.h"
#include "Rendering/Core/SpriteRenderParams.h"
#include "Rendering/Core/PBRRenderParams.h"
#include "Resource/VMDLModel.h"

enum class DrawPass
{
	Opaque,
	Transparent
};

class Shader
{
public:
	Shader() = default;
	virtual ~Shader() = default;
	virtual void End(const RenderContext& rc) = 0;
	virtual void DrawGUI() {}
	// 戻り値は編集の有無。変更通知・保存は呼び出し側で行う。
	virtual bool DrawGUI(VMDLModel::Material& material) { return false; }
	virtual bool DrawGUI(VMDLModel::VmdlMaterialKeyframe& key) { return false; }

protected:
	std::unique_ptr<Gpu::VertexShader> vertexShader;
	std::unique_ptr<Gpu::PixelShader> pixelShader;
};

class SpriteShader : public Shader
{
public:
	virtual void Begin(const RenderContext& rc) = 0;
  virtual void Update(
	  const RenderContext& rc,
	  ID3D11ShaderResourceView* srv,
	  Vector2 textureSize,
	  const Color& color,
	  const SpriteRenderParams* params)
  {
  }
	static const D3D11_INPUT_ELEMENT_DESC InputLayout[3];
};

class ModelShader : public Shader
{
public:
	virtual ModelShaderId GetId() const = 0;
	virtual const char* GetName() const = 0;
	virtual void Begin(const RenderContext& rc, DrawPass pass) = 0;
	virtual void Update(
		const RenderContext& rc,
		const VMDLModel::Mesh& mesh,
		const PBRRenderParams* params,
		const PBRMaterialParams* materialParams)
	{
	}
	static const D3D11_INPUT_ELEMENT_DESC InputLayout[6];

protected:
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

};
