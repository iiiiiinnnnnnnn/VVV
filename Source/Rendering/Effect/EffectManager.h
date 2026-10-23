// EffectManager.h
#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>
#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include <string>
#include <string_view>

struct RenderContext;
class Effect;
class Object;

// エフェクトマネージャー
class EffectManager
{
private:
	EffectManager() {}
	~EffectManager() {}

public:
	// 唯一のインスタンス取得
	static EffectManager& Instance()
	{
		static EffectManager instance;
		return instance;
	}

	// 初期化
	void Initialize();

	// 終了化
	void Finalize();

	// 更新処理
	void Update();

	// 描画処理
	void Render(const RenderContext& rc);
	static Matrix CreateBillboardTransform(const Matrix& source, const Matrix& view);

	// 再生元とは独立したObjectとしてエフェクトを再生
	Effekseer::Handle PlayDetached(const std::shared_ptr<Effect>& effect, const Matrix& transform);
	std::shared_ptr<Effect> LoadEffect(std::string_view path);
	static std::vector<std::string> GetEffectPaths();

	// Effeckseerマネージャーの取得
	Effekseer::ManagerRef GetEffekseerManager() { return effekseerManager; }
	EffekseerRenderer::RendererRef GetEffekseerRenderer() { return effekseerRenderer; }

private:
	friend class Effect;

	void InitializeFog();
	struct FogConstants
	{
		Matrix inverseViewProjection;
		Vector4 cameraPosition;
		Vector4 viewport;
		Vector4 parameters;
	};
	static_assert(sizeof(FogConstants) % 16 == 0);
	Microsoft::WRL::ComPtr<ID3D11Buffer> fogConstantBuffer;

	void RegisterPlayback(const Effect* effect, Effekseer::Handle handle, bool billboard);
	void SetPlaybackTransform(const Effect* effect, Effekseer::Handle handle, const Matrix& transform);
	void SetPlaybackPosition(const Effect* effect, Effekseer::Handle handle, const Vector3& position);
	void SetPlaybackScale(const Effect* effect, Effekseer::Handle handle, const Vector3& scale);
	void SetBillboard(const Effect* effect, bool value);
	void StopPlayback(const Effect* effect, Effekseer::Handle handle);
	void ReleaseEffect(const Effect* effect);

	struct EffectPlayback
	{
		const Effect* effect = nullptr;
		Effekseer::Handle handle = -1;
		Matrix transform = Matrix::Identity;
		bool billboard = false;
	};

	Effekseer::ManagerRef effekseerManager = nullptr;
	EffekseerRenderer::RendererRef effekseerRenderer = nullptr;
	std::unordered_map<std::string, std::vector<uint8_t>> effectData;
	std::vector<std::shared_ptr<Object>> playbackObjects;
	std::vector<EffectPlayback> effectPlaybacks;
};
