// Time.h
#pragma once

namespace Game
{
	class Time
	{
	public:
		static float time; // 起動からの累計時間
		static float scale; // 時間のスケール
		static float deltaTime; // 前フレームからの経過時間
		static float unscaledDeltaTime; // 起動からの累計時間（スケールの影響を受けない）
	};
}