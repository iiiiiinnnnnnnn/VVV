// Terrain.cpp

#include "Terrain.h"

#include "Actor.h"
#include "Graphics.h"
#include "GpuResourceUtils.h"
#include "Input.h"
#include "Misc.h"

namespace
{
	std::vector<uint8_t> LoadBinaryFile(const char* filename)
	{
		std::ifstream file(filename, std::ios::binary | std::ios::ate);
		_ASSERT_EXPR_A(file.is_open(), filename);

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(static_cast<size_t>(size));
		file.read(reinterpret_cast<char*>(buffer.data()), size);

		return buffer;
	}

	void LoadHullShader(ID3D11Device* device, const char* filename, ID3D11HullShader** shader)
	{
		std::vector<uint8_t> data = LoadBinaryFile(filename);

		HRESULT hr = device->CreateHullShader(
			data.data(),
			data.size(),
			nullptr,
			shader);

		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	void LoadDomainShader(ID3D11Device* device, const char* filename, ID3D11DomainShader** shader)
	{
		std::vector<uint8_t> data = LoadBinaryFile(filename);

		HRESULT hr = device->CreateDomainShader(
			data.data(),
			data.size(),
			nullptr,
			shader);

		_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
	}

	void LoadTextureOrDummy(
		ID3D11Device* device,
		const char* filename,
		UINT dummyColor,
		ID3D11ShaderResourceView** shaderResourceView)
	{
		if (std::filesystem::exists(filename))
		{
			GpuResourceUtils::LoadTexture(device, filename, shaderResourceView);
		}
		else
		{
			GpuResourceUtils::CreateDummyTexture(device, dummyColor, shaderResourceView);
		}
	}
}

Terrain::Terrain(Object* owner)
	: Component(owner)
{
	Component::GetOwnerAsActor();

	InitializeGpuResources();
	ClearTerrainTexture();
}

void Terrain::InitializeGpuResources()
{
	ID3D11Device* device = Game::Graphics::Instance().GetDevice();

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTerrainObject),
		terrainObjectConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTerrainScene),
		terrainSceneConstantBuffer.GetAddressOf());

	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbTessellation),
		tesselationConstantBuffer.GetAddressOf());

	CreateGridMesh(device);
	CreateTerrainTexture(device);

	D3D11_INPUT_ELEMENT_DESC inputElementDescs[]
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/TerrainPrimitiveVS.cso",
		inputElementDescs,
		_countof(inputElementDescs),
		terrainInputLayout.GetAddressOf(),
		terrainVertexShader.GetAddressOf());

	LoadHullShader(
		device,
		"Data/Shader/TerrainPrimitiveHS.cso",
		terrainHullShader.GetAddressOf());

	LoadDomainShader(
		device,
		"Data/Shader/TerrainPrimitiveDS.cso",
		terrainDomainShader.GetAddressOf());

	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/TerrainPrimitivePS.cso",
		terrainPixelShader.GetAddressOf());

	LoadTextureOrDummy(
		device,
		"Data/Terrain/layered-rock1-albedo.png",
		0xFF777777,
		terrainBaseColorShaderResourceView[0].GetAddressOf());

	LoadTextureOrDummy(
		device,
		"Data/Terrain/rocky_dirt1-albedo.png",
		0xFF2E4A6B,
		terrainBaseColorShaderResourceView[1].GetAddressOf());

	LoadTextureOrDummy(
		device,
		"Data/Terrain/wispy-grass-meadow_albedo.png",
		0xFF3A8A4A,
		terrainBaseColorShaderResourceView[2].GetAddressOf());
}

void Terrain::CreateGridMesh(ID3D11Device* device)
{
	if (gridResolution < 1)
	{
		gridResolution = 1;
	}

	std::vector<TerrainVertex> vertices;
	std::vector<uint32_t> indices;

	const int vertexLineCount = gridResolution + 1;
	vertices.reserve(vertexLineCount * vertexLineCount);
	indices.reserve(gridResolution * gridResolution * 6);

	for (int z = 0; z <= gridResolution; ++z)
	{
		for (int x = 0; x <= gridResolution; ++x)
		{
			float u = static_cast<float>(x) / static_cast<float>(gridResolution);
			float v = static_cast<float>(z) / static_cast<float>(gridResolution);

			TerrainVertex vertex{};
			vertex.position = {
				(u - 0.5f) * terrainSize,
				0.0f,
				(v - 0.5f) * terrainSize
			};
			vertex.normal = Vector3::UnitY;
			vertex.texcoord = {u, v};

			vertices.push_back(vertex);
		}
	}

	for (int z = 0; z < gridResolution; ++z)
	{
		for (int x = 0; x < gridResolution; ++x)
		{
			uint32_t i0 = static_cast<uint32_t>(z * vertexLineCount + x);
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + static_cast<uint32_t>(vertexLineCount);
			uint32_t i3 = i2 + 1;

			indices.push_back(i0);
			indices.push_back(i2);
			indices.push_back(i1);

			indices.push_back(i1);
			indices.push_back(i2);
			indices.push_back(i3);
		}
	}

	indexCount = static_cast<UINT>(indices.size());

	D3D11_BUFFER_DESC vertexBufferDesc{};
	vertexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(TerrainVertex) * vertices.size());
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	HRESULT hr = device->CreateBuffer(
		&vertexBufferDesc,
		&vertexData,
		vertexBuffer.ReleaseAndGetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	D3D11_BUFFER_DESC indexBufferDesc{};
	indexBufferDesc.ByteWidth = static_cast<UINT>(sizeof(uint32_t) * indices.size());
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	hr = device->CreateBuffer(
		&indexBufferDesc,
		&indexData,
		indexBuffer.ReleaseAndGetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void Terrain::CreateTerrainTexture(ID3D11Device* device)
{
	terrainPixels.resize(TerrainTextureWidth * TerrainTextureHeight);

	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = TerrainTextureWidth;
	desc.Height = TerrainTextureHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = terrainPixels.data();
	data.SysMemPitch = sizeof(Vector4) * TerrainTextureWidth;

	HRESULT hr = device->CreateTexture2D(
		&desc,
		&data,
		terrainTexture.GetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

	hr = device->CreateShaderResourceView(
		terrainTexture.Get(),
		nullptr,
		terrainTextureShaderResourceView.GetAddressOf());

	_ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
}

void Terrain::UploadTerrainTexture(ID3D11DeviceContext* dc)
{
	if (!terrainTextureDirty)
	{
		return;
	}

	dc->UpdateSubresource(
		terrainTexture.Get(),
		0,
		nullptr,
		terrainPixels.data(),
		sizeof(Vector4) * TerrainTextureWidth,
		0);

	terrainTextureDirty = false;
}

void Terrain::ClearTerrainTexture()
{
	for (Vector4& pixel : terrainPixels)
	{
		pixel.x = terrain_texture_clear_color.x;
		pixel.y = terrain_texture_clear_color.y;
		pixel.z = terrain_texture_clear_color.z;
		pixel.w = terrain_texture_clear_color.w;
	}

	terrainTextureDirty = true;
	is_terrain_texture_clear_color = false;
}

void Terrain::Update()
{
}

void Terrain::Render(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	if (is_terrain_texture_clear_color)
	{
		ClearTerrainTexture();
	}

	PaintByMouse(rc);
	UploadTerrainTexture(dc);

	Actor* actor = GetOwnerAsActor();

	CbTerrainObject cbObject{};
	cbObject.world = actor->transform.matrix;
	cbObject.terrainSize = terrainSize;
	cbObject.heightMapTexelSize = 1.0f / static_cast<float>(TerrainTextureWidth);

	dc->UpdateSubresource(
		terrainObjectConstantBuffer.Get(),
		0,
		nullptr,
		&cbObject,
		0,
		0);

	CbTerrainScene cbScene{};
	cbScene.viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	cbScene.viewPosition = rc.camera->GetEye();
	cbScene.directionalLightDirection = rc.lightData.GetDirectionalLight().direction;
	cbScene.directionalLightColor = rc.lightData.GetDirectionalLight().color;
	cbScene.ambientColor = rc.lightData.GetAmbientColor();

	dc->UpdateSubresource(
		terrainSceneConstantBuffer.Get(),
		0,
		nullptr,
		&cbScene,
		0,
		0);

	dc->UpdateSubresource(
		tesselationConstantBuffer.Get(),
		0,
		nullptr,
		&tesselation_constant,
		0,
		0);

	dc->OMSetBlendState(
		rc.renderState->GetBlendState(BlendState::Opaque),
		nullptr,
		0xFFFFFFFF);

	dc->OMSetDepthStencilState(
		rc.renderState->GetDepthStencilState(DepthState::TestAndWrite),
		0);

	dc->RSSetState(
		rc.renderState->GetRasterizerState(
		use_wire ? RasterizerState::WireCullNone : RasterizerState::SolidCullNone));

	UINT stride = sizeof(TerrainVertex);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffers[] = {vertexBuffer.Get()};

	dc->IASetInputLayout(terrainInputLayout.Get());
	dc->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
	dc->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);

	dc->VSSetShader(terrainVertexShader.Get(), nullptr, 0);
	dc->HSSetShader(terrainHullShader.Get(), nullptr, 0);
	dc->DSSetShader(terrainDomainShader.Get(), nullptr, 0);
	dc->PSSetShader(terrainPixelShader.Get(), nullptr, 0);

	ID3D11Buffer* objectCbs[] = {terrainObjectConstantBuffer.Get()};
	ID3D11Buffer* tessCbs[] = {tesselationConstantBuffer.Get()};
	ID3D11Buffer* sceneCbs[] = {terrainSceneConstantBuffer.Get()};

	dc->VSSetConstantBuffers(0, 1, objectCbs);
	dc->DSSetConstantBuffers(0, 1, objectCbs);

	dc->HSSetConstantBuffers(2, 1, tessCbs);
	dc->DSSetConstantBuffers(2, 1, tessCbs);
	dc->PSSetConstantBuffers(2, 1, tessCbs);

	dc->DSSetConstantBuffers(7, 1, sceneCbs);
	dc->PSSetConstantBuffers(7, 1, sceneCbs);

	ID3D11ShaderResourceView* terrainSrvs[] =
	{
		terrainTextureShaderResourceView.Get()
	};

	dc->DSSetShaderResources(0, 1, terrainSrvs);
	dc->PSSetShaderResources(0, 1, terrainSrvs);

	ID3D11ShaderResourceView* baseColorSrvs[] =
	{
		terrainBaseColorShaderResourceView[0].Get(),
		terrainBaseColorShaderResourceView[1].Get(),
		terrainBaseColorShaderResourceView[2].Get(),
	};

	dc->PSSetShaderResources(20, _countof(baseColorSrvs), baseColorSrvs);

	ID3D11SamplerState* samplerStates[] =
	{
		rc.renderState->GetSamplerState(SamplerState::PointClamp),
		rc.renderState->GetSamplerState(SamplerState::LinearClamp),
		rc.renderState->GetSamplerState(SamplerState::LinearWrap),
	};

	dc->DSSetSamplers(0, _countof(samplerStates), samplerStates);
	dc->PSSetSamplers(0, _countof(samplerStates), samplerStates);

	dc->DrawIndexed(indexCount, 0, 0);

	ID3D11Buffer* nullBuffer = nullptr;
	dc->VSSetConstantBuffers(0, 1, &nullBuffer);
	dc->HSSetConstantBuffers(2, 1, &nullBuffer);
	dc->DSSetConstantBuffers(0, 1, &nullBuffer);
	dc->DSSetConstantBuffers(2, 1, &nullBuffer);
	dc->DSSetConstantBuffers(7, 1, &nullBuffer);
	dc->PSSetConstantBuffers(2, 1, &nullBuffer);
	dc->PSSetConstantBuffers(7, 1, &nullBuffer);

	ID3D11ShaderResourceView* nullSrv = nullptr;
	ID3D11ShaderResourceView* nullSrvs3[] = {nullptr, nullptr, nullptr};
	dc->DSSetShaderResources(0, 1, &nullSrv);
	dc->PSSetShaderResources(0, 1, &nullSrv);
	dc->PSSetShaderResources(20, 3, nullSrvs3);

	ID3D11SamplerState* nullSamplers3[] = {nullptr, nullptr, nullptr};
	dc->DSSetSamplers(0, 3, nullSamplers3);
	dc->PSSetSamplers(0, 3, nullSamplers3);

	dc->VSSetShader(nullptr, nullptr, 0);
	dc->HSSetShader(nullptr, nullptr, 0);
	dc->DSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);
}

void Terrain::PaintByMouse(const RenderContext& rc)
{
	if (!use_brush)
	{
		return;
	}

	if (!Game::Input::IsFocusedWindow())
	{
		return;
	}

	Mouse& mouse = Game::Input::Instance().GetMouse();

	if ((mouse.GetButton() & Mouse::BTN_LEFT) == 0)
	{
		return;
	}

	float u = 0.0f;
	float v = 0.0f;
	if (!ScreenToTerrainUV(rc, u, v))
	{
		return;
	}

	float heightSign = 1.0f;
	if (::GetAsyncKeyState(VK_SHIFT) & 0x8000)
	{
		heightSign = -1.0f;
	}

	ApplyBrush(u, v, heightSign);
}

bool Terrain::ScreenToTerrainUV(const RenderContext& rc, float& outU, float& outV) const
{
	Mouse& mouse = Game::Input::Instance().GetMouse();

	float screenWidth = Game::Graphics::ScreenWidth;
	float screenHeight = Game::Graphics::ScreenHeight;

	if (screenWidth <= 0.0f || screenHeight <= 0.0f)
	{
		return false;
	}

	float ndcX = (2.0f * static_cast<float>(mouse.GetPositionX()) / screenWidth) - 1.0f;
	float ndcY = 1.0f - (2.0f * static_cast<float>(mouse.GetPositionY()) / screenHeight);

	Matrix viewProjection = rc.camera->GetView() * rc.camera->GetProjection();
	Matrix invViewProjection = viewProjection.Invert();

	Vector3 nearPoint = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), invViewProjection);
	Vector3 farPoint = Vector3::Transform(Vector3(ndcX, ndcY, 1.0f), invViewProjection);

	Vector3 rayOriginWorld = nearPoint;
	Vector3 rayDirectionWorld = farPoint - nearPoint;
	rayDirectionWorld.Normalize();

	Actor* actor = const_cast<Terrain*>(this)->GetOwnerAsActor();
	Matrix invWorld = actor->transform.matrix.Invert();

	Vector3 rayOriginLocal = Vector3::Transform(rayOriginWorld, invWorld);
	Vector3 rayDirectionLocal = Vector3::TransformNormal(rayDirectionWorld, invWorld);
	rayDirectionLocal.Normalize();

	if (fabsf(rayDirectionLocal.y) < eps)
	{
		return false;
	}

	float t = -rayOriginLocal.y / rayDirectionLocal.y;
	if (t < 0.0f)
	{
		return false;
	}

	Vector3 hitLocal = rayOriginLocal + rayDirectionLocal * t;

	float u = hitLocal.x / terrainSize + 0.5f;
	float v = hitLocal.z / terrainSize + 0.5f;

	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
	{
		return false;
	}

	outU = u;
	outV = v;

	return true;
}

void Terrain::ApplyBrush(float u, float v, float heightSign)
{
	if (brush_size <= 0)
	{
		return;
	}

	int centerX = static_cast<int>(u * static_cast<float>(TerrainTextureWidth - 1));
	int centerY = static_cast<int>(v * static_cast<float>(TerrainTextureHeight - 1));

	int x0 = centerX - brush_size;
	int x1 = centerX + brush_size;
	int y0 = centerY - brush_size;
	int y1 = centerY + brush_size;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 >= TerrainTextureWidth) x1 = TerrainTextureWidth - 1;
	if (y1 >= TerrainTextureHeight) y1 = TerrainTextureHeight - 1;

	float radius = static_cast<float>(brush_size);

	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			float dx = static_cast<float>(x - centerX);
			float dy = static_cast<float>(y - centerY);
			float distance = sqrtf(dx * dx + dy * dy) / radius;

			if (distance > 1.0f)
			{
				continue;
			}

			float falloff = 1.0f - distance;
			falloff = falloff * falloff * (3.0f - 2.0f * falloff);

			Vector4& pixel = terrainPixels[y * TerrainTextureWidth + x];

			if (brushMode == BrushMode::Height)
			{
				pixel.x += heightBrushStrength * heightSign * falloff;
			}
			else
			{
				pixel.y += (blendTarget - pixel.y) * blendBrushStrength * falloff;

				if (pixel.y < 0.0f) pixel.y = 0.0f;
				if (pixel.y > 1.0f) pixel.y = 1.0f;
			}
		}
	}

	terrainTextureDirty = true;
}

void Terrain::DrawGUI()
{
	if (ImGui::TreeNode("Terrain"))
	{
		if (ImGui::Button("terrain texture clear"))
		{
			is_terrain_texture_clear_color = true;
		}

		ImGui::Text("R = height, G = material blend");
		ImGui::DragFloat4("terrain data clear RGBA", &terrain_texture_clear_color.x, 0.01f);

		if (terrainTextureShaderResourceView)
		{
			ImGui::Image(
				terrainTextureShaderResourceView.Get(),
				ImVec2(256, 256),
				ImVec2(0, 0),
				ImVec2(1, 1));
		}

		ImGui::Separator();

		float newTerrainSize = terrainSize;
		if (ImGui::DragFloat("terrain size", &newTerrainSize, 1.0f, 1.0f, 10000.0f))
		{
			terrainSize = newTerrainSize;
			CreateGridMesh(Game::Graphics::Instance().GetDevice());
		}

		int newGridResolution = gridResolution;
		if (ImGui::DragInt("grid resolution", &newGridResolution, 1, 1, 256))
		{
			gridResolution = newGridResolution;
			CreateGridMesh(Game::Graphics::Instance().GetDevice());
		}

		ImGui::Separator();

		ImGui::Checkbox("use brush", &use_brush);

		int brushModeIndex = static_cast<int>(brushMode);
		const char* brushModeItems[] =
		{
			"Height",
			"Blend",
		};

		if (ImGui::Combo("brush mode", &brushModeIndex, brushModeItems, _countof(brushModeItems)))
		{
			brushMode = static_cast<BrushMode>(brushModeIndex);
		}

		ImGui::SliderInt("brush size", &brush_size, 1, 256);
		ImGui::DragFloat("height brush strength", &heightBrushStrength, 0.001f, 0.0f, 1.0f);
		ImGui::DragFloat("blend brush strength", &blendBrushStrength, 0.001f, 0.0f, 1.0f);
		ImGui::SliderFloat("blend target", &blendTarget, 0.0f, 1.0f);
		ImGui::Text("Left drag: paint");
		ImGui::Text("Shift + left drag: lower height");

		ImGui::Separator();

		ImGui::Checkbox("wire", &use_wire);
		ImGui::SliderFloat("edge", &tesselation_constant.edge_factor, 1.0f, 16.0f);
		ImGui::SliderFloat("inner", &tesselation_constant.inner_factor, 1.0f, 16.0f);
		ImGui::SliderFloat("height scaler", &tesselation_constant.height_scaler, -200.0f, 200.0f);
		ImGui::SliderFloat("tilling scale", &tesselation_constant.tilling_scale, 1.0f, 300.0f);

		ImGui::TreePop();
	}
}

float Terrain::GetHeightByUV(float u, float v) const
{
	if (terrainPixels.empty())
	{
		return 0.0f;
	}

	if (u < 0.0f) u = 0.0f;
	if (v < 0.0f) v = 0.0f;
	if (u > 1.0f) u = 1.0f;
	if (v > 1.0f) v = 1.0f;

	int x = static_cast<int>(u * static_cast<float>(TerrainTextureWidth - 1));
	int y = static_cast<int>(v * static_cast<float>(TerrainTextureHeight - 1));

	const Vector4& pixel = terrainPixels[y * TerrainTextureWidth + x];
	return pixel.x * tesselation_constant.height_scaler;
}