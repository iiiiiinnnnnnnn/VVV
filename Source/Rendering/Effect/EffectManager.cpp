// EffectManager.cpp
#include "Rendering/Core/Graphics.h"
#include "Rendering/Effect/EffectManager.h"
#include "Rendering/Effect/Effect.h"
#include "Rendering/Core/RenderContext.h"
#include "Resource/GpuResourceUtils.h"
#include "Rendering/Effect/EffekseerOverride/Shader.h"
#include <EffekseerRendererCommon/EffekseerRenderer.Renderer_Impl.h>
#include "Application/Time/GameTime.h"
#include "Core/Object/Object.h"
#include "Resource/ResourceManager.h"

#include "Resource/ResourcePath.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace
{
void SetBaseMatrix(const Effekseer::ManagerRef& manager, Effekseer::Handle handle,
	const Matrix& transform)
{
	if (!manager || handle < 0 || !manager->Exists(handle)) return;
	Effekseer::Matrix43 matrix;
	for (int row = 0; row < 4; ++row)
		for (int column = 0; column < 3; ++column)
			matrix.Value[row][column] = transform.m[row][column];
	manager->SetBaseMatrix(handle, matrix);
}

class DetachedEffectObject final : public Object
{
public:
	DetachedEffectObject(std::shared_ptr<Effect> effect, const Matrix& transform)
		: Object("Detached Effect"), effect(std::move(effect)),
		manager(EffectManager::Instance().GetEffekseerManager())
	{
		if (!this->effect) return;
		handle = this->effect->Play(Vector3::Zero);
		if (handle >= 0) this->effect->SetTransform(handle, transform);
	}

	~DetachedEffectObject() override
	{
		if (effect && manager && handle >= 0 && manager->Exists(handle)) effect->Stop(handle);
	}

	Effekseer::Handle GetHandle() const { return handle; }

protected:
	void OnUpdate() override
	{
		if (!manager || handle < 0 || !manager->Exists(handle)) Destroy();
	}

private:
	std::shared_ptr<Effect> effect;
	Effekseer::ManagerRef manager;
	Effekseer::Handle handle = -1;
};
}

// 初期化
void EffectManager::Initialize()
{
	Game::Graphics& graphics = Game::Graphics::Instance();

	// Effekseerレンダラ生成
	effekseerRenderer = EffekseerRendererDX11::Renderer::Create(graphics.GetDevice(),
		graphics.GetDeviceContext(), 2048);
	InitializeFog();

	// Effekseerマネージャー生成
	effekseerManager = Effekseer::Manager::Create(2048);

	// Effekseerレンダラの各種設定（特別なカスタマイズをしない場合は定型的に以下の設定でOK）
	effekseerManager->SetSpriteRenderer(effekseerRenderer->CreateSpriteRenderer());
	effekseerManager->SetRibbonRenderer(effekseerRenderer->CreateRibbonRenderer());
	effekseerManager->SetRingRenderer(effekseerRenderer->CreateRingRenderer());
	effekseerManager->SetTrackRenderer(effekseerRenderer->CreateTrackRenderer());
	effekseerManager->SetModelRenderer(effekseerRenderer->CreateModelRenderer());
	// Effekseer内でのローダーの設定（特別なカスタマイズをしない場合は以下の設定でOK）
	effekseerManager->SetTextureLoader(effekseerRenderer->CreateTextureLoader());
	effekseerManager->SetModelLoader(effekseerRenderer->CreateModelLoader());
	effekseerManager->SetMaterialLoader(effekseerRenderer->CreateMaterialLoader());

	// Effekseerを左手座標系で計算する
	effekseerManager->SetCoordinateSystem(Effekseer::CoordinateSystem::LH);

}

void EffectManager::InitializeFog()
{
	auto* impl = effekseerRenderer->GetImpl();
	const auto replace = [this](std::unique_ptr<EffekseerRenderer::ShaderBase>& shader,
		EffekseerRenderer::RendererShaderType type, const char* name) {
		const std::string base = std::string("Resources/Shader/Effekseer") + name;
		const auto vs = GpuResourceUtils::LoadBinaryFile((base + "VS.cso").c_str());
		const auto ps = GpuResourceUtils::LoadBinaryFile((base + "PS.cso").c_str());
		auto replacement = effekseerRenderer->GetGraphicsDevice()->CreateShaderFromBinary(
			vs.data(), static_cast<int32_t>(vs.size()),
			ps.data(), static_cast<int32_t>(ps.size()));
		if (!replacement) throw std::runtime_error("Failed to create effect fog shader: " + base);
		auto device = effekseerRenderer->GetGraphicsDevice();
		auto layout = EffekseerRenderer::GetVertexLayout(device, type);
		std::unique_ptr<EffekseerRendererDX11::Shader> fogShader(
			EffekseerRendererDX11::Shader::Create(device, replacement, layout, name));
		if (!fogShader) throw std::runtime_error("Failed to create effect fog layout: " + base);
		const auto* original = static_cast<EffekseerRendererDX11::Shader*>(shader.get());
		fogShader->SetVertexConstantBufferSize(original->GetVertexConstantBufferSize());
		fogShader->SetPixelConstantBufferSize(original->GetPixelConstantBufferSize());
		// UnlitのOverrideShader(nullptr)でもフォグ対応版が使われるよう、本体を置き換える。
		shader = std::move(fogShader);
	};
	replace(impl->ShaderUnlit, EffekseerRenderer::RendererShaderType::Unlit, "Unlit");
	replace(impl->ShaderLit, EffekseerRenderer::RendererShaderType::Lit, "Lit");
	replace(impl->ShaderDistortion, EffekseerRenderer::RendererShaderType::BackDistortion, "Distortion");
	replace(impl->ShaderAdUnlit, EffekseerRenderer::RendererShaderType::AdvancedUnlit, "AdvancedUnlit");
	replace(impl->ShaderAdLit, EffekseerRenderer::RendererShaderType::AdvancedLit, "AdvancedLit");
	replace(impl->ShaderAdDistortion, EffekseerRenderer::RendererShaderType::AdvancedBackDistortion, "AdvancedDistortion");
	if (FAILED(GpuResourceUtils::CreateConstantBuffer(Game::Graphics::Instance().GetDevice(),
		sizeof(FogConstants), fogConstantBuffer.ReleaseAndGetAddressOf())))
		throw std::runtime_error("Failed to create effect fog constant buffer");
}

// 終了化
void EffectManager::Finalize()
{
	playbackObjects.clear();
	effectPlaybacks.clear();
	effectData.clear();
	fogConstantBuffer.Reset();
}

// 更新処理
void EffectManager::Update()
{
	// エフェクト更新処理（引数にはフレームの経過時間を渡す）
	effekseerManager->Update(Game::Time::deltaTime * 60.0f);
	for (const auto& object : playbackObjects)
		if (object && !object->IsPendingDestroy()) object->Update();
	std::erase_if(playbackObjects, [](const std::shared_ptr<Object>& object)
	{
		return !object || object->IsPendingDestroy();
	});
	std::erase_if(effectPlaybacks, [this](const EffectPlayback& playback)
	{
		return !effekseerManager || playback.handle < 0 ||
			!effekseerManager->Exists(playback.handle);
	});
}

Effekseer::Handle EffectManager::PlayDetached(
	const std::shared_ptr<Effect>& effect, const Matrix& transform, bool billboard)
{
	if (!effect || !effect->IsValid()) return -1;
	auto object = std::make_shared<DetachedEffectObject>(effect, transform);
	const Effekseer::Handle handle = object->GetHandle();
	if (handle < 0) return -1;
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect.get() || playback.handle != handle) continue;
		playback.billboard = billboard;
		break;
	}
	object->Awake();
	object->Start();
	playbackObjects.push_back(std::move(object));
	return handle;
}

std::vector<std::string> EffectManager::GetEffectPaths()
{
	auto root = ResourceManager::FindSourceResourceRoot();
	if (root.empty()) root = std::filesystem::current_path() / "Resources";
	root /= "Effect";
	std::vector<std::string> paths;
	std::error_code error;
	for (std::filesystem::recursive_directory_iterator iterator(root, error), end;
		!error && iterator != end; iterator.increment(error))
	{
		if (!iterator->is_regular_file(error)) continue;
		auto extension = ResourcePath::Text(iterator->path().extension());
		std::transform(extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character) { return static_cast<char>(std::tolower(character)); });
		if (extension != ".efkpkg") continue;
		auto relative = iterator->path().lexically_relative(root);
		paths.push_back(ResourceManager::ShortResourcePath(ResourcePath::Text(relative), "Effect"));
	}
	std::sort(paths.begin(), paths.end());
	return paths;
}

std::shared_ptr<Effect> EffectManager::LoadEffect(std::string_view effectPath)
{
	if (effectPath.empty()) throw std::runtime_error("Effect path is empty");
	const auto path = std::filesystem::weakly_canonical(ResourceManager::ResolveSourcePath(
		ResourcePath::Path(ResourceManager::Instance().ResolveResourcePath(effectPath, ResourceManager::ResourceType::Effect))));
	const std::string key = ResourcePath::Text(path);
	const std::vector<uint8_t>* data = nullptr;
	if (const auto found = effectData.find(key); found != effectData.end()) data = &found->second;
	else
	{
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input) throw std::runtime_error("Effect file was not found: " + key);
		const std::streamsize size = input.tellg();
		if (size <= 0) throw std::runtime_error("Effect file is empty: " + key);
		input.seekg(0);
		std::vector<uint8_t> loadedData(static_cast<size_t>(size));
		if (!input.read(reinterpret_cast<char*>(loadedData.data()), size))
			throw std::runtime_error("Effect file read failed: " + key);
		data = &effectData.emplace(key, std::move(loadedData)).first->second;
	}
	auto loaded = std::shared_ptr<Effect>(new Effect(data->data(), data->size()));
	if (!loaded->IsValid()) throw std::runtime_error("Effect load failed: " + key);
	return loaded;
}

void EffectManager::RegisterPlayback(
	const Effect* effect, Effekseer::Handle handle, bool billboard)
{
	if (!effect || handle < 0) return;
	std::erase_if(effectPlaybacks, [handle](const EffectPlayback& playback)
	{
		return playback.handle == handle;
	});
	effectPlaybacks.push_back({effect, handle, Matrix::Identity, billboard});
}

void EffectManager::SetPlaybackTransform(
	const Effect* effect, Effekseer::Handle handle, const Matrix& transform)
{
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect || playback.handle != handle) continue;
		playback.transform = transform;
		SetBaseMatrix(effekseerManager, handle, transform);
		return;
	}
}

Matrix EffectManager::CreateBillboardTransform(const Matrix& source, const Matrix& view)
{
	const Vector3 sourceRight(source._11, source._12, source._13);
	const Vector3 sourceUp(source._21, source._22, source._23);
	const Vector3 sourceFront(source._31, source._32, source._33);
	const float scaleX = sourceRight.Length();
	const float scaleY = sourceUp.Length();
	const float scaleZ = sourceFront.Length();
	const Vector3 position(source._41, source._42, source._43);

	// 元の回転を捨ててカメラの各軸へ置き換える
	const Matrix cameraTransform = view.Invert();
	const Vector3 right =
		Vector3(cameraTransform._11, cameraTransform._12, cameraTransform._13) * scaleX;
	const Vector3 up =
		Vector3(cameraTransform._21, cameraTransform._22, cameraTransform._23) * scaleY;
	const Vector3 front =
		Vector3(cameraTransform._31, cameraTransform._32, cameraTransform._33) * scaleZ;
	return Matrix(
		right.x, right.y, right.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		front.x, front.y, front.z, 0.0f,
		position.x, position.y, position.z, 1.0f);
}

void EffectManager::SetPlaybackPosition(
	const Effect* effect, Effekseer::Handle handle, const Vector3& position)
{
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect || playback.handle != handle) continue;
		playback.transform._41 = position.x;
		playback.transform._42 = position.y;
		playback.transform._43 = position.z;
		SetBaseMatrix(effekseerManager, handle, playback.transform);
		return;
	}
}

void EffectManager::SetPlaybackScale(
	const Effect* effect, Effekseer::Handle handle, const Vector3& scale)
{
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect || playback.handle != handle) continue;
		[[maybe_unused]] Vector3 currentScale;
		Quaternion rotation;
		Vector3 position;
		Matrix matrix = playback.transform;
		if (!matrix.Matrix::Decompose(currentScale, rotation, position)) return;
		playback.transform = Matrix::CreateScale(scale) *
			Matrix::CreateFromQuaternion(rotation) * Matrix::CreateTranslation(position);
		SetBaseMatrix(effekseerManager, handle, playback.transform);
		return;
	}
}

void EffectManager::SetBillboard(const Effect* effect, bool value)
{
	if (!effect) return;
	for (EffectPlayback& playback : effectPlaybacks)
	{
		if (playback.effect != effect) continue;
		playback.billboard = value;
		if (!value) SetBaseMatrix(effekseerManager, playback.handle, playback.transform);
	}
}

void EffectManager::StopPlayback(const Effect* effect, Effekseer::Handle handle)
{
	std::erase_if(effectPlaybacks, [effect, handle](const EffectPlayback& playback)
	{
		return playback.effect == effect && playback.handle == handle;
	});
}

void EffectManager::ReleaseEffect(const Effect* effect)
{
	std::erase_if(effectPlaybacks, [effect](const EffectPlayback& playback)
	{
		return playback.effect == effect;
	});
}

// 描画処理
void EffectManager::Render(const RenderContext& rc)
{
	if (!rc.camera) return;
	const Matrix& view = rc.camera->GetView();
	const Matrix& projection = rc.camera->GetProjection();
	const RenderSettings& settings = rc.renderSettings;
	// ビュー＆プロジェクション行列をEffekseerレンダラに設定
	effekseerRenderer->SetCameraMatrix(*reinterpret_cast<const Effekseer::Matrix44*>(&view));
	effekseerRenderer->SetProjectionMatrix(*reinterpret_cast<const Effekseer::Matrix44*>(&projection));
	for (const EffectPlayback& playback : effectPlaybacks)
	{
		if (!effekseerManager->Exists(playback.handle)) continue;
		if (playback.billboard)
			SetBaseMatrix(effekseerManager, playback.handle,
				EffectManager::CreateBillboardTransform(playback.transform, view));


	}

	// Effekseer描画開始
	effekseerRenderer->BeginRendering();

	ID3D11DeviceContext* dc = rc.deviceContext;
	D3D11_VIEWPORT viewport{};
	UINT count = 1;
	dc->RSGetViewports(&count, &viewport);
	FogConstants fog{};
	fog.inverseViewProjection = (view * projection).Invert();
	const Vector3 eye = rc.camera->GetEye();
	fog.cameraPosition = {eye.x, eye.y, eye.z, 1.0f};
	fog.viewport = {viewport.TopLeftX, viewport.TopLeftY,
		1.0f / std::max(viewport.Width, 1.0f), 1.0f / std::max(viewport.Height, 1.0f)};
	fog.parameters = {settings.distanceFogStart, settings.distanceFogEnd,
		settings.distanceFogStrength, settings.showFog && settings.distanceFogEnabled ? 1.0f : 0.0f};
	Microsoft::WRL::ComPtr<ID3D11Buffer> previousFogBuffer;
	dc->PSGetConstantBuffers(13, 1, previousFogBuffer.GetAddressOf());
	dc->UpdateSubresource(fogConstantBuffer.Get(), 0, nullptr, &fog, 0, 0);
	dc->PSSetConstantBuffers(13, 1, fogConstantBuffer.GetAddressOf());

	// Effekseer描画実行
	// マネージャー単位で描画するので描画順を制御する場合はマネージャーを複数個作成し、
	// Draw()関数を実行する順序で制御できそう
	effekseerManager->Draw();

	// Effekseer描画終了
	effekseerRenderer->EndRendering();
	dc->PSSetConstantBuffers(13, 1, previousFogBuffer.GetAddressOf());
}
