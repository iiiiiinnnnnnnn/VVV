#include "Application/SettingsAndDebug/ProfileScope.h"
#include "Rendering/Core/RenderTarget.h"
#include "Rendering/Core/VertexBuffer.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Renderer/SpriteRenderer.h"
#include "Application/SettingsAndDebug/DebugUtil.h"
#include "Rendering/Shader/BasicSpriteShader.h"
#include "Rendering/Shader/DissolveSpriteShader.h"

// 追加
#include "Rendering/Shader/GaussianFilterShader.h"
#include "Rendering/Shader/VignetteSpriteShader.h"

// コンストラクタ
SpriteRenderer::SpriteRenderer(ID3D11Device* device)
{
	// 頂点バッファ生成（DYNAMIC: 毎フレーム Map/Unmap で更新）
	const Gpu::BufferDesc bufferDesc(sizeof(SpriteVertex) * 4,
		D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, bufferDesc, nullptr);

	shaders[static_cast<int>(SpriteShaderId::Basic)] = std::make_unique<BasicSpriteShader>(device);
	shaders[static_cast<int>(SpriteShaderId::Dissolve)] = std::make_unique<DissolveSpriteShader>(device);

	// 追加
	shaders[static_cast<int>(SpriteShaderId::GaussianFilter)] = std::make_unique<GaussianFilterShader>(device);
	shaders[static_cast<int>(SpriteShaderId::Vignette)] =
		std::make_unique<VignetteSpriteShader>(device);
	shaders[static_cast<int>(SpriteShaderId::VignetteOverlay)] =
		std::make_unique<VignetteSpriteShader>(device, true);
}

// 頂点計算（Texture版）
SpriteRenderer::DrawInfo SpriteRenderer::BuildDrawInfo(
	SpriteShaderId shaderId,
	std::shared_ptr<Texture> texture,
	Vector3 dxyz,
	Vector2 dwh,
	Vector2 sxy,
	Vector2 swh,
	float angle,
	const Color& color,
	const SpriteRenderParams* params)
{
	return BuildDrawInfo(
		shaderId,
		texture->GetShaderResourceView().Get(),
		{ static_cast<float>(texture->GetWidth()), static_cast<float>(texture->GetHeight()) },
		dxyz, dwh, sxy, swh, angle, color, params);
}

SpriteRenderer::DrawInfo SpriteRenderer::BuildDrawInfo(
	SpriteShaderId shaderId,
	ID3D11ShaderResourceView* srv,
	Vector2 textureSize,
	Vector3 dxyz,
	Vector2 dwh,
	Vector2 sxy,
	Vector2 swh,
	float angle,
	const Color& color,
	const SpriteRenderParams* params)
{
	DrawInfo info = {};
	info.shaderId = shaderId;
	info.srv = srv;
	info.textureSize = textureSize;
	info.color = color;
	info.params = params;

	// 頂点座標（スクリーン空間）
	DirectX::XMFLOAT2 positions[4] = {
		{ dxyz.x,      dxyz.y      },	// 左上
		{ dxyz.x + dwh.x, dxyz.y      },	// 右上
		{ dxyz.x,      dxyz.y + dwh.y },	// 左下
		{ dxyz.x + dwh.x, dxyz.y + dwh.y },	// 右下
	};

	// テクスチャ座標（UV）
	DirectX::XMFLOAT2 texcoords[4] = {
		{ sxy.x,      sxy.y       },
		{ sxy.x + swh.x, sxy.y       },
		{ sxy.x,      sxy.y + swh.y  },
		{ sxy.x + swh.x, sxy.y + swh.y  },
	};

	// 中心を原点に移動して回転
	float mx = dxyz.x + dwh.x * 0.5f;
	float my = dxyz.y + dwh.y * 0.5f;
	for (auto& p : positions) { p.x -= mx; p.y -= my; }

	float theta = DirectX::XMConvertToRadians(angle);
	float c = cosf(theta), s = sinf(theta);
	for (auto& p : positions)
	{
		DirectX::XMFLOAT2 tmp = p;
		p.x = c * tmp.x - s * tmp.y;
		p.y = s * tmp.x + c * tmp.y;
	}

	for (auto& p : positions) { p.x += mx; p.y += my; }

	// スクリーン座標のまま保存（NDC 変換は Render() 時にビューポートを使って行う）
	for (int i = 0; i < 4; ++i)
	{
		info.vertices[i].alpha = alpha;
		info.vertices[i].position = Vector3::Transform(Vector3(positions[i].x, positions[i].y, dxyz.z), transform);
		info.vertices[i].texcoord = {
			texcoords[i].x / textureSize.x,
			texcoords[i].y / textureSize.y
		};
	}

	return info;
}

void SpriteRenderer::Draw(
	SpriteShaderId shaderId,
	std::shared_ptr<Texture> texture,
	Vector3 dxyz,
	Vector2 size,
	Vector2 sxy,
	Vector2 swh,
	float angle,
	const Color& color,
	const SpriteRenderParams* params)
{
	drawCalls.push_back(BuildDrawInfo(shaderId, texture, dxyz, size, sxy, swh, angle, color, params));
}

void SpriteRenderer::Render(const RenderContext& rc)
{
	PROFILE_FUNCTION();
	if (drawCalls.empty()) return;

	ID3D11DeviceContext* dc = rc.deviceContext;

	// ビューポート取得（NDC 変換に使う）
	const auto viewport = RenderTarget::GetViewport(dc);
	float screenWidth = viewport.Width;
	float screenHeight = viewport.Height;

	// レンダーステート設定
	rc.graphics->TransparentBlendState->Bind(dc);
	rc.graphics->DepthDisabledState->Bind(dc);
	rc.graphics->SolidRasterizerState->Bind(dc);

	// プリミティブトポロジー・頂点バッファ設定
	UINT stride = sizeof(SpriteVertex);
	UINT offset = 0;
	Gpu::VertexBuffer::SetTopology(dc, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	vertexBuffer->Bind(dc, stride, 0, offset);

	SpriteShader* currentShader = nullptr;

	for (DrawInfo& call : drawCalls)
	{
		SpriteShader* shader = shaders[static_cast<int>(call.shaderId)].get();

		// シェーダーが変わったら Begin/End を切り替え
		if (shader != currentShader)
		{
			if (currentShader) currentShader->End(rc);
			shader->Begin(rc);
			currentShader = shader;
		}

		// NDC 変換
		SpriteVertex verts[4];
		memcpy(verts, call.vertices, sizeof(verts));
		for (auto& v : verts)
		{
			v.position.x = 2.0f * v.position.x / screenWidth - 1.0f;
			v.position.y = 1.0f - 2.0f * v.position.y / screenHeight;
		}

		// 頂点バッファ更新
		vertexBuffer->Update(dc, verts, static_cast<UINT>(sizeof(verts)));

		shader->Update(rc, call.srv.Get(), call.textureSize, call.color, call.params);

		// 描画
		vertexBuffer->Draw(dc, 4);
	}

	if (currentShader) currentShader->End(rc);

	drawCalls.clear();
}
