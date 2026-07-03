// Easing.h

#pragma once

#include <algorithm>
#include <cmath>

class Easing
{
public:
	enum class Type
	{
		Linear,

		InQuad,
		OutQuad,
		InOutQuad,

		InCubic,
		OutCubic,
		InOutCubic,

		InSine,
		OutSine,
		InOutSine
	};
	static inline float Evaluate(float t, Type type)
	{
		t = std::clamp(t, 0.0f, 1.0f);

		switch (type)
		{
			case Type::Linear:
				return t;

			case Type::InQuad:
				return t * t;

			case Type::OutQuad:
			{
				const float inv = 1.0f - t;
				return 1.0f - inv * inv;
			}

			case Type::InOutQuad:
				if (t < 0.5f)
				{
					return 2.0f * t * t;
				}
				else
				{
					const float inv = -2.0f * t + 2.0f;
					return 1.0f - inv * inv * 0.5f;
				}

			case Type::InCubic:
				return t * t * t;

			case Type::OutCubic:
			{
				const float inv = 1.0f - t;
				return 1.0f - inv * inv * inv;
			}

			case Type::InOutCubic:
				if (t < 0.5f)
				{
					return 4.0f * t * t * t;
				}
				else
				{
					const float inv = -2.0f * t + 2.0f;
					return 1.0f - inv * inv * inv * 0.5f;
				}

			case Type::InSine:
				return 1.0f - std::cos((t * 3.1415926535f) * 0.5f);

			case Type::OutSine:
				return std::sin((t * 3.1415926535f) * 0.5f);

			case Type::InOutSine:
				return -(std::cos(3.1415926535f * t) - 1.0f) * 0.5f;
		}

		return t;
	}
};