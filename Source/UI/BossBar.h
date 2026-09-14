#pragma once

#include <memory>

#include "UI/GaugeHUD.h"

class Entity;
class Texture;

// ボスの検知状態とHP表示をひとつにまとめた専用HUD。
class BossBar final : public GaugeHUD
{
public:
	BossBar();

	void Show(Entity* value);
	void Hide(const Entity* value = nullptr);
	Entity* GetBoss() const { return boss; }

protected:
	void OnUpdate() override;

private:
	Entity* boss = nullptr;
};
