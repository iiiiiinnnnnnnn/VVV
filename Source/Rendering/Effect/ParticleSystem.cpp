#include "Rendering/Core/GeometryShader.h"
#include "Rendering/Core/VertexBuffer.h"
#include "Rendering/Core/Graphics.h"
#include "Rendering/Core/PixelShader.h"
#include "Rendering/Core/VertexShader.h"
#include "Application/SettingsAndDebug/ProfileScope.h"
#include <algorithm>

#include "Rendering/Effect/ParticleSystem.h"
#include "Application/Time/GameTime.h"

ParticleSystem::ParticleSystem(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView, int komax, int komay, int num)
	: komax(komax)
	, komay(komay)
	, numParticles(num)
{
	//	パーティクル情報リスト
	data = new ParticleData[num]{};

	//	頂点情報リスト
	vertices = new Vertex[num];
	ZeroMemory(vertices, sizeof(Vertex) * num);

	for (int i = 0; i < numParticles; i++) { data[i].type = -1; }

	//	パーティクル用画像ロード
	this->shaderResourceView = shaderResourceView;

	//	頂点バッファ作成
	const Gpu::BufferDesc bd(sizeof(Vertex) * numParticles, D3D11_BIND_VERTEX_BUFFER);

	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, bd);

	//	定数バッファ生成
	const Gpu::BufferDesc cbd(sizeof(Constants), D3D11_BIND_CONSTANT_BUFFER);

	constantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, cbd, nullptr);

	//	頂点シェーダー
	Gpu::InputLayoutDesc InputElementDesc;
	InputElementDesc.AddPosition()
		.AddTexcoord()
		.AddColor()
		.Add("PARAMETER", DXGI_FORMAT_R32G32B32A32_FLOAT);

	vertexShader = std::make_unique<Gpu::VertexShader>(device, "Resources/Shader/GeometryParticleVS.cso",
		InputElementDesc);

	geometryShader = std::make_unique<Gpu::GeometryShader>(device, "Resources/Shader/GeometryParticleGS.cso");

	pixelShader = std::make_unique<Gpu::PixelShader>(device, "Resources/Shader/GeometryParticlePS.cso");
}

ParticleSystem::~ParticleSystem()
{
	delete[] data;
	delete[] vertices;
}

void ParticleSystem::Update()
{
	PROFILE_FUNCTION();
	for (int i = 0; i < numParticles; i++) {
		if (data[i].type < 0) continue;

		data[i].vx += data[i].ax * Game::Time::deltaTime;
		data[i].vy += data[i].ay * Game::Time::deltaTime;
		data[i].vz += data[i].az * Game::Time::deltaTime;

		data[i].x += data[i].vx * Game::Time::deltaTime;
		data[i].y += data[i].vy * Game::Time::deltaTime;
		data[i].z += data[i].vz * Game::Time::deltaTime;

		data[i].lifetime.Update();
		if (data[i].fadeInDuration > 0.0f || data[i].fadeOutDuration > 0.0f)
		{
			const float age = data[i].lifetime.GetElapsed();
			const float fadeIn = data[i].fadeInDuration > 0.0f
				? std::clamp(age / data[i].fadeInDuration, 0.0f, 1.0f)
				: 1.0f;
			const float fadeOut = data[i].fadeOutDuration > 0.0f
				? std::clamp(data[i].lifetime.GetRemaining() / data[i].fadeOutDuration, 0.0f, 1.0f)
				: 1.0f;
			data[i].alpha = std::min(fadeIn, fadeOut);
		}
		else
		{
			// フェード未指定なら、寿命の長さに関係なく指定された色の不透明度を保つ。
			data[i].alpha = 1.0f;
		}
		// アニメ
		if (data[i].anime)
		{
			data[i].type += Game::Time::deltaTime * data[i].animeSpeed;	// speedコマ/秒
			const float frameCount = static_cast<float>(std::max(1, komax * komay));
			while (data[i].type >= frameCount) data[i].type -= frameCount;
			while (data[i].type < 0.0f) data[i].type += frameCount;
		}

		// 終了判定
		if (data[i].lifetime.GetRemaining() <= 0)
			data[i].type = -1;
	}
}

void ParticleSystem::Render(const RenderContext& rc)
{
	PROFILE_FUNCTION();
	ID3D11DeviceContext* immediateContext = rc.deviceContext;

	//定数バッファの更新
	Constants cb{};
	cb.viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	cb.cameraRight = rc.camera->GetRight();
	cb.cameraUp = rc.camera->GetUp();
	constantBuffer->Update(immediateContext, &cb);
	constantBuffer->BindVS(immediateContext, 0);
	constantBuffer->BindGS(immediateContext, 0);
	constantBuffer->BindPS(immediateContext, 0);

	//	点描画設定
	Gpu::VertexBuffer::SetTopology(immediateContext, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	//	シェーダー設定
	vertexShader->Bind(immediateContext);
	geometryShader->Bind(immediateContext);
	pixelShader->Bind(immediateContext);

	//	入力レイアウト設定

	//	テクスチャ設定
	Gpu::PixelShader::SetShaderResources(immediateContext, 0, shaderResourceView.Get());
	rc.graphics->LinearWrapSamplerState->BindPS(immediateContext, 0);

	//	パーティクル情報を頂点バッファに転送
	int n = 0; //パーティクル発生数
	for (int i = 0; i < numParticles; i++)
	{
		if (data[i].type < 0) continue;

		vertices[n].position.x = data[i].x;
		vertices[n].position.y = data[i].y;
		vertices[n].position.z = data[i].z;
		vertices[n].texcoord.x = data[i].w;
		vertices[n].texcoord.y = data[i].h;
		vertices[n].color = data[i].color;
		vertices[n].color.w *= data[i].alpha;

		vertices[n].param.x = 0;
		vertices[n].param.y = data[i].type;
		vertices[n].param.z = (float)komax;	//	横コマ数
		vertices[n].param.w = (float)komay;	//	縦コマ数
		++n;
	}
	//	頂点データ更新
	vertexBuffer->Update(immediateContext, vertices);

	//	バーテックスバッファーをセット
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	vertexBuffer->Bind(immediateContext, stride, 0, offset);

	//	パーティクル情報分描画コール
	vertexBuffer->Draw(immediateContext, n);

	Gpu::PixelShader::UnbindShaderResources(immediateContext, 0);
	Gpu::SamplerState::UnbindPS(immediateContext, 0);

	//	シェーダ無効化
	Gpu::VertexShader::Unbind(immediateContext);
	Gpu::GeometryShader::Unbind(immediateContext);
	Gpu::PixelShader::Unbind(immediateContext);
}

void ParticleSystem::Set(
	int type,
	float timer,
	Vector3 p,
	Vector3 v,
	Vector3 f,
	Vector2 size,
	bool anime,
	float animeSpeed,
	Color color,
	float fadeInDuration,
	float fadeOutDuration)
{
	for (int i = 0; i < numParticles; i++)
	{
		if (data[i].type >= 0) continue;
		data[i].type = (float)type;
		data[i].x = p.x;
		data[i].y = p.y;
		data[i].z = p.z;
		data[i].vx = v.x;
		data[i].vy = v.y;
		data[i].vz = v.z;
		data[i].ax = f.x;
		data[i].ay = f.y;
		data[i].az = f.z;
		data[i].w = size.x;
		data[i].h = size.y;
		data[i].alpha = 1.0f;
		data[i].fadeInDuration = fadeInDuration;
		data[i].fadeOutDuration = fadeOutDuration;
		data[i].color = color;
		data[i].lifetime.Reset(timer);
		data[i].anime = anime;
		data[i].animeSpeed = animeSpeed;
		break;
	}
}
