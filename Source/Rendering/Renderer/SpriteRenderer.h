#pragma once
#include <Dx11Kit/Dx11Kit.h>
#include <d3d11.h>
#include <wrl.h>

#include <algorithm>
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
	Dissolve,

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
	void SetAlpha(float value) { alpha = std::clamp(value, 0.0f, 1.0f); }
	float GetAlpha() const { return alpha; }
	void SetTransform(const Matrix& value) { transform = value; }
	const Matrix& GetTransform() const { return transform; }

private:
	struct CbSpriteColor
	{
		float decodeSrgb;
		Vector3 padding;
	};
	std::unique_ptr<Gpu::ConstantBuffer> colorConstantBuffer;
	Matrix transform = Matrix::Identity;
	float alpha = 1.0f;
	struct SpriteVertex
	{
		Vector3	position;
		Vector2	texcoord;
		float alpha = 1.0f;
	};

	struct DrawInfo
	{
		SpriteShaderId										shaderId;
		SpriteVertex										vertices[4];
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	srv;
		Vector2 											textureSize;
		Color												color;
		float decodeSrgb;
		const SpriteRenderParams*					params;
	};
	DrawInfo BuildDrawInfo(SpriteShaderId shaderId, std::shared_ptr<Texture> texture, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const Color& color, const SpriteRenderParams* params);
	DrawInfo BuildDrawInfo(SpriteShaderId shaderId, ID3D11ShaderResourceView* srv, Vector2 textureSize, Vector3 dxyz, Vector2 dwh, Vector2 sxy, Vector2 swh, float angle, const Color& color, const SpriteRenderParams* params);

	std::unique_ptr<SpriteShader>	shaders[static_cast<int>(SpriteShaderId::EnumCount)];
	std::vector<DrawInfo>			drawCalls;
	std::unique_ptr<Gpu::VertexBuffer> vertexBuffer;
};
