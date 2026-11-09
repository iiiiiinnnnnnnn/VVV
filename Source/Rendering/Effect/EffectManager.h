// EffectManager.h
#pragma once
#include <Dx11Kit/Dx11Kit.h>

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl.h>
#pragma push_macro("Log")
#undef Log
#include <Effekseer.h>
#pragma pop_macro("Log")
#pragma push_macro("Log")
#undef Log
#include <EffekseerRendererDX11.h>
#pragma pop_macro("Log")
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <vector>

#include <string>
#include <string_view>

struct RenderContext;
class Effect;
class Camera;
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
	void Update(const Camera* camera = nullptr);

	// 描画処理
	void Render(const RenderContext& rc);
	void Render2D(const RenderContext& rc, bool affectedByPostProcess);
	static Matrix CreateBillboardTransform(const Matrix& source, const Matrix& view);

	// 再生元とは独立したObjectとしてエフェクトを再生
	Effekseer::Handle PlayDetached(const std::shared_ptr<Effect>& effect, const Matrix& transform, bool billboard = false);
	Effekseer::Handle Play2D(const std::shared_ptr<Effect>& effect, const Vector2& screenPosition,
		float scale = 1.0f, bool affectedByPostProcess = false);
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
	std::unique_ptr<Gpu::ConstantBuffer> fogConstantBuffer;

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
		int layer = 0; // 0 => 3D, 1 => 2D (affected by post process), 2 => 2D (not affected by post process)
	};

	Effekseer::ManagerRef effekseerManager = nullptr;
	EffekseerRenderer::RendererRef effekseerRenderer = nullptr;
	std::unordered_map<std::string, std::vector<uint8_t>> effectData;
	std::vector<std::shared_ptr<Object>> playbackObjects;
	std::vector<EffectPlayback> effectPlaybacks;
};
