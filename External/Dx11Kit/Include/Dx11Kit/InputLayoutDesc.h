#pragma once

#include <Dx11Kit/Common.h>

// セマンティクス名も所有するため、追加・コピー・移動で参照先が壊れない

namespace Gpu
{
class InputLayoutDesc
{
  public:
	// 手動追加(非推奨)
	InputLayoutDesc& Add(const char* semantic, DXGI_FORMAT format, UINT semanticIndex = 0,
		UINT slot = 0, UINT offset = D3D11_APPEND_ALIGNED_ELEMENT,
		D3D11_INPUT_CLASSIFICATION classification = D3D11_INPUT_PER_VERTEX_DATA, UINT stepRate = 0);

	// セマンティクス名を自動で決める追加
	InputLayoutDesc& AddPosition(DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT)
	{
		return Add("POSITION", format);
	}
	InputLayoutDesc& AddNormal(DXGI_FORMAT format = DXGI_FORMAT_R32G32B32_FLOAT)
	{
		return Add("NORMAL", format);
	}
	InputLayoutDesc& AddTangent(DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT)
	{
		return Add("TANGENT", format);
	}
	InputLayoutDesc& AddTexcoord(DXGI_FORMAT format = DXGI_FORMAT_R32G32_FLOAT, UINT index = 0)
	{
		return Add("TEXCOORD", format, index);
	}
	InputLayoutDesc& AddColor(DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT)
	{
		return Add("COLOR", format);
	}
	InputLayoutDesc& AddBoneWeights()
	{
		return Add("BONE_WEIGHTS", DXGI_FORMAT_R32G32B32A32_FLOAT);
	}
	InputLayoutDesc& AddBoneIndices()
	{
		return Add("BONE_INDICES", DXGI_FORMAT_R32G32B32A32_UINT);
	}

	UINT GetCount() const
	{
		return static_cast<UINT>(elements.size());
	}
	// 返す要素の名前は、この設定オブジェクトを参照する
	std::vector<D3D11_INPUT_ELEMENT_DESC> GetElements() const;

  private:
	struct Element
	{
		std::string semantic;
		DXGI_FORMAT format;
		UINT semanticIndex;
		UINT slot;
		UINT offset;
		D3D11_INPUT_CLASSIFICATION classification;
		UINT stepRate;
	};
	std::vector<Element> elements;
};
} // namespace Gpu

inline Gpu::InputLayoutDesc& Gpu::InputLayoutDesc::Add(const char* semantic, DXGI_FORMAT format,
	UINT semanticIndex, UINT slot, UINT offset, D3D11_INPUT_CLASSIFICATION classification, UINT stepRate)
{
	if (!semantic || !*semantic) throw std::invalid_argument("Input semantic is empty.");
	if (elements.size() >= D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
		throw std::out_of_range("Too many input elements.");
	elements.push_back({semantic, format, semanticIndex, slot, offset, classification, stepRate});
	return *this;
}

inline std::vector<D3D11_INPUT_ELEMENT_DESC> Gpu::InputLayoutDesc::GetElements() const
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> result;
	result.reserve(elements.size());
	for (const auto& element : elements)
		result.push_back({element.semantic.c_str(), element.semanticIndex, element.format, element.slot,
			element.offset, element.classification, element.stepRate});
	return result;
}
