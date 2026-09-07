#pragma once

#include <memory>

#include "UI/Widget.h"

class Entity;
class Texture;

// ボスの検知状態とHP表示をひとつにまとめた専用HUD。
class BossBar final : public Widget
{
public:
	BossBar();

	void Show(Entity* value);
	void Hide(const Entity* value = nullptr);
	Entity* GetBoss() const { return boss; }

protected:
	void OnUpdate() override;
	void OnRender(const RenderContext& rc) override;

private:
	Entity* boss = nullptr;
	std::shared_ptr<Texture> frameTexture;
	std::shared_ptr<Texture> fillTexture;
	float lifeRatio = 1.0f;
	float delayedLifeRatio = 1.0f;
};
