#include "Rendering/Shader/Shader.h"

const Gpu::InputLayoutDesc SpriteShader::InputLayout = Gpu::InputLayoutDesc()
	.AddPosition()
	.AddTexcoord()
	.AddColor(DXGI_FORMAT_R32_FLOAT);

const Gpu::InputLayoutDesc ModelShader::InputLayout = Gpu::InputLayoutDesc()
	.AddPosition()
	.AddNormal()
	.AddTangent()
	.AddTexcoord()
	.AddBoneWeights()
	.AddBoneIndices();
