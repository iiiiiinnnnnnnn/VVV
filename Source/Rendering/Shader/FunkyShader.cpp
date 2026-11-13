#include "Rendering/Shader/FunkyShader.h"
#include "Resource/ResourceManager.h"
#include "Rendering/Core/Graphics.h"
#include <ImGuiToolKit/Widgets.h>
#include <ImGuiToolKit/Drag.h>

FunkyShader::FunkyShader(ID3D11Device* device)
{
	static_assert(sizeof(CbRainbowData) == 16);
	static_assert(sizeof(CbMaterial) == 160);
	vertexShader = std::make_unique<Gpu::VertexShader>(device,
		ResourceManager::Instance().ResolveShaderPath("Resources/Shader/PBRVS.cso").c_str(), ModelShader::InputLayout);
	pixelShader = std::make_unique<Gpu::PixelShader>(device,
		ResourceManager::Instance().ResolveShaderPath("Resources/Shader/FunkyPS.cso").c_str());
	rainbowConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbRainbowData));
	materialConstantBuffer = std::make_unique<Gpu::ConstantBuffer>(device, sizeof(CbMaterial));
}

void FunkyShader::Begin(const RenderContext& rc, DrawPass pass)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	const bool transparent = pass == DrawPass::Transparent;
	(transparent ? *rc.graphics->TransparentBlendState : *rc.graphics->OpaqueBlendState).Bind(dc);
	(transparent ? *rc.graphics->DepthReadState : *rc.graphics->DepthWriteState).Bind(dc);
	vertexShader->Bind(dc);
	Gpu::GeometryShader::Unbind(dc);
	pixelShader->Bind(dc);
}

void FunkyShader::Update(const RenderContext& rc, const VMDLModel::Mesh& mesh,
	const PBRRenderParams* params, const PBRMaterialParams* materialParams)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	pixelShader->Bind(dc);
	Gpu::GeometryShader::Unbind(dc);

	// 共用頂点シェーダーの振動と、Renderer側の表示モード切り替えに必要なCB
	CbMaterial material{};
	// 表示モードの上書き色だけを受け取り、通常の虹描画には使わない
	material.baseColor = materialParams && materialParams->baseColor
		? *materialParams->baseColor : Color(1, 1, 1, 1);
	if (params)
	{
		material.vivraDir = params->vibrationDirection;
		material.vivraPower = params->vibrationPower;
	}
	materialConstantBuffer->Update(dc, &material);
	materialConstantBuffer->BindVS(dc, 1);
	materialConstantBuffer->BindPS(dc, 1);

	CbRainbowData rainbow{};
	rainbow.lineWidth = std::max(mesh.material->funkyLineWidth, 0.001f);
	rainbow.speed = mesh.material->funkySpeed;
	rainbowConstantBuffer->Update(dc, &rainbow);
	rainbowConstantBuffer->BindPS(dc, 2);
}

void FunkyShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	Gpu::VertexShader::Unbind(dc);
	Gpu::GeometryShader::Unbind(dc);
	Gpu::PixelShader::Unbind(dc);
	Gpu::VertexShader::UnbindInputLayout(dc);
	Gpu::ConstantBuffer::UnbindVS(dc, 1, 1);
	Gpu::ConstantBuffer::UnbindPS(dc, 1, 3);
	rc.graphics->DepthWriteState->Bind(dc);
	rc.graphics->SolidRasterizerState->Bind(dc);
}

bool FunkyShader::DrawGUI(VMDLModel::Material& material)
{
	ImGuiToolKit::SeparatorText(u8"Funkyシェーダー");
	
	ImGui::PushItemWidth(240.0f);

	bool changed = ImGuiToolKit::DragSizeFloat(u8"縞の幅", &material.funkyLineWidth,
		0.05f, 0.001f, 10000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	changed |= ImGuiToolKit::DragSpeedFloat(u8"移動速度", &material.funkySpeed,
		0.1f, -1000.0f, 1000.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	if (ImGui::IsItemHovered()) ImGuiToolKit::SetTooltip(u8"0で停止、負の値で逆方向に流れます");
	
	ImGui::PopItemWidth();
	
	return changed;
}
