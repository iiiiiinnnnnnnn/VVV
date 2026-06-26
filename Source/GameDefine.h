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
    constexpr int FootIK = 9;
    constexpr int Count = 10;

    // 衝突マトリックス
    constexpr bool CollisionMatrix[Count][Count] = {
        //             Default  Player  Weapon  Enemy   Stage   Body    Hair    EnemyM   Prop    FootIK
        /* Default */ { true,   true,   true,   true,   true,   false,  false,  true,   true,   true  },
        /* Player  */ { true,   true,   false,  true,   true,   false,  false,  true,   true,   false },
        /* Weapon  */ { true,   true,   true,   false,  false,  false,  false,  false,  true,   false },
        /* Enemy   */ { true,   true,   false,  false,  true,   false,  false,  false,  true,   false },
        /* Stage   */ { true,   true,   false,  false,  false,  false,  false,  false,  true,   true  },
        /* Body    */ { false,  false,  false,  false,  false,  false,  true,   false,  true,   false },
        /* Hair    */ { false,  false,  false,  false,  false,  true,   false,  false,  true,   false },
        /* EnemyM  */ { true,   true,   false,  false,  false,  false,  false,  false,  true,   false },
        /* Prop    */ { true,   true,   true,   true,   true,   true,   true,   true,   true,   true  },
        /* FootIK  */ { false,  false,  false,  false,  true,   false,  false,  false,  false,  false }
    };

    // A と B が衝突するか（対称参照）
    inline bool Collides(int a, int b)
    {
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        return CollisionMatrix[lo][hi];
    }
}
