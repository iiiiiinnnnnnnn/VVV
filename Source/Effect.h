#pragma once

#include "Common.h"
#include <DirectXMath.h>
#include <Effekseer.h>

// エフェクト
class Effect
{
public:
	Effect(const char* filename);
	~Effect();

	// 再生
	Effekseer::Handle Play(const Vector3& position, float scale = 1.0f);

	// 停止
	void Stop(Effekseer::Handle handle);

	// 座標設定
	void SetPosition(Effekseer::Handle handle, const Vector3& position);

	// スケール設定
	void SetScale(Effekseer::Handle handle, const Vector3& scale);

private:
	Effekseer::EffectRef effekseerEffect = nullptr;
};
