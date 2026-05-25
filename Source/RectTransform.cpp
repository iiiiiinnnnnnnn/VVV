// RectTransform.h

#include "RectTransform.h"

RectTransform::RectTransform(const Vector2& position, float angle, const Vector2& size, const Vector2 anchor)
    : position(position), angle(angle), size(size), anchor(anchor)
{
}

void RectTransform::Update()
{
}

RectTransform RectTransform::FromPosition(const Vector2& position)
{
	RectTransform res(position);
	res.Update();
	return res;
}

RectTransform RectTransform::FromAngle(float angle)
{
	RectTransform res(Vector2::Zero, angle);
	res.Update();
	return res;
}

RectTransform RectTransform::FromSize(const Vector2& size)
{
	RectTransform res(Vector2::Zero, 0, size);
	res.Update();
	return res;
}

RectTransform RectTransform::FromSize(float size)
{
	RectTransform res(Vector2::Zero, 0, Vector2(size, size));
	res.Update();
	return res;
}