// GameDefine.h

#pragma once

namespace Layer
{
    constexpr int Default = 0;
    constexpr int Player = 1;
    constexpr int Weapon = 2;
    constexpr int Enemy = 3;
    constexpr int Stage = 4;
    constexpr int Body = 5;
    constexpr int Hair = 6;
    constexpr int EnemyM = 7;
    constexpr int Prop = 8;
    constexpr int Count = 9;

    // 衝突マトリックス
    constexpr bool CollisionMatrix[Count][Count] = {
        //             Default  Player  Weapon  Enemy   Stage   Body    Hair    EnemyM   Prop
        /* Default */ { true,   true,   true,   true,   true,   false,  false,  true,   true },
        /* Player  */ { true,   true,   false,  true,   true,   false,  false,  true,   true },
        /* Weapon  */ { true,   true,   true,   false,  false,  false,  false,  false,  true },
        /* Enemy   */ { true,   true,   false,  false,  true,   false,  false,  false,  true },
        /* Stage   */ { true,   true,   false,  false,  false,  false,  false,  false,  true },
        /* Body    */ { false,  false,  false,  false,  false,  false,  true,   false,  true },
        /* Hair    */ { false,  false,  false,  false,  false,  true,   false,  false,  true },
        /* EnemyM  */ { true,   true,   false,  false,  false,  false,  false,  false,  true },
        /* Prop    */ { true,   true,   true,   true,   true,   true,   true,   true,   true }
    };

    // A と B が衝突するか（対称参照）
    inline bool Collides(int a, int b)
    {
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        return CollisionMatrix[lo][hi];
    }
}
