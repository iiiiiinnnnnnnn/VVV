#include "Gameplay/Scene/VstgEditorScene.h"
#include "Gameplay/Stage/Stage.h"
#include "Physics/Navigation/NavMeshActor.h"
#include "Resource/ResourceManager.h"
#include "Resource/ResourcePath.h"
#include "Resource/ImageData.h"
#include "Resource/Texture.h"
#include "Resource/EmbeddedResources.h"
#include "Rendering/Core/Graphics.h"
#include <ImGuiToolKit/Color.h>
#include <ImGuiToolKit/Drag.h>
#include <InputKit/InputKit.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>

void VstgEditorScene::ApplyBrush(float u, float v, float heightSign)
{
	const TerrainBrush* brush = GetCurrentBrush();
	if (brush == nullptr || brush_size <= 0)
	{
		return;
	}

	const int centerX = std::clamp(static_cast<int>(u * Terrain::TerrainTextureWidth), 0, Terrain::TerrainTextureWidth - 1);
	const int centerY = std::clamp(static_cast<int>(v * Terrain::TerrainTextureHeight), 0, Terrain::TerrainTextureHeight - 1);
	const int radius = brush_size;

	const int x0 = std::max(centerX - radius, 0);
	const int y0 = std::max(centerY - radius, 0);
	const int x1 = std::min(centerX + radius, Terrain::TerrainTextureWidth - 1);
	const int y1 = std::min(centerY + radius, Terrain::TerrainTextureHeight - 1);
	const float brushDiameter = static_cast<float>(radius * 2);

	if (brushDiameter <= 0.0f)
	{
		return;
	}

	for (int y = y0; y <= y1; ++y)
	{
		for (int x = x0; x <= x1; ++x)
		{
			const float brushU =
				static_cast<float>(x - (centerX - radius)) / brushDiameter;

			const float brushV =
				static_cast<float>(y - (centerY - radius)) / brushDiameter;

			float mask = SampleBrushMask(brushU, brushV);
			if (invertBrushMask)
			{
				mask = 1.0f - mask;
			}

			if (mask <= 0.0001f)
			{
				continue;
			}

			Vector4& pixel = terrain->terrainPixels[
				static_cast<size_t>(y) * Terrain::TerrainTextureWidth +
				static_cast<size_t>(x)];

			if (brushMode == BrushMode::RaiseLower)
			{
				pixel.x += heightBrushStrength * heightSign * mask;
			}
			else if (brushMode == BrushMode::SetHeight)
			{
				const float setAmount = std::clamp(mask, 0.0f, 1.0f);
				pixel.x += (setHeightValue - pixel.x) * setAmount;
			}
			else if (brushMode == BrushMode::Paint)
			{
				const float paintAmount = std::clamp(
					paintOpacity * mask,
					0.0f,
					1.0f);
				const float targetLayer = GetTerrainLayerValue(currentTerrainLayerIndex);

				pixel.y += (targetLayer - pixel.y) * paintAmount;
				pixel.y = std::clamp(pixel.y, 0.0f, 1.0f);
			}
			else if (brushMode == BrushMode::GrassPaint)
			{
				const float paintAmount = std::clamp(paintOpacity * mask, 0.0f, 1.0f);
				float targetGrass = 1.0f;
				if (heightSign < 0.0f) targetGrass = 0.0f;
				pixel.z += (targetGrass - pixel.z) * paintAmount;
				pixel.z = std::clamp(pixel.z, 0.0f, 1.0f);
			}
			else if (brushMode == BrushMode::NavMeshPaint)
			{
				const float amount = std::clamp(paintOpacity * mask, 0.0f, 1.0f);
				pixel.w = std::clamp(pixel.w + ((heightSign < 0.0f ? 0.0f : 1.0f) - pixel.w) * amount, 0.0f, 1.0f);
				terrain->navMeshMaskInitialized = true;
			}
		}
	}

	terrain->terrainTextureDirty = true;

	terrain->terrainEditHashDirty = true;
	dirty = true;
	const bool heightChanged = brushMode == BrushMode::RaiseLower || brushMode == BrushMode::SetHeight;
	if (heightChanged)
	{
		terrain->navigationBaseHeights.clear();
		terrain->MarkTerrainMeshDirty();
	}
	if (brushMode == BrushMode::GrassPaint) terrain->grassDirty = true;
	if (heightChanged || brushMode == BrushMode::NavMeshPaint)
	{
		if (NavMeshActor* navMesh = currentStage->GetComponent<NavMeshActor>())
		{
			const Vector3 center((u - 0.5f) * terrain->terrainSize, terrain->GetHeightByUV(u, v), (v - 0.5f) * terrain->terrainSize);
			navMesh->RequestBuildRegion(center, (brush_size + 1.0f) / Terrain::TerrainTextureWidth * terrain->terrainSize);
		}
	}
}

bool VstgEditorScene::AddBrushTexture(const std::string& filename)
{
	if (filename.empty())
	{
		terrainEditorMessage = "Brush add failed: filename is empty.";
		return false;
	}

	const std::filesystem::path filepath =
		ResourcePath::Path(filename).lexically_normal();


	const std::string normalizedPath = ResourcePath::Text(filepath);

	for (int i = 0; i < static_cast<int>(brushes.size()); ++i)
	{
		if (brushes[i].filepath == normalizedPath)
		{
			currentBrushIndex = i;
			return true;
		}
	}

	ImageData decodedImage;
	HRESULT hr = decodedImage.LoadFile(filepath);
	const auto& sourceMetadata = decodedImage.GetMetadata();
	const auto& sourceImage = decodedImage.GetImage();
	if (FAILED(hr))
	{
		const auto data = ResourceManager::LoadEmbeddedResource(IDR_ERROR_TEXTURE);
		hr = decodedImage.LoadMemory(data.data(), data.size());
		if (FAILED(hr)) return false;
	}

	const DirectX::ScratchImage* workingImage = &sourceImage;
	DirectX::ScratchImage convertedImage;

	if (sourceMetadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
	{
		if (DirectX::IsCompressed(sourceMetadata.format))
		{
			hr = DirectX::Decompress(
				sourceImage.GetImages(),
				sourceImage.GetImageCount(),
				sourceMetadata,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				convertedImage);
		}
		else
		{
			hr = DirectX::Convert(
				sourceImage.GetImages(),
				sourceImage.GetImageCount(),
				sourceMetadata,
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				convertedImage);
		}

		if (FAILED(hr))
		{
			terrainEditorMessage = "Brush format conversion failed: " + normalizedPath;
			return false;
		}

		workingImage = &convertedImage;
	}

	const DirectX::Image* image = workingImage->GetImage(0, 0, 0);
	if (image == nullptr)
	{
		terrainEditorMessage = "Brush image data is empty: " + normalizedPath;
		return false;
	}

	TerrainBrush brush;
	brush.name = ResourcePath::Text(filepath.stem());
	brush.filepath = normalizedPath;
	brush.width = static_cast<int>(image->width);
	brush.height = static_cast<int>(image->height);

	if (brush.width <= 0 || brush.height <= 0)
	{
		terrainEditorMessage = "Brush image size is invalid: " + normalizedPath;
		return false;
	}

	brush.mask.resize(
		static_cast<size_t>(brush.width) *
		static_cast<size_t>(brush.height));

	for (int y = 0; y < brush.height; ++y)
	{
		const uint8_t* row = image->pixels + static_cast<size_t>(y) * image->rowPitch;

		for (int x = 0; x < brush.width; ++x)
		{
			const uint8_t* pixel = row + static_cast<size_t>(x) * 4;

			brush.mask[
				static_cast<size_t>(y) * static_cast<size_t>(brush.width) +
				static_cast<size_t>(x)] =
				static_cast<float>(pixel[3]) / 255.0f;
		}
	}

	hr = DirectX::CreateShaderResourceView(
		Game::Graphics::Instance().GetDevice(),
		workingImage->GetImages(),
		workingImage->GetImageCount(),
		workingImage->GetMetadata(),
		brush.shaderResourceView.GetAddressOf());

	if (FAILED(hr))
	{
		terrainEditorMessage = "Brush GPU texture creation failed: " + normalizedPath;
		return false;
	}

	brushes.push_back(std::move(brush));

	if (currentBrushIndex < 0)
	{
		currentBrushIndex = 0;
	}

	terrainEditorMessage = "Brush added: " + brushes.back().name;
	return true;
}

bool VstgEditorScene::SetBrushTexture(int index)
{
	if (index < 0 || index >= static_cast<int>(brushes.size()))
	{
		terrainEditorMessage = "Brush switch failed: invalid index.";
		return false;
	}

	currentBrushIndex = index;
	terrainEditorMessage = "Brush selected: " + brushes[currentBrushIndex].name;
	return true;
}

const VstgEditorScene::TerrainBrush* VstgEditorScene::GetCurrentBrush() const
{
	if (currentBrushIndex < 0 || currentBrushIndex >= static_cast<int>(brushes.size()))
	{
		return nullptr;
	}

	return &brushes[currentBrushIndex];
}

float VstgEditorScene::SampleBrushMask(float u, float v) const
{
	const TerrainBrush* brush = GetCurrentBrush();

	if (brush == nullptr || brush->mask.empty() || brush->width <= 0 || brush->height <= 0)
	{
		return 0.0f;
	}

	if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f)
	{
		return 0.0f;
	}

	const float sourceX = u * static_cast<float>(brush->width - 1);
	const float sourceY = v * static_cast<float>(brush->height - 1);

	const int x0 = static_cast<int>(std::floor(sourceX));
	const int y0 = static_cast<int>(std::floor(sourceY));
	const int x1 = std::min(x0 + 1, brush->width - 1);
	const int y1 = std::min(y0 + 1, brush->height - 1);

	const float tx = sourceX - static_cast<float>(x0);
	const float ty = sourceY - static_cast<float>(y0);

	auto GetMask = [brush](int x, int y)
	{
		return brush->mask[
			static_cast<size_t>(y) * static_cast<size_t>(brush->width) +
			static_cast<size_t>(x)];
	};

	const float mask00 = GetMask(x0, y0);
	const float mask10 = GetMask(x1, y0);
	const float mask01 = GetMask(x0, y1);
	const float mask11 = GetMask(x1, y1);

	const float upper = mask00 + (mask10 - mask00) * tx;
	const float lower = mask01 + (mask11 - mask01) * tx;

	return upper + (lower - upper) * ty;
}

float VstgEditorScene::GetTerrainLayerValue(int layerIndex) const
{
	if (terrain->terrainLayers.size() <= 1)
	{
		return 0.0f;
	}

	const int clampedIndex = std::clamp(
		layerIndex,
		0,
		static_cast<int>(terrain->terrainLayers.size()) - 1);

	return static_cast<float>(clampedIndex) /
		static_cast<float>(terrain->terrainLayers.size() - 1);
}

void VstgEditorScene::DrawTerrainGUI()
{
	if (!terrain) return;
	currentTerrainLayerIndex = std::clamp(currentTerrainLayerIndex, 0,
		std::max(terrain->GetTerrainLayerCount() - 1, 0));
	if (!terrainEditorMessage.empty()) ImGui::TextWrapped("%s", terrainEditorMessage.c_str());
	if (!grassDraftInitialized)
	{
		grassDraftSettings = terrain->grassSettings;
		grassDraftInitialized = true;
	}

	// よく使う地形ブラシ
	ImGui::TextUnformatted(ICON_FA_PAINT_BRUSH " 地形ブラシ");
	ImGui::Checkbox(ICON_FA_PAINT_BRUSH " ブラシを使用（B）", &use_brush);

	int brushModeIndex = static_cast<int>(brushMode);
	const char* brushModeItems[] =
	{
		ICON_FA_MOUNTAIN  " 上げる／下げる",
		ICON_FA_MOUNTAIN " 高さを指定",
		ICON_FA_PAINT_BRUSH " ペイント",
		ICON_FA_LEAF " 草マップ",
		ICON_FA_WALKING " NavMesh範囲",
	};

	if (ImGui::Combo(ICON_FA_PAINT_BRUSH " ブラシモード", &brushModeIndex, brushModeItems,
			_countof(brushModeItems)))
	{
		brushMode = static_cast<BrushMode>(brushModeIndex);
		use_brush = true;
	}

	ImGui::DragInt(ICON_FA_PAINT_BRUSH " ブラシサイズ", &brush_size, 1.0f, 1, 256, "%d", ImGuiSliderFlags_AlwaysClamp);
	if (brushMode == BrushMode::RaiseLower)
	{
		ImGui::DragFloat(ICON_FA_COG " 上下の強さ", &heightBrushStrength, 0.001f, 0.0f, 1.0f);
	}
	else if (brushMode == BrushMode::SetHeight)
	{
		ImGui::DragFloat(ICON_FA_COG " 指定する高さ", &setHeightValue, 0.001f, -10.0f, 10.0f);
	}
	else if (brushMode == BrushMode::Paint)
	{
		ImGui::DragFloat(ICON_FA_PAINT_BRUSH " ペイント不透明度", &paintOpacity, 0.001f, 0.0f, 1.0f);
	}
	else if (brushMode == BrushMode::GrassPaint)
	{
		ImGui::DragFloat(ICON_FA_COG " 草の塗り強さ", &paintOpacity, 0.001f, 0.0f, 1.0f);
	}
	else if (brushMode == BrushMode::NavMeshPaint)
	{
		ImGui::DragFloat(ICON_FA_COG " 塗り強さ", &paintOpacity, 0.001f, 0.0f, 1.0f);
		ImGui::TextUnformatted("左ドラッグ：範囲を追加 / Shift：範囲を削除");
		ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "緑：入力範囲 / 赤：範囲外 / 水色の線：生成結果");
		if (NavMeshActor* navMesh = currentStage->GetComponent<NavMeshActor>())
		{
			if (ImGui::TreeNode(ICON_FA_MAP " NavMesh生成設定"))
			{
				navMesh->DrawGUI();
				ImGui::TreePop();
			}
		}
	}
	ImGui::Separator();

	// ブラシとペイントレイヤー
	DrawBrushGUI();
	if (brushMode == BrushMode::Paint)
	{
		DrawTerrainLayerGUI();
	}

	// 地形のマテリアル
	if (ImGui::TreeNode(ICON_FA_SLIDERS_H " 地形PBR"))
	{
		ImGuiToolKit::ColorEdit4("基本色", &terrain->baseColor.x);
		ImGuiToolKit::ColorEdit4("発光色", &terrain->emissiveColor.x);
		ImGui::SliderFloat(ICON_FA_COG " メタリック", &terrain->metalness, 0.0f, 1.0f);
		ImGui::SliderFloat(ICON_FA_COG " 粗さ", &terrain->roughness, 0.0001f, 1.0f);
		ImGui::SliderFloat(ICON_FA_COG " オクルージョン", &terrain->occlusion, 0.0f, 1.0f);
		ImGui::SliderFloat(ICON_FA_COG " オクルージョン強度", &terrain->occlusionStrength, 0.0f, 1.0f);
		ImGui::SliderFloat(ICON_FA_COG " 影の強度", &terrain->shadowStrength, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode(ICON_FA_LEAF " 草マップ"))
	{
		bool changed = false;
		changed |= ImGui::DragFloat(ICON_FA_COG " 密度", &grassDraftSettings.density,
			0.05f, 0.0f, 0.0f);
		changed |= ImGuiToolKit::DragSizeFloat("横幅", &grassDraftSettings.width,
			0.01f, 0.05f, 5.0f);
		changed |= ImGuiToolKit::DragSizeFloat("高さ", &grassDraftSettings.height,
			0.01f, 0.05f, 8.0f);
		changed |= ImGui::SliderFloat(ICON_FA_COG " 大きさのばらつき",
			&grassDraftSettings.sizeVariation, 0.0f, 0.9f);
		changed |= ImGui::DragFloat(ICON_FA_COG " 風の強さ", &grassDraftSettings.windStrength,
			0.01f, 0.0f, 2.0f);
		changed |= ImGuiToolKit::DragSpeedFloat("風の速度", &grassDraftSettings.windSpeed,
			0.01f, 0.0f, 8.0f);
		changed |= ImGui::DragFloat(ICON_FA_COG " 描画距離", &grassDraftSettings.drawDistance,
			1.0f, 5.0f, 500.0f);
		changed |= ImGuiToolKit::ColorEdit4("草の色", &grassDraftSettings.tint.x);

		if (changed)
		{
			terrain->grassSettings = grassDraftSettings;
			terrain->grassDirty = true;
		}
		int generatedTuftCount = 0;
		if (terrain->grassRenderer)
		{
			generatedTuftCount = terrain->grassRenderer->GetTuftCount();
		}
		ImGui::Text("生成株数: %d", generatedTuftCount);
		ImGui::TextDisabled("草マップはブラシモードから編集し、ブラシをOFFにすると確定します");
		ImGui::TreePop();
	}

	// 地形に適用する距離フォグ
	if (ImGui::TreeNode(ICON_FA_CLOUD " 距離フォグ"))
	{
		ImGui::Checkbox(ICON_FA_COG " 有効##TerrainDistanceFog", &terrain->distanceFogEnabled);
		ImGui::DragFloat(ICON_FA_COG " 開始距離##TerrainDistanceFog",
			&terrain->distanceFogStart,
			0.5f,
			0.0f,
			std::max(terrain->distanceFogEnd - 0.1f, 0.0f));
		ImGui::DragFloat(ICON_FA_COG " 終了距離##TerrainDistanceFog",
			&terrain->distanceFogEnd,
			0.5f,
			terrain->distanceFogStart + 0.1f,
			1000.0f);
		ImGui::DragFloat(ICON_FA_COG " 強度##TerrainDistanceFog",
			&terrain->distanceFogStrength,
			0.01f,
			0.0f,
			1.0f);
		ImGuiToolKit::ColorEdit3("色##TerrainDistanceFog", &terrain->distanceFogColor.x);
		ImGui::TreePop();
	}

	// 高さとペイントを保持する地形データ
	if (ImGui::TreeNode(ICON_FA_DATABASE " 地形データ"))
	{
		if (ImGui::Button(ICON_FA_MOUNTAIN " 地形テクスチャをクリア"))
		{
			terrain->ClearTerrainTexture();
		}

		ImGui::Text("R：高さ / G：レイヤー / B：草 / A：NavMesh範囲");
		ImGui::DragFloat4(ICON_FA_COG " クリア時のRGBA", &terrain->terrain_texture_clear_color.x, 0.01f);

		if (terrain->terrainTextureShaderResourceView)
		{
			ImGui::Image(
				terrain->terrainTextureShaderResourceView.Get(),
				ImVec2(256, 256),
				ImVec2(0, 0),
				ImVec2(1, 1));
		}

		ImGui::TreePop();
	}

	// 地形メッシュとテセレーション
	if (ImGui::TreeNode(ICON_FA_MOUNTAIN " 地形レンダリング"))
	{
		float newTerrainSize = terrain->terrainSize;
		if (ImGuiToolKit::DragSizeFloat("地形サイズ", &newTerrainSize, 1.0f, 1.0f, 10000.0f))
		{
			terrain->terrainSize = newTerrainSize;
			terrain->MarkTerrainMeshDirty();
		}

		int newGridResolution = terrain->gridResolution;
		if (ImGui::DragInt(ICON_FA_COG " グリッド解像度", &newGridResolution, 1, 1, 256))
		{
			terrain->gridResolution = newGridResolution;
			terrain->MarkTerrainMeshDirty();
		}

		bool meshSettingChanged = false;
		meshSettingChanged |= ImGui::SliderFloat(ICON_FA_COG " 外周分割", &terrain->tesselation_constant.edge_factor, 1.0f, 16.0f);
		meshSettingChanged |= ImGui::SliderFloat(ICON_FA_COG " 内部分割", &terrain->tesselation_constant.inner_factor, 1.0f, 16.0f);
		if (ImGui::SliderFloat(ICON_FA_COG " 高さ倍率", &terrain->tesselation_constant.height_scaler, -200.0f, 200.0f))
		{
			meshSettingChanged = true;
		}
		if (meshSettingChanged) terrain->MarkTerrainMeshDirty();
		ImGui::SliderFloat(ICON_FA_COG " タイリング倍率", &terrain->tesselation_constant.tilling_scale, 1.0f, 300.0f);

		ImGui::TreePop();
	}
}

void VstgEditorScene::DrawBrushGUI()
{
	if (!ImGui::TreeNode(ICON_FA_IMAGE " ブラシテクスチャ"))
	{
		return;
	}

	if (ImGui::Button(ICON_FA_SYNC_ALT " 再読み込み##TerrainBrushes")) RefreshTerrainAssets();

	const TerrainBrush* currentBrush = GetCurrentBrush();
	ImGui::Text("選択中: %s",
		currentBrush != nullptr ? currentBrush->name.c_str() : "なし");

	// ブラシテクスチャのサムネイル一覧
	{
		const float thumbnailSize = 72.0f;
		const float childHeight =
			thumbnailSize +
			ImGui::GetStyle().ScrollbarSize +
			ImGui::GetStyle().WindowPadding.y * 2.0f;

		ImGui::BeginChild(
			"brush texture palette",
			ImVec2(0.0f, childHeight),
			true,
			ImGuiWindowFlags_HorizontalScrollbar);

		for (int i = 0; i < static_cast<int>(brushes.size()); ++i)
		{
			ImGui::PushID(i);

			const bool selected = i == currentBrushIndex;
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.85f, 1.0f));
			}

			if (ImGui::ImageButton(
				"brush texture",
				brushes[i].shaderResourceView.Get(),
				ImVec2(thumbnailSize, thumbnailSize),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f)))
			{
				SetBrushTexture(i);
			}

			if (selected)
			{
				ImGui::PopStyleColor(3);
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"%s\n%d x %d\n%s",
					brushes[i].name.c_str(),
					brushes[i].width,
					brushes[i].height,
					brushes[i].filepath.c_str());
			}

			if (i + 1 < static_cast<int>(brushes.size()))
			{
				ImGui::SameLine();
			}

			ImGui::PopID();
		}

		ImGui::EndChild();
	}

	ImGui::Checkbox(ICON_FA_PAINT_BRUSH " ブラシマスクを反転", &invertBrushMask);

	ImGui::Text("ブラシ数: %d", static_cast<int>(brushes.size()));
	ImGui::TreePop();
}

bool VstgEditorScene::ScreenToTerrainUV(float& u, float& v) const
{
	Vector3 point;
	if (!terrain || !currentStage || !ScreenToTerrainPoint(ImGui::GetMousePos(), point)) return false;
	const Vector3 local = Vector3::Transform(point, currentStage->GetTransform()->GetMatrix().Invert());
	u = local.x / terrain->GetTerrainSize() + 0.5f;
	v = local.z / terrain->GetTerrainSize() + 0.5f;
	return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
}

void VstgEditorScene::UpdateTerrainBrush()
{
	terrainBrushPreview = {};
	if (!terrain || !use_brush) return;
	auto& io = ImGui::GetIO();
	if (!InputKit::InputSystem::IsFocusedWindow() || io.KeyAlt) return;
	if (!io.WantTextInput)
	{
		const int step = std::max(brush_size / 10, 1);
		if (ImGui::IsKeyPressed(ImGuiKey_RightBracket, true)) brush_size = std::max(brush_size - step, 1);
		if (ImGui::IsKeyPressed(ImGuiKey_Backslash, true)) brush_size = std::min(brush_size + step, 256);
	}
	const TerrainBrush* brush = GetCurrentBrush();
	if (!brush) return;
	terrainBrushPreview.terrain = terrain;
	terrainBrushPreview.mode = static_cast<int>(brushMode) + 1;
	terrainBrushPreview.flags = (invertBrushMask ? 1 : 0) |
		(io.KeyShift && brushMode != BrushMode::SetHeight && brushMode != BrushMode::Paint ? 2 : 0);
	terrainBrushPreview.parameters = Vector4(heightBrushStrength, setHeightValue,
		GetTerrainLayerValue(currentTerrainLayerIndex), paintOpacity);
	terrainBrushPreview.texture = brush->shaderResourceView.Get();
	float u = 0.0f;
	float v = 0.0f;
	if (!brushViewportInputAllowed || !ScreenToTerrainUV(u, v)) return;
	const int width = terrain->GetHeightMapWidth();
	const int height = terrain->GetHeightMapHeight();
	const int x = std::clamp(static_cast<int>(u * width), 0, width - 1);
	const int y = std::clamp(static_cast<int>(v * height), 0, height - 1);
	terrainBrushPreview.area = Vector4((x + 0.5f) / width, (y + 0.5f) / height,
		static_cast<float>(brush_size) / width, 1.0f);
	if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) ApplyBrush(u, v, io.KeyShift ? -1.0f : 1.0f);
}

void VstgEditorScene::RefreshTerrainAssets()
{
	auto& resources = ResourceManager::Instance();
	if (!resources.RefreshResources())
	{
		terrainEditorMessage = "地形画像の再読み込みに失敗しました。";
		return;
	}
	const auto listPng = [](ResourceManager::ResourceType type) {
		std::vector<std::filesystem::path> paths;
		std::error_code error;
		for (std::filesystem::recursive_directory_iterator file(ResourceManager::GetResourceDirectory(type), error), end;
			file != end && !error; file.increment(error))
		{
			if (!file->is_regular_file(error)) continue;
			std::string extension = file->path().extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (extension == ".png") paths.push_back(file->path());
		}
		std::sort(paths.begin(), paths.end());
		return paths;
	};
	const std::string selectedBrush = GetCurrentBrush() ? GetCurrentBrush()->filepath : "";
	brushes.clear();
	currentBrushIndex = -1;
	for (const auto& path : listPng(ResourceManager::ResourceType::TerrainBrush))
	{
		if (AddBrushTexture(ResourcePath::Text(path)) && brushes.back().filepath == selectedBrush)
			currentBrushIndex = static_cast<int>(brushes.size()) - 1;
	}
	terrainLayerImages.clear();
	for (const auto& path : listPng(ResourceManager::ResourceType::TerrainLayer))
	{
		std::string stem = path.stem().string();
		std::transform(stem.begin(), stem.end(), stem.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (stem.ends_with("_n")) continue;
		TerrainLayerImage layer;
		layer.name = ResourcePath::Text(path.stem());
		layer.baseColorPath = ResourceManager::PortableResourcePath(path);
		const auto normal = path.parent_path() / (path.stem().wstring() + L"_n.png");
		if (std::filesystem::exists(normal)) layer.normalPath = ResourceManager::PortableResourcePath(normal);
		layer.texture = resources.LoadTexture(layer.baseColorPath, ResourceManager::ResourceType::TerrainLayer);
		if (!layer.texture) continue;
		terrainLayerImages.push_back(std::move(layer));
	}
	if (terrain)
	{
		// 読み込み直してもVSTGのレイヤー番号とパスの対応を変えない。
		for (auto& layer : terrain->terrainLayers)
		{
			MipmapTexture base(layer.baseColorPath.c_str());
			layer.baseColorView = base.GetShaderResourceView();
			if (layer.normalPath.empty()) layer.normalView = terrain->defaultNormalTexture->GetShaderResourceView();
			else
			{
				MipmapTexture normal(layer.normalPath.c_str());
				layer.normalView = normal.GetShaderResourceView();
			}
		}
		currentTerrainLayerIndex = std::clamp(currentTerrainLayerIndex, 0,
			std::max(terrain->GetTerrainLayerCount() - 1, 0));
	}
	terrainEditorMessage.clear();
}

void VstgEditorScene::DrawTerrainLayerGUI()
{
	if (!ImGui::TreeNode(ICON_FA_LAYER_GROUP " 地形ペイントレイヤー")) return;
	if (ImGui::Button(ICON_FA_SYNC_ALT " 再読み込み##TerrainLayers")) RefreshTerrainAssets();
	const float size = 72.0f;
	const float height = size + ImGui::GetStyle().ScrollbarSize + ImGui::GetStyle().WindowPadding.y * 2.0f;
	ImGui::BeginChild("terrain layer palette", ImVec2(0.0f, height), true, ImGuiWindowFlags_HorizontalScrollbar);
	for (size_t i = 0; i < terrainLayerImages.size(); ++i)
	{
		const auto& layer = terrainLayerImages[i];
		const auto key = ResourceManager::ShortResourcePath(layer.baseColorPath, ResourceManager::ResourceType::TerrainLayer);
		int activeIndex = -1;
		for (int j = 0; j < terrain->GetTerrainLayerCount(); ++j)
			if (key == ResourceManager::ShortResourcePath(terrain->terrainLayers[j].baseColorPath,
				ResourceManager::ResourceType::TerrainLayer)) { activeIndex = j; break; }
		ImGui::PushID(static_cast<int>(i));
		const bool selected = activeIndex >= 0 && activeIndex == currentTerrainLayerIndex;
		if (selected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.55f, 0.95f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.65f, 1.0f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.85f, 1.0f));
		}
		if (ImGui::ImageButton("terrain layer", layer.texture->GetShaderResourceView().Get(), ImVec2(size, size)))
		{
			if (activeIndex >= 0) currentTerrainLayerIndex = activeIndex;
			else
			{
				const int oldCount = terrain->GetTerrainLayerCount();
				if (terrain->AddTerrainLayer(layer.baseColorPath, layer.normalPath))
				{
					const int newCount = terrain->GetTerrainLayerCount();
					const float scale = static_cast<float>(std::max(oldCount - 1, 0)) / std::max(newCount - 1, 1);
					for (auto& pixel : terrain->terrainPixels) pixel.y *= scale;
					terrain->terrainTextureDirty = true;
					terrain->terrainEditHashDirty = true;
					currentTerrainLayerIndex = newCount - 1;
					dirty = true;
				}
				else terrainEditorMessage = terrain->terrainIoMessage;
			}
		}
		if (selected) ImGui::PopStyleColor(3);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\n%s", layer.name.c_str(), layer.baseColorPath.c_str());
		ImGui::PopID();
		if (i + 1 < terrainLayerImages.size()) ImGui::SameLine();
	}
	ImGui::EndChild();
	ImGui::TreePop();
}
