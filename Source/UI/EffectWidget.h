#pragma once
#include <algorithm>
#include <memory>
#include <vector>

#include "UI/Widget.h"

class Effect;

class EffectWidget : public Widget
{
  public:
	void OnAwake() override;
	EffectWidget(const std::string& path, float scale = 1);
	~EffectWidget() override;
	void Play();
	void Stop();
	void SetUseUnscaledTime(bool value);
	void Update() override;

	void SetEffectPath(const std::string& path)
	{
		if (effectPath == path) return;
		effectPath = path;
		effect.reset();
	}
	const std::string& GetEffectPath() const
	{
		return effectPath;
	}

	void SetEffectScale(float scale)
	{
		effectScale = std::max(scale, 0.0f);
	}

	float GetEffectScale() const
	{
		return effectScale;
	}

private:
	struct Playback
	{
		std::shared_ptr<Effect> effect;
		int handle = -1;
	};
	Vector2 GetEffectPosition() const;
	std::vector<Playback> playbacks;
	std::shared_ptr<Effect> effect;
	std::string effectPath = "";
	float effectScale = 1.0f;
	bool useUnscaledTime = false;
};
