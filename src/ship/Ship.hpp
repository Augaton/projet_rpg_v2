// Ship.hpp - Définition de la classe Ship
#pragma once
#include "raylib.h"
#include "../lib/raylib-aseprite.h"

class Ship {
public:
    Aseprite sprite;
    float posX, posY;
    bool textureLoaded = false;

    Ship(float x, float y, const char* fileName);
    const Aseprite& GetSprite() const { return sprite; }
    void Unload();
    ~Ship();
    void Draw() const;
};
