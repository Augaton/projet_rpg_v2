#pragma once
#include "raylib.h"

enum class ProjType {
    BULLET,
    BIG_BULLET,
    LASER,
    TORPEDO,
    WAVE,
    ROCK     // astéroïde (danger de balise)
};

struct Projectile {
    ProjType type;
    Vector2  pos;
    Vector2  vel;
    float    rotation;
    float    life;
    bool     active    = true;
    bool     isEnemy   = false;

    // Ciblage : le projectile s'immobilise sur `target` et lève `arrived`.
    // C'est au combat de décider de la suite (impact, bouclier, raté…).
    bool     hasTarget = false;
    bool     arrived   = false;
    Vector2  target    = { 0, 0 };
    int      shotId    = -1;
};
