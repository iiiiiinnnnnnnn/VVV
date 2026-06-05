// GameDefine.h

#pragma once

namespace Layer {
    constexpr int Default = 0;
    constexpr int Player  = 1;
    constexpr int Weapon  = 2;
    constexpr int Enemy   = 3;
    constexpr int Stage   = 4;
    constexpr int Count   = 5;

    // 衝突マトリックス（上三角のみ定義、片方設定すれば対称になる）
    //                 Default  Player  Weapon  Enemy   Stage
    constexpr bool CollisionMatrix[Count][Count] = {
        /* Default */ {  true,   true,   true,   true,   true  },
        /* Player  */ {          true,   false,  true,   true  },
        /* Weapon  */ {                  true,   true,   false },
        /* Enemy   */ {                          true,   false },
        /* Stage   */ {                                  true  },
    };

    // A と B が衝突するか（対称参照）
    inline bool Collides(int a, int b)
    {
        int lo = a < b ? a : b;
        int hi = a < b ? b : a;
        return CollisionMatrix[lo][hi];
    }
}