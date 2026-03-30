// Ship.hpp - Définition de la classe Ship
#pragma once
#include "raylib.h"

class Ship {
public:
    Ship(float x, float y);
    ~Ship();
    void Draw() const;
    float getWidth() const { return width; }
    float getHeight() const { return height; }
    bool isTextureLoaded() const { return textureLoaded; }
    Texture2D texture;
private:
    float posX, posY;
    float width = 100;
    float height = 40;
    bool textureLoaded = false;
};
