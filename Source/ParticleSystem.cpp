// ParticleSystem.cpp

#include "ParticleSystem.h"
#include "GpuResourceUtils.h"
#include "GameTime.h"

ParticleSystem::ParticleSystem(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shaderResourceView, int komax, int komay, int num)
	: komax(komax)
	, komay(komay)
	, numParticles(num)
{
	HRESULT hr;

	//	パーティクル情報リスト
	data = new ParticleData[num];
	ZeroMemory(data, sizeof(ParticleData) * num);

	//	頂点情報リスト
	vertices = new Vertex[num];
	ZeroMemory(vertices, sizeof(Vertex) * num);

	for (int i = 0; i < numParticles; i++) { data[i].type = -1; }

	//	パーティクル用画像ロード
	this->shaderResourceView = shaderResourceView;

	//	頂点バッファ作成
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	//	頂点数分のバッファ
	bd.ByteWidth = sizeof(Vertex) * numParticles;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	hr = device->CreateBuffer(&bd, NULL, vertexBuffer.GetAddressOf());
	assert(SUCCEEDED(hr));

	//	定数バッファ生成
	D3D11_BUFFER_DESC cbd;
	ZeroMemory(&cbd, sizeof(cbd));
	cbd.Usage = D3D11_USAGE_DEFAULT;
	cbd.ByteWidth = sizeof(Constants);
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.CPUAccessFlags = 0;

	hr = device->CreateBuffer(&cbd, nullptr, constantBuffer.GetAddressOf());
	assert(SUCCEEDED(hr));

	//	頂点シェーダー
	std::vector<D3D11_INPUT_ELEMENT_DESC> InputElementDesc
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "PARAMETER", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	GpuResourceUtils::LoadVertexShader(
		device,
		"GeometryParticleVS.cso",
		InputElementDesc.data(),
		InputElementDesc.size(),
		inputLayout.ReleaseAndGetAddressOf(),
		vertexShader.ReleaseAndGetAddressOf());

	GpuResourceUtils::LoadGeometryShader(
		device,
		"GeometryParticleGS.cso",
		geometryShader.ReleaseAndGetAddressOf());

	GpuResourceUtils::LoadPixelShader(
		device,
		"GeometryParticlePS.cso",
		pixelShader.ReleaseAndGetAddressOf());
}

ParticleSystem::~ParticleSystem()
{
	delete[] data;
	delete[] vertices;
}

void ParticleSystem::Update()
{
	for (int i = 0; i < numParticles; i++) {
		if (data[i].type < 0) continue;

		data[i].vx += data[i].ax * Game::Time::deltaTime;
		data[i].vy += data[i].ay * Game::Time::deltaTime;
		data[i].vz += data[i].az * Game::Time::deltaTime;

		data[i].x += data[i].vx * Game::Time::deltaTime;
		data[i].y += data[i].vy * Game::Time::deltaTime;
		data[i].z += data[i].vz * Game::Time::deltaTime;

		data[i].timer -= Game::Time::deltaTime;
		data[i].alpha = sqrtf(data[i].timer);
		// アニメ
		if (data[i].anime)
			data[i].type += Game::Time::deltaTime * data[i].animeSpeed;	// speedコマ/秒

		// 終了判定
		if (data[i].timer <= 0)
			data[i].type = -1;
	}
}

void ParticleSystem::Render(ID3D11DeviceContext* immediateContext)
{
	//定数バッファの更新
	Constants cb;
	cb.size = { 0.1f,0.1f };
	immediateContext->UpdateSubresource(constantBuffer.Get(), 0, nullptr, &cb, 0, 0);
	immediateContext->VSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
	immediateContext->GSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());
	immediateContext->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

	//	点描画設定
	immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	//	シェーダー設定
	immediateContext->VSSetShader(vertexShader.Get(), nullptr, 0);
	immediateContext->GSSetShader(geometryShader.Get(), nullptr, 0);
	immediateContext->PSSetShader(pixelShader.Get(), nullptr, 0);

	//	入力レイアウト設定
	immediateContext->IASetInputLayout(inputLayout.Get());

	//	テクスチャ設定
	immediateContext->PSSetShaderResources(0, 1, shaderResourceView.GetAddressOf());

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
		vertices[n].color.x =
			vertices[n].color.y =
			vertices[n].color.z = 1.0f;
		vertices[n].color.w = data[i].alpha;

		vertices[n].param.x = 0;
		vertices[n].param.y = data[i].type;
		vertices[n].param.z = (float)komax;	//	横コマ数
		vertices[n].param.w = (float)komay;	//	縦コマ数
		++n;
	}
	//	頂点データ更新
	immediateContext->UpdateSubresource(vertexBuffer.Get(), 0, nullptr, vertices, 0, 0);

	//	バーテックスバッファーをセット
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	immediateContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);

	//	パーティクル情報分描画コール
	immediateContext->Draw(n, 0);

	//	シェーダ無効化
	immediateContext->VSSetShader(nullptr, nullptr, 0);
	immediateContext->GSSetShader(nullptr, nullptr, 0);
	immediateContext->PSSetShader(nullptr, nullptr, 0);
}

void ParticleSystem::Set(int type, float timer, Vector3 p, Vector3 v, Vector3 f, Vector2 size, bool anime, float animeSpeed)
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
		data[i].timer = timer;
		data[i].anime = anime;
		data[i].animeSpeed = animeSpeed;
		break;
	}
}