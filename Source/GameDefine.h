// GameDefine.h

#pragma once

namespace Layer {
    constexpr int Default = 0;
    constexpr int Player  = 1;
    constexpr int Weapon  = 2;
    constexpr int Enemy   = 3;
    constexpr int Stage   = 4;
    constexpr int Count   = 5;

    // 衝突マトリックス
    constexpr bool CollisionMatrix[Count][Count] = {
        //              Default  Player  Weapon  Enemy   Stage
        /* Default */ {  true,   true,   true,   true,   true  },
        /* Player  */ {  true,   true,   false,  true,   true  },
        /* Weapon  */ {  true,           true,   true,   false },
        /* Enemy   */ {  true,                   true,   false },
        /* Stage   */ {  true,                           true  },
    };

    // A と B が衝突するか（対称参照）
    inline bool Collides(int a, int b)
    {
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        return CollisionMatrix[lo][hi];
    }
}