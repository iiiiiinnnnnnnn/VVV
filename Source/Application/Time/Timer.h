#pragma once

#include "Application/Time/GameTime.h"

#include <cmath>
#include <cstddef>
#include <limits>

namespace Game
{
	// Tickで時間を進める周期タイマー。単位は秒。
	class Timer
	{
	public:
		explicit Timer(float seconds = 1.0f)
		{
			Reset(seconds);
		}

		void Reset()
		{
			remaining = duration;
			elapsed = 0.0f;
		}

		// 0以下・非有限の周期は無効として扱う。
		void Reset(float seconds)
		{
			duration = std::isfinite(seconds) && seconds > 0.0f ? seconds : 0.0f;
			Reset();
		}

		// 経過時間の計測。寿命のない演出やストップウォッチにも使用する。
		void Advance(float deltaTime = Time::deltaTime)
		{
			if (std::isfinite(deltaTime) && deltaTime > 0.0f) elapsed += deltaTime;
		}

		float GetElapsed() const { return elapsed; }

		void Stop() { remaining = 0.0f; }
		bool IsRunning() const { return remaining > 0.0f; }

		// 残り時間がsecondsより短い場合だけ延長する。
		void Extend(float seconds)
		{
			if (std::isfinite(seconds) && seconds > remaining) Reset(seconds);
		}

		// 一度きりの待機。終了したフレームだけtrueを返し、0で停止する。
		bool Update(float deltaTime = Time::deltaTime)
		{
			if (!IsRunning() || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return false;
			elapsed += deltaTime >= remaining ? remaining : deltaTime;
			remaining = deltaTime >= remaining ? 0.0f : remaining - deltaTime;
			return !IsRunning();
		}

		// 周期を越えた余りを次回へ引き継ぎ、1回のTickにつき最大1回trueを返す。
		// 引数を省略するとTime::deltaTimeを使い、ポーズ・時間スケールに追従する。
		bool Tick(float deltaTime = Time::deltaTime)
		{
			return TickCount(deltaTime) > 0;
		}

		// 長いフレームで複数周期が経過した場合、その回数を返す。
		std::size_t TickCount(float deltaTime = Time::deltaTime)
		{
			if (!IsRunning() || duration <= 0.0f || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return 0;
			Advance(deltaTime);
			const double rest = static_cast<double>(remaining) - deltaTime;
			if (rest > 0.0)
			{
				remaining = static_cast<float>(rest);
				return 0;
			}
			remaining = static_cast<float>(duration + std::fmod(rest, duration));
			const double count = 1.0 + std::floor(-rest / duration);
			const auto maximum = (std::numeric_limits<std::size_t>::max)();
			return count >= static_cast<double>(maximum) ? maximum : static_cast<std::size_t>(count);
		}

		float GetRemaining() const { return remaining; }
		float GetDuration() const { return duration; }

	private:
		float duration = 1.0f;
		float remaining = 1.0f;
		float elapsed = 0.0f;
	};
}
