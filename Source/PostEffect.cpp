#include "PostEffect.h"
#include "GpuResourceUtils.h"
#include "imgui.h"
#include "Graphics.h"
#include "GameTime.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

PostEffect::PostEffect()
{
	auto device = Game::Graphics::Instance().GetDevice();

	// フルスクリーンクアッド頂点シェーダー読み込み
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/FullScreenQuadVS.cso",
		nullptr,
		0,
		nullptr,
		fullscreenQuadVS.GetAddressOf());

	// 輝度抽出ピクセルシェーダー読み込み
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/LuminanceExtractionPS.cso",
		luminanceExtractionPS.GetAddressOf());

	// Bloom
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/BloomPS.cso",
		bloomPS.GetAddressOf());

	// ToneMapping
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/ToneMappingPS.cso",
		toneMappingPS.GetAddressOf());

	// 定数バッファ作成
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbPostEffect),
		constantBuffer.GetAddressOf());

	// Default.json があれば読む。なければ現在の初期値で作る。
	if (!Load("Default"))
	{
		Save("Default");
	}
}

void PostEffect::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;
	const RenderState* renderState = rc.renderState;

	// TransitionTo() 後の自動遷移
	UpdateTransition(Game::Time::deltaTime);

	// ブレンドステート設定
	FLOAT blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	dc->OMSetBlendState(renderState->GetBlendState(BlendState::Opaque), blendFactor, 0xFFFFFFFF);

	// 深度ステンシルステート設定
	dc->OMSetDepthStencilState(renderState->GetDepthStencilState(DepthState::NoTestNoWrite), 0);

	// ラスタライザーステート設定
	dc->RSSetState(renderState->GetRasterizerState(RasterizerState::SolidCullNone));

	// 頂点バッファ設定
	dc->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	dc->IASetInputLayout(nullptr);

	// PostEffectでは画面端を回り込ませたくないのでClamp
	ID3D11SamplerState* samplers[] =
	{
		renderState->GetSamplerState(SamplerState::LinearClamp)
	};
	dc->PSSetSamplers(0, _countof(samplers), samplers);

	// 定数バッファ設定
	dc->PSSetConstantBuffers(0, 1, constantBuffer.GetAddressOf());

	// 定数バッファ更新
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cbPostEffect, 0, 0);
}

// 輝度抽出処理
void PostEffect::LuminanceExtraction(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー設定
	dc->VSSetShader(fullscreenQuadVS.Get(), 0, 0);
	dc->PSSetShader(luminanceExtractionPS.Get(), 0, 0);

	// シェーダーリソース設定
	ID3D11ShaderResourceView* srvs[] = { colorMap };
	dc->PSSetShaderResources(0, _countof(srvs), srvs);

	// 描画
	dc->Draw(4, 0);
}

void PostEffect::Bloom(const RenderContext& rc, ID3D11ShaderResourceView* colorMap,
					   ID3D11ShaderResourceView* bloomMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	dc->VSSetShader(fullscreenQuadVS.Get(), 0, 0);
	dc->PSSetShader(bloomPS.Get(), 0, 0);

	ID3D11ShaderResourceView* srvs[] = { colorMap, bloomMap };
	dc->PSSetShaderResources(0, _countof(srvs), srvs);

	dc->Draw(4, 0);
}

// トーンマッピング処理
void PostEffect::ToneMapping(const RenderContext& rc, ID3D11ShaderResourceView* colorMap)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー設定
	dc->VSSetShader(fullscreenQuadVS.Get(), 0, 0);
	dc->PSSetShader(toneMappingPS.Get(), 0, 0);

	// シェーダーリソース設定
	ID3D11ShaderResourceView* srvs[] = { colorMap };
	dc->PSSetShaderResources(0, _countof(srvs), srvs);

	// 描画
	dc->Draw(4, 0);
}

// 終了処理
void PostEffect::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// 設定されているシェーダーリソースを解除
	ID3D11ShaderResourceView* srvs[] = { nullptr, nullptr };
	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

// デバッグGUI描画
void PostEffect::DrawGUI()
{
	ImGui::Text("Bloom Threshold");

	ImGui::DragFloat(
		"LuminanceLowerEdge",
		&cbPostEffect.luminanceExtractionLowerEdge,
		0.01f,
		0.0f,
		10.0f);

	ImGui::DragFloat(
		"LuminanceHigherEdge",
		&cbPostEffect.luminanceExtractionHigherEdge,
		0.01f,
		0.0f,
		10.0f);

	if (cbPostEffect.luminanceExtractionHigherEdge < cbPostEffect.luminanceExtractionLowerEdge)
	{
		cbPostEffect.luminanceExtractionHigherEdge = cbPostEffect.luminanceExtractionLowerEdge;
	}

	ImGui::Separator();

	ImGui::DragFloat("GaussianSigma", &cbPostEffect.gaussianSigma, 0.01f, 0.01f, 10.0f);
	ImGui::DragFloat("BloomIntensity", &cbPostEffect.bloomIntensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Saturation", &cbPostEffect.saturation, 0.01f, 0.0f, 3.0f);
	ImGui::DragFloat("Exposure", &cbPostEffect.exposure, 0.01f, 0.0f, 5.0f);

	ImGui::Separator();

	static char presetName[128] = "Default";
	static float transitionSeconds = 1.0f;

	ImGui::InputText("Preset Name", presetName, sizeof(presetName));
	ImGui::DragFloat("Transition Seconds", &transitionSeconds, 0.01f, 0.0f, 10.0f);

	if (ImGui::Button("Save"))
	{
		Save(presetName);
	}

	ImGui::SameLine();

	if (ImGui::Button("Load"))
	{
		Load(presetName);
	}

	ImGui::SameLine();

	if (ImGui::Button("TransitionTo"))
	{
		TransitionTo(presetName, transitionSeconds);
	}

	ImGui::Text("Current Preset: %s", currentPresetName.c_str());

	if (isTransitioning)
	{
		float t = 1.0f;
		if (transitionDuration > 0.0f)
		{
			t = Clamp01(transitionTimer / transitionDuration);
		}

		ImGui::Text("Transition Target: %s", targetPresetName.c_str());
		ImGui::Text("Transition: %.0f%%", t * 100.0f);
	}
}

// プリセット即読み込み
bool PostEffect::Load(const std::string& jsonName)
{
	CbPostEffect settings;
	const std::string filename = MakePresetPath(jsonName);

	if (!LoadFromFile(filename, settings))
	{
		return false;
	}

	cbPostEffect = settings;

	isTransitioning = false;
	transitionTimer = 0.0f;
	transitionDuration = 0.0f;
	targetPresetName.clear();

	currentPresetName = jsonName;

	return true;
}

// 現在値をプリセットとして保存
bool PostEffect::Save(const std::string& jsonName) const
{
	const std::string filename = MakePresetPath(jsonName);
	return SaveToFile(filename, cbPostEffect);
}

// プリセットへ徐々に切り替え開始
bool PostEffect::TransitionTo(const std::string& jsonName, float duration)
{
	if (duration <= 0.0f)
	{
		return Load(jsonName);
	}

	CbPostEffect settings;
	const std::string filename = MakePresetPath(jsonName);

	if (!LoadFromFile(filename, settings))
	{
		return false;
	}

	transitionBegin = cbPostEffect;
	transitionEnd = settings;
	transitionTimer = 0.0f;
	transitionDuration = duration;
	isTransitioning = true;

	targetPresetName = jsonName;

	return true;
}

// パス作成
std::string PostEffect::MakePresetPath(const std::string& jsonName) const
{
	std::filesystem::path path(jsonName.empty() ? "Default" : jsonName);

	if (path.extension().empty())
	{
		path += ".json";
	}

	// "Stage00" や "Stage00.json" だけ渡された場合は
	// Data/Config/PostEffect/ の中を見る
	if (path.parent_path().empty())
	{
		path = std::filesystem::path("Data/Config/PostEffect") / path;
	}

	return path.string();
}

// ファイルから読み込み
bool PostEffect::LoadFromFile(const std::string& filename, CbPostEffect& outSettings) const
{
	std::string text;
	if (!ReadTextFile(filename, text))
	{
		return false;
	}

	// 読めなかった項目はデフォルト値を使う
	CbPostEffect settings;

	ExtractJsonFloat(text, "luminanceExtractionLowerEdge", settings.luminanceExtractionLowerEdge);
	ExtractJsonFloat(text, "luminanceExtractionHigherEdge", settings.luminanceExtractionHigherEdge);
	ExtractJsonFloat(text, "gaussianSigma", settings.gaussianSigma);
	ExtractJsonFloat(text, "bloomIntensity", settings.bloomIntensity);
	ExtractJsonFloat(text, "saturation", settings.saturation);
	ExtractJsonFloat(text, "exposure", settings.exposure);

	outSettings = settings;
	return true;
}

// ファイルへ保存
bool PostEffect::SaveToFile(const std::string& filename, const CbPostEffect& settings) const
{
	std::filesystem::path path(filename);

	if (!path.parent_path().empty())
	{
		std::filesystem::create_directories(path.parent_path());
	}

	std::ofstream file(filename);
	if (!file)
	{
		return false;
	}

	file << "{\n";
	file << "  \"version\": 1,\n";
	file << "  \"postEffect\": {\n";
	file << "    \"luminanceExtractionLowerEdge\": " << settings.luminanceExtractionLowerEdge << ",\n";
	file << "    \"luminanceExtractionHigherEdge\": " << settings.luminanceExtractionHigherEdge << ",\n";
	file << "    \"gaussianSigma\": " << settings.gaussianSigma << ",\n";
	file << "    \"bloomIntensity\": " << settings.bloomIntensity << ",\n";
	file << "    \"saturation\": " << settings.saturation << ",\n";
	file << "    \"exposure\": " << settings.exposure << "\n";
	file << "  }\n";
	file << "}\n";

	return true;
}

// テキストファイル読み込み
bool PostEffect::ReadTextFile(const std::string& filename, std::string& outText) const
{
	outText.clear();

	std::ifstream file(filename);
	if (!file)
	{
		return false;
	}

	std::ostringstream stream;
	stream << file.rdbuf();
	outText = stream.str();

	return true;
}

// 簡易JSON float抽出
bool PostEffect::ExtractJsonFloat(const std::string& text, const char* key, float& outValue) const
{
	const std::string quotedKey = "\"" + std::string(key) + "\"";

	size_t keyPosition = text.find(quotedKey);
	if (keyPosition == std::string::npos)
	{
		return false;
	}

	size_t colonPosition = text.find(':', keyPosition + quotedKey.size());
	if (colonPosition == std::string::npos)
	{
		return false;
	}

	const char* valueBegin = text.c_str() + colonPosition + 1;
	char* valueEnd = nullptr;

	float value = std::strtof(valueBegin, &valueEnd);
	if (valueBegin == valueEnd)
	{
		return false;
	}

	outValue = value;
	return true;
}

// 遷移更新
void PostEffect::UpdateTransition(float elapsedTime)
{
	if (!isTransitioning)
	{
		return;
	}

	transitionTimer += elapsedTime;

	float t = 1.0f;
	if (transitionDuration > 0.0f)
	{
		t = Clamp01(transitionTimer / transitionDuration);
	}

	cbPostEffect = LerpSettings(transitionBegin, transitionEnd, t);

	if (t >= 1.0f)
	{
		cbPostEffect = transitionEnd;
		isTransitioning = false;
		transitionTimer = 0.0f;
		transitionDuration = 0.0f;

		currentPresetName = targetPresetName;
		targetPresetName.clear();
	}
}

// 0～1に丸める
float PostEffect::Clamp01(float value) const
{
	if (value < 0.0f)
	{
		return 0.0f;
	}

	if (value > 1.0f)
	{
		return 1.0f;
	}

	return value;
}

// 設定値補間
PostEffect::CbPostEffect PostEffect::LerpSettings(
	const CbPostEffect& a,
	const CbPostEffect& b,
	float t) const
{
	t = Clamp01(t);

	CbPostEffect result;

	result.luminanceExtractionLowerEdge =
		a.luminanceExtractionLowerEdge +
		(b.luminanceExtractionLowerEdge - a.luminanceExtractionLowerEdge) * t;

	result.luminanceExtractionHigherEdge =
		a.luminanceExtractionHigherEdge +
		(b.luminanceExtractionHigherEdge - a.luminanceExtractionHigherEdge) * t;

	result.gaussianSigma =
		a.gaussianSigma +
		(b.gaussianSigma - a.gaussianSigma) * t;

	result.bloomIntensity =
		a.bloomIntensity +
		(b.bloomIntensity - a.bloomIntensity) * t;

	result.saturation =
		a.saturation +
		(b.saturation - a.saturation) * t;

	result.exposure =
		a.exposure +
		(b.exposure - a.exposure) * t;

	return result;
}