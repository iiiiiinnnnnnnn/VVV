#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "Common.h"
#include "Shader.h"
#include "Texture.h"

enum class SpriteShaderId
{
	Basic,
	GaussianFilter,
	Vignette,
	ThreatenLine,

	EnumCount
};

class SpriteRenderer
{
public:
	SpriteRenderer(ID3D11Device* device);
	~SpriteRenderer() {}

	void Draw(SpriteShaderId shaderId, std::shared_ptr<Texture> texture, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const ShaderParamList& shaderParams);

	void Render(const RenderContext& rc);

private:
	struct SpriteVertex
	{
		Vector3	position;
		Vector2	texcoord;
	};

	struct DrawInfo
	{
		SpriteShaderId										shaderId;
		SpriteVertex										vertices[4];
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	srv;
		Vector2 											textureSize;
		ShaderParamList										shaderParams;
	};
	DrawInfo BuildDrawInfo(SpriteShaderId shaderId, std::shared_ptr<Texture> texture, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const ShaderParamList& shaderParams);
	DrawInfo BuildDrawInfo(SpriteShaderId shaderId, ID3D11ShaderResourceView* srv, Vector2 textureSize, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const ShaderParamList& shaderParams);

	std::unique_ptr<SpriteShader>	shaders[static_cast<int>(SpriteShaderId::EnumCount)];
	std::vector<DrawInfo>			drawCalls;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
};
