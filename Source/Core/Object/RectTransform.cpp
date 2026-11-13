#include "Core/Object/RectTransform.h"

RectTransform::RectTransform(const Vector2& position, float angle, const Vector2& size, const Vector2 anchor)
    : position(position), angle(angle), size(size), anchor(anchor)
{
}

RectTransform RectTransform::FromPosition(const Vector2& position)
{
	RectTransform res(position);
	return res;
}

RectTransform RectTransform::FromAngle(float angle)
{
	RectTransform res(Vector2::Zero, angle);
	return res;
}

RectTransform RectTransform::FromSize(const Vector2& size)
{
	RectTransform res(Vector2::Zero, 0, size);
	return res;
}

RectTransform RectTransform::FromSize(float size)
{
	RectTransform res(Vector2::Zero, 0, Vector2(size, size));
	return res;
}
Matrix RectTransform::GetMatrix() const
{
	// アンカーを原点へ移してから拡縮・回転し、指定位置へ配置する
	return Matrix::CreateTranslation(-size.x * anchor.x, -size.y * anchor.y, 0.0f)
		* Matrix::CreateScale(scale.x, scale.y, 1.0f)
		* Matrix::CreateRotationZ(DirectX::XMConvertToRadians(angle))
		* Matrix::CreateTranslation(position.x, position.y, 0.0f);
}
