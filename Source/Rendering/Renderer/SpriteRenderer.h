#pragma once
#include <d3d11.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "Core/Foundation/Common.h"
#include "Rendering/Shader/Shader.h"
#include "Resource/Texture.h"

enum class SpriteShaderId
{
	Basic,
	GaussianFilter,
	Vignette,
	VignetteOverlay,

	EnumCount
};

class SpriteRenderer
{
public:
	SpriteRenderer(ID3D11Device* device);
	~SpriteRenderer() {}

	void Draw(
		SpriteShaderId shaderId,
		std::shared_ptr<Texture> texture,
		Vector3 dxyz,
		Vector2 dwh,
		Vector2 sxy,
		Vector2 swh,
		float angle,
		const Color& color = Color(1, 1, 1, 1),
		const SpriteRenderParams* params = nullptr);

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
		Color												color;
		const SpriteRenderParams*					params;
	};
	DrawInfo BuildDrawInfo(SpriteShaderId shaderId, std::shared_ptr<Texture> texture, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const Color& color, const SpriteRenderParams* params);
	DrawInfo BuildDrawInfo(SpriteShaderId shaderId, ID3D11ShaderResourceView* srv, Vector2 textureSize, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const Color& color, const SpriteRenderParams* params);

	std::unique_ptr<SpriteShader>	shaders[static_cast<int>(SpriteShaderId::EnumCount)];
	std::vector<DrawInfo>			drawCalls;
	Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
};
