// GameDefine.h

#pragma once

namespace Layer {
    constexpr int Default = 0;
    constexpr int Player  = 1;
    constexpr int Weapon  = 2;
    constexpr int Enemy   = 3;
    constexpr int Stage   = 4;
    constexpr int Body    = 5;
    constexpr int Count   = 6;

    // 衝突マトリックス
    constexpr bool CollisionMatrix[Count][Count] = {
        //            Default  Player  Weapon  Enemy   Stage   Body
        /* Default */{ true,   true,   true,   true,   true,   false },
        /* Player  */{ true,   true,   false,  true,   true,   false },
        /* Weapon  */{ true,   true,   true,   false,  false         },
        /* Enemy   */{ true,   true,   false,  false                 },
        /* Stage   */{ true,   true,   false                         },
        /* Body    */{ false,  false,  false,  false,  false,  false },
    };

    // A と B が衝突するか（対称参照）
    inline bool Collides(int a, int b)
    {
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        return CollisionMatrix[lo][hi];
    }
}