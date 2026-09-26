// Effect.h
#pragma once

#include "Core/Foundation/Common.h"
#include <DirectXMath.h>
#include <Effekseer.h>

// エフェクト
class Effect
{
public:
	Effect(const char* filename);
	Effect(const void* data, size_t size);
	~Effect();
	static bool IsPackageValid(const void* data, size_t size);

	// 再生
	Effekseer::Handle Play(const Vector3& position, float scale = 1.0f);

	// 停止
	void Stop(Effekseer::Handle handle);

	// 座標設定
	void SetPosition(Effekseer::Handle handle, const Vector3& position);

	// スケール設定
	void SetScale(Effekseer::Handle handle, const Vector3& scale);
	void SetTransform(Effekseer::Handle handle, const Matrix& transform);
	bool IsValid() const { return effekseerEffect != nullptr; }

private:
	Effekseer::EffectRef effekseerEffect = nullptr;
};
