#pragma once
#include "raylib.h"

enum class ProjType {
    BULLET,
    BIG_BULLET,
    LASER,
    TORPEDO,
    WAVE
};

struct Projectile {
    ProjType type;
    Vector2  pos;
    Vector2  vel;
    float    rotation;
    float    life;
    bool     active;
    bool     isEnemy = false;
};