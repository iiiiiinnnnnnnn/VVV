// GLBExporter.cpp
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Resource/VMDLModel.h"
#include "Core/Foundation/Json.h"
#include <DirectXTex.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>

bool VMDLModel::ExportGlb(const std::filesystem::path& filepath, std::string* error) const
{
	std::filesystem::path temporary;
	try
	{
		if (filepath.empty() || nodes.empty() || meshes.empty()) throw std::runtime_error("No model to export.");
		json gltf = {{"asset", {{"version", "2.0"}, {"generator", "VMDL Editor"}}},
			{"scene", 0}};
		for (const char* key : {"nodes", "meshes", "materials", "skins", "animations",
			"accessors", "bufferViews", "images", "textures"}) gltf[key] = json::array();
		std::vector<uint8_t> binary;
		auto addView = [&](const void* data, size_t size, int target = 0) {
			while (binary.size() % 4) binary.push_back(0);
			json view = {{"buffer", 0}, {"byteOffset", binary.size()}, {"byteLength", size}};
			if (target) view["target"] = target;
			const auto* bytes = static_cast<const uint8_t*>(data);
			binary.insert(binary.end(), bytes, bytes + size);
			gltf["bufferViews"].push_back(view);
			return static_cast<int>(gltf["bufferViews"].size() - 1);
		};
		auto addAccessor = [&](const auto& values, int components, const char* type,
			int componentType = 5126, int target = 0, bool bounds = false) {
			if (values.empty()) throw std::runtime_error("Empty GLB accessor.");
			for (const auto value : values)
				if (!std::isfinite(static_cast<double>(value))) throw std::runtime_error("Non-finite GLB value.");
			json accessor = {{"bufferView", addView(values.data(), values.size() * sizeof(values[0]), target)},
				{"componentType", componentType}, {"count", values.size() / components}, {"type", type}};
			if (bounds)
			{
				std::vector<double> minimum(components, std::numeric_limits<double>::max());
				std::vector<double> maximum(components, std::numeric_limits<double>::lowest());
				for (size_t i = 0; i < values.size(); ++i)
				{
					const double value = values[i];
					if (!std::isfinite(value)) throw std::runtime_error("Non-finite GLB value.");
					minimum[i % components] = std::min(minimum[i % components], value);
					maximum[i % components] = std::max(maximum[i % components], value);
				}
				accessor["min"] = minimum;
				accessor["max"] = maximum;
			}
			gltf["accessors"].push_back(accessor);
			return static_cast<int>(gltf["accessors"].size() - 1);
		};
		auto addTexture = [&](const std::vector<uint8_t>& dds) {
			if (dds.empty()) return -1;
			DirectX::ScratchImage loaded, converted;
			if (FAILED(DirectX::LoadFromDDSMemory(dds.data(), dds.size(), DirectX::DDS_FLAGS_NONE,
				nullptr, loaded))) throw std::runtime_error("Failed to decode material texture.");
			const auto* image = loaded.GetImage(0, 0, 0);
			if (!image) throw std::runtime_error("Missing material texture image.");
			if (DirectX::IsCompressed(image->format))
			{
				if (FAILED(DirectX::Decompress(*image, DXGI_FORMAT_R8G8B8A8_UNORM, converted)))
					throw std::runtime_error("Failed to decompress material texture.");
				image = converted.GetImage(0, 0, 0);
			}
			DirectX::Blob png;
			if (FAILED(DirectX::SaveToWICMemory(*image, DirectX::WIC_FLAGS_NONE,
				DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), png)))
				throw std::runtime_error("Failed to encode material texture as PNG.");
			const int index = static_cast<int>(gltf["images"].size());
			gltf["images"].push_back({{"bufferView", addView(png.GetBufferPointer(), png.GetBufferSize())},
				{"mimeType", "image/png"}});
			gltf["textures"].push_back({{"source", index}});
			return index;
		};
		const auto& glbMaterials = sourceMaterials.empty() ? materials : sourceMaterials;
		for (const Material& material : glbMaterials)
		{
			json pbr = {{"baseColorFactor", {material.baseColor.x, material.baseColor.y,
				material.baseColor.z, material.baseColor.w}}, {"metallicFactor", material.metalness},
				{"roughnessFactor", material.roughness}};
			json output = {{"name", material.name}, {"emissiveFactor", {material.emissiveColor.x,
				material.emissiveColor.y, material.emissiveColor.z}}, {"alphaMode",
				material.alphaMode == AlphaMode::Blend ? "BLEND" : material.alphaMode == AlphaMode::Mask ? "MASK" : "OPAQUE"}};
			if (material.alphaMode == AlphaMode::Mask) output["alphaCutoff"] = material.alphaCutoff;
			int texture = addTexture(material.baseTextureDDS);
			if (texture >= 0) pbr["baseColorTexture"] = {{"index", texture}};
			texture = addTexture(material.metalnessRoughnessTextureDDS);
			if (texture >= 0) pbr["metallicRoughnessTexture"] = {{"index", texture}};
			texture = addTexture(material.normalTextureDDS);
			if (texture >= 0) output["normalTexture"] = {{"index", texture}};
			texture = addTexture(material.emissiveTextureDDS);
			if (texture >= 0) output["emissiveTexture"] = {{"index", texture}};
			texture = addTexture(material.occlusionTextureDDS);
			if (texture >= 0) output["occlusionTexture"] = {{"index", texture}, {"strength", material.occlusionStrength}};
			output["pbrMetallicRoughness"] = pbr;
			gltf["materials"].push_back(output);
		}
		// Reverse the importer's root-facing correction and X-axis reflection.
		const Matrix correction = Matrix::CreateRotationY(-DirectX::XM_PI);
		json roots = json::array();
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const Node& node = nodes[i];
			Vector3 position = node.position, scale = node.scale;
			Quaternion rotation = node.rotation;
			if (node.parentIndex < 0)
			{
				const Matrix local = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rotation) *
					Matrix::CreateTranslation(position);
				(local * correction).Decompose(scale, rotation, position);
				roots.push_back(i);
			}
			gltf["nodes"].push_back({{"name", node.name}, {"translation", {-position.x, position.y, position.z}},
				{"rotation", {-rotation.x, rotation.y, rotation.z, -rotation.w}},
				{"scale", {scale.x, scale.y, scale.z}}});
		}
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const int parent = nodes[i].parentIndex;
			if (parent < 0) continue;
			if (parent >= static_cast<int>(nodes.size())) throw std::runtime_error("Invalid node parent.");
			auto& children = gltf["nodes"][parent]["children"];
			if (children.is_null()) children = json::array();
			children.push_back(i);
		}
		gltf["scenes"] = json::array({{{"nodes", roots}}});
		for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
		{
			if (IsExternalMesh(static_cast<int>(meshIndex)))
				throw std::runtime_error("Exporting external mesh attachments is not supported. Detach them first.");
			const Mesh& mesh = meshes[meshIndex];
			if (mesh.vertices.empty() || mesh.indices.empty()) throw std::runtime_error("Mesh CPU data is unavailable.");
			if (mesh.nodeIndex < 0 || mesh.nodeIndex >= static_cast<int>(nodes.size()))
				throw std::runtime_error("Invalid mesh node.");
			std::vector<float> positions, normals, tangents, texcoords, weights;
			std::vector<uint16_t> joints;
			for (const Vertex& vertex : mesh.vertices)
			{
				positions.insert(positions.end(), {-vertex.position.x, vertex.position.y, vertex.position.z});
				normals.insert(normals.end(), {-vertex.normal.x, vertex.normal.y, vertex.normal.z});
				tangents.insert(tangents.end(), {-vertex.tangent.x, vertex.tangent.y, vertex.tangent.z, vertex.tangent.w});
				texcoords.insert(texcoords.end(), {vertex.texcoord.x, vertex.texcoord.y});
				if (mesh.bones.empty()) continue;
				const std::array<uint32_t, 4> indices = {vertex.boneIndex.x, vertex.boneIndex.y, vertex.boneIndex.z, vertex.boneIndex.w};
				for (uint32_t index : indices)
				{
					if (index >= mesh.bones.size() || index > 65535) throw std::runtime_error("Invalid vertex joint.");
					joints.push_back(static_cast<uint16_t>(index));
				}
				weights.insert(weights.end(), {vertex.boneWeight.x, vertex.boneWeight.y, vertex.boneWeight.z, vertex.boneWeight.w});
			}
			std::vector<uint32_t> indices = mesh.indices;
			if (indices.size() % 3) throw std::runtime_error("Invalid triangle indices.");
			for (uint32_t index : indices)
				if (index >= mesh.vertices.size()) throw std::runtime_error("Invalid vertex index.");
			for (size_t i = 0; i < indices.size(); i += 3) std::swap(indices[i + 1], indices[i + 2]);
			json attributes = {{"POSITION", addAccessor(positions, 3, "VEC3", 5126, 34962, true)},
				{"NORMAL", addAccessor(normals, 3, "VEC3", 5126, 34962)},
				{"TANGENT", addAccessor(tangents, 4, "VEC4", 5126, 34962)},
				{"TEXCOORD_0", addAccessor(texcoords, 2, "VEC2", 5126, 34962)}};
			json meshNode = {{"name", nodes[mesh.nodeIndex].name + " Mesh"}, {"mesh", gltf["meshes"].size()}};
			if (!mesh.bones.empty())
			{
				attributes["JOINTS_0"] = addAccessor(joints, 4, "VEC4", 5123, 34962);
				attributes["WEIGHTS_0"] = addAccessor(weights, 4, "VEC4", 5126, 34962);
				json skinJoints = json::array();
				std::vector<float> matrices;
				for (const Bone& bone : mesh.bones)
				{
					if (bone.nodeIndex < 0 || bone.nodeIndex >= static_cast<int>(nodes.size()))
						throw std::runtime_error("Invalid bone node.");
					skinJoints.push_back(bone.nodeIndex);
					const float* matrix = &bone.offsetTransform._11;
					for (int i = 0; i < 16; ++i)
					{
						const bool reflect = (i / 4 == 0) != (i % 4 == 0);
						matrices.push_back(reflect ? -matrix[i] : matrix[i]);
					}
				}
				meshNode["skin"] = gltf["skins"].size();
				gltf["skins"].push_back({{"joints", skinJoints}, {"inverseBindMatrices", addAccessor(matrices, 16, "MAT4")}});
			}
			if (mesh.materialIndex < 0 || mesh.materialIndex >= static_cast<int>(glbMaterials.size()))
				throw std::runtime_error("Invalid mesh material.");
			gltf["meshes"].push_back({{"primitives", json::array({{{"attributes", attributes},
				{"indices", addAccessor(indices, 1, "SCALAR", 5125, 34963)}, {"material", mesh.materialIndex}, {"mode", 4}}})}});
			auto& children = gltf["nodes"][mesh.nodeIndex]["children"];
			if (children.is_null()) children = json::array();
			children.push_back(gltf["nodes"].size());
			gltf["nodes"].push_back(meshNode);
		}
		for (const Animation& animation : animations)
		{
			json output = {{"name", animation.name}, {"channels", json::array()}, {"samplers", json::array()}};
			for (size_t i = 0; i < animation.nodeAnims.size() && i < nodes.size(); ++i)
			{
				const auto addTrack = [&](const auto& keys, const char* path, bool quaternion) {
					if (keys.empty()) return;
					std::vector<float> times, values;
					for (const auto& key : keys)
					{
						if (!std::isfinite(key.seconds) || key.seconds < 0 || (!times.empty() && key.seconds <= times.back()))
							throw std::runtime_error("Invalid animation key times.");
						times.push_back(key.seconds);
						if constexpr (std::is_same_v<std::decay_t<decltype(key.value)>, Quaternion>)
						{
							Quaternion value = key.value;
							if (nodes[i].parentIndex < 0) value = Quaternion::CreateFromRotationMatrix(Matrix::CreateFromQuaternion(value) * correction);
							value.Normalize();
							values.insert(values.end(), {-value.x, value.y, value.z, -value.w});
						}
						else
						{
							Vector3 value = key.value;
							if (std::string_view(path) == "translation")
							{
								if (nodes[i].parentIndex < 0) value = Vector3::Transform(value, correction);
								value.x = -value.x;
							}
							values.insert(values.end(), {value.x, value.y, value.z});
						}
					}
					output["channels"].push_back({{"sampler", output["samplers"].size()}, {"target", {{"node", i}, {"path", path}}}});
					output["samplers"].push_back({{"input", addAccessor(times, 1, "SCALAR", 5126, 0, true)},
						{"output", addAccessor(values, quaternion ? 4 : 3, quaternion ? "VEC4" : "VEC3")}, {"interpolation", "LINEAR"}});
				};
				addTrack(animation.nodeAnims[i].positionKeyframes, "translation", false);
				addTrack(animation.nodeAnims[i].rotationKeyframes, "rotation", true);
				addTrack(animation.nodeAnims[i].scaleKeyframes, "scale", false);
			}
			if (!output["channels"].empty()) gltf["animations"].push_back(output);
		}
		gltf["buffers"] = json::array({{{"byteLength", binary.size()}}});
		for (const char* key : {"meshes", "materials", "skins", "animations", "accessors", "bufferViews", "images", "textures"})
			if (gltf[key].empty()) gltf.erase(key);
		std::string document = gltf.dump();
		while (document.size() % 4) document.push_back(' ');
		while (binary.size() % 4) binary.push_back(0);
		const uint64_t length = 12ull + 8 + document.size() + 8 + binary.size();
		if (length > UINT32_MAX) throw std::runtime_error("GLB exceeds 4 GB.");
		temporary = filepath;
		temporary += L".export.tmp";
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		const auto writeUint = [&](uint32_t value) { file.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
		writeUint(0x46546C67); writeUint(2); writeUint(static_cast<uint32_t>(length));
		writeUint(static_cast<uint32_t>(document.size())); writeUint(0x4E4F534A);
		file.write(document.data(), static_cast<std::streamsize>(document.size()));
		writeUint(static_cast<uint32_t>(binary.size())); writeUint(0x004E4942);
		file.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
		file.close();
		if (!file || !MoveFileExW(temporary.c_str(), filepath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Failed to save GLB file.");
		return true;
	}
	catch (const std::exception& exception)
	{
		if (!temporary.empty()) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
		if (error) *error = exception.what();
		return false;
	}
}
