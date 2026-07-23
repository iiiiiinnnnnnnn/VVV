// DirectXSerialization.h
#pragma once

#include "Core/Foundation/Common.h"

#include <cereal/cereal.hpp>

namespace DirectX
{
	template<class Archive>
	void serialize(Archive& archive, XMUINT4& value)
	{
		archive(cereal::make_nvp("x", value.x), cereal::make_nvp("y", value.y), cereal::make_nvp("z", value.z), cereal::make_nvp("w", value.w));
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT2& value)
	{
		archive(cereal::make_nvp("x", value.x), cereal::make_nvp("y", value.y));
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT3& value)
	{
		archive(cereal::make_nvp("x", value.x), cereal::make_nvp("y", value.y), cereal::make_nvp("z", value.z));
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4& value)
	{
		archive(cereal::make_nvp("x", value.x), cereal::make_nvp("y", value.y), cereal::make_nvp("z", value.z), cereal::make_nvp("w", value.w));
	}

	template<class Archive>
	void serialize(Archive& archive, XMFLOAT4X4& value)
	{
		archive(
			cereal::make_nvp("_11", value._11), cereal::make_nvp("_12", value._12), cereal::make_nvp("_13", value._13), cereal::make_nvp("_14", value._14),
			cereal::make_nvp("_21", value._21), cereal::make_nvp("_22", value._22), cereal::make_nvp("_23", value._23), cereal::make_nvp("_24", value._24),
			cereal::make_nvp("_31", value._31), cereal::make_nvp("_32", value._32), cereal::make_nvp("_33", value._33), cereal::make_nvp("_34", value._34),
			cereal::make_nvp("_41", value._41), cereal::make_nvp("_42", value._42), cereal::make_nvp("_43", value._43), cereal::make_nvp("_44", value._44)
		);
	}
}
