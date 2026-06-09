// GameDefine.h

#pragma once

namespace Layer {
    constexpr int Default = 0;
    constexpr int Player  = 1;
    constexpr int Weapon  = 2;
    constexpr int Enemy   = 3;
    constexpr int Stage   = 4;
    constexpr int Body    = 5;
    constexpr int Hair    = 6;
    constexpr int Count   = 7;

    // 衝突マトリックス
    constexpr bool CollisionMatrix[Count][Count] = {
        //            Default  Player  Weapon  Enemy   Stage   Body    Hair
        /* Default */{ true,   true,   true,   true,   true,   false,  false },
        /* Player  */{ true,   true,   false,  true,   true,   false,  false },
        /* Weapon  */{ true,   true,   true,   false,  false,  false,  false },
        /* Enemy   */{ true,   true,   false,  false,  false,  false,  false },
        /* Stage   */{ true,   true,   false,  false,  false,  false,  false },
        /* Body    */{ false,  false,  false,  false,  false,  false,  true  },
        /* Hair    */{ false,  false,  false,  false,  false,  true,   false },
    };

    // A と B が衝突するか（対称参照）
    inline bool Collides(int a, int b)
    {
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        return CollisionMatrix[lo][hi];
    }
}