#include "Rendering/Core/InputLayoutDesc.h"
#include <stdexcept>

Gpu::InputLayoutDesc& Gpu::InputLayoutDesc::Add(const char* semantic, DXGI_FORMAT format,
	UINT semanticIndex, UINT slot, UINT offset, D3D11_INPUT_CLASSIFICATION classification, UINT stepRate)
{
	if (!semantic || !*semantic) throw std::invalid_argument("Input semantic is empty.");
	if (elements.size() >= D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)
		throw std::out_of_range("Too many input elements.");
	elements.push_back({semantic, format, semanticIndex, slot, offset, classification, stepRate});
	return *this;
}

std::vector<D3D11_INPUT_ELEMENT_DESC> Gpu::InputLayoutDesc::GetElements() const
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> result;
	result.reserve(elements.size());
	for (const auto& element : elements)
		result.push_back({element.semantic.c_str(), element.semanticIndex, element.format, element.slot,
			element.offset, element.classification, element.stepRate});
	return result;
}
