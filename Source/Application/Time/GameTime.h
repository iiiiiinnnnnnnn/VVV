#pragma once

namespace Game
{
	class Time
	{
	public:
		static float time; // 起動してからの経過時間
		static float scale; // 時間のスケール
		static float deltaTime; // 前フレームからの経過時間
		static float unscaledDeltaTime; // 時間スケールの影響を受けない経過時間
	};
}
