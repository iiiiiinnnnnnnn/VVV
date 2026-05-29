// PlayerController.h

#pragma once

#include "Common.h"

class PlayerController {
public:
    virtual float GetMoveX() = 0; // 移動X
	virtual float GetMoveZ() = 0; // 移動Z
	virtual bool GetJump() = 0; // ジャンプ
	virtual bool GetCrouch() = 0; // しゃがみ
	virtual bool GetReady() = 0; // 構え
	virtual bool GetShoot() = 0; // 射撃
	virtual bool GetReload() = 0; // リロード
};
