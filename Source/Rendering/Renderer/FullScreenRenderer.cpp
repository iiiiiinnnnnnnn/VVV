#include "Resource/ResourceManager.h"
#include "Rendering/Renderer/FullScreenRenderer.h"
#include <Dx11Kit/Dx11Kit.h>
#include <cstddef>

FullScreenRenderer::FullScreenRenderer(ID3D11Device* device)
{
	const Vertex vertices[VertexCount] =
	{
		{{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
	};
	
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	vertexBuffer = std::make_unique<Gpu::VertexBuffer>(device, vertices, static_cast<UINT>(sizeof(vertices)));
	vertexShader = std::make_unique<Gpu::VertexShader>(device, ResourceManager::Instance().ResolveShaderPath("Resources/Shader/FullScreenQuadVS.cso").c_str(), layout);
}

void FullScreenRenderer::Draw(ID3D11DeviceContext* context) const
{
	vertexBuffer->Bind(context, sizeof(Vertex));
	vertexShader->Bind(context);
	Gpu::VertexBuffer::SetTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	vertexBuffer->Draw(context, VertexCount);

	Gpu::VertexBuffer::Unbind(context);
	Gpu::VertexShader::Unbind(context);
	Gpu::VertexShader::UnbindInputLayout(context);
}
