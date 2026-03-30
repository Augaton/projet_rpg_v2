// Ship.cpp - Implémentation de la classe Ship
#include "Ship.hpp"


Ship::Ship(float x, float y) : posX(x), posY(y) {
    // Charge la texture du Battlecruiser
    texture = LoadTexture("asset/Base/PNGs/Kla'ed - Battlecruiser - Base.png");
    if (texture.id > 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR); // Teste un filtre plus doux
        textureLoaded = true;
        width = texture.width;
        height = texture.height;
    }
}

Ship::~Ship() {
    if (textureLoaded) UnloadTexture(texture);
}

void Ship::Draw() const {
    if (textureLoaded) {
        // Pivot de 90° vers la droite (sens horaire)
        Vector2 center = {width / 2.0f, height / 2.0f};
        DrawTexturePro(
            texture,
            (Rectangle){0, 0, (float)width, (float)height},
            (Rectangle){posX + width / 2.0f, posY + height / 2.0f, (float)height, (float)width},
            center,
            90.0f,
            WHITE
        );
    } else {
        // Fallback : rectangle simple
        DrawRectangle(posX, posY, width, height, BLUE);
    }
}
