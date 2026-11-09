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
	
	Gpu::InputLayoutDesc layout;
	layout.AddPosition().AddTexcoord();

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
