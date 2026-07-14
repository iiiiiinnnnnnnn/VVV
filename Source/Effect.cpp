// Effect.cpp

#include "Graphics.h"
#include "Effect.h"
#include "EffectManager.h"
#include <mutex>

// コンストラクタ
Effect::Effect(const char* filename)
{
	// エフェクトを読み込みする前にロックする
	// ※マルチスレッドでEffectを作成するとDeviceContextを同時アクセスして
	// 　フリーズする可能性があるので排他制御する
	//std::lock_guard<std::mutex> lock(Game::Graphics::Instance().());

	// Effekseerのリソースを読み込む
	// EffekseerはUTF-16のファイルパス以外は対応していないため文字コード変換が必要
	char16_t utf16Filename[256];
	Effekseer::ConvertUtf8ToUtf16(utf16Filename, 256, filename);

	// Effekseer::Managerを取得
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	// Effekseerエフェクトを読み込み
	effekseerEffect = Effekseer::Effect::Create(effekseerManager, (EFK_CHAR*)utf16Filename);
}

// デストラクタ
Effect::~Effect()
{
}

// 再生
Effekseer::Handle Effect::Play(const Vector3& position, float scale)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	Effekseer::Handle handle = effekseerManager->Play(effekseerEffect, position.x, position.y, position.z);
	effekseerManager->SetScale(handle, scale, scale, scale);
	return handle;
}

// 停止
void Effect::Stop(Effekseer::Handle handle)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	effekseerManager->StopEffect(handle);
}

// 座標設定
void Effect::SetPosition(Effekseer::Handle handle, const Vector3& position)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	effekseerManager->SetLocation(handle, position.x, position.y, position.z);
}

// スケール設定
void Effect::SetScale(Effekseer::Handle handle, const Vector3& scale)
{
	Effekseer::ManagerRef effekseerManager = EffectManager::Instance().GetEffekseerManager();

	effekseerManager->SetScale(handle, scale.x, scale.y, scale.z);
}
