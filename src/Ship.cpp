// Ship.cpp - Implémentation de la classe Ship
#include "Ship.hpp"
#include "raylib.h"
#include "lib/raylib-aseprite.h"


Ship::Ship(float x, float y, const char* fileName) : posX(x), posY(y) {
    // On charge directement le fichier .aseprite
    sprite = LoadAseprite(fileName);
    
    if (IsAsepriteValid(sprite)) {
        textureLoaded = true;
    }
}

Ship::~Ship() {
    Unload();
}

void Ship::Unload() {
    if (IsAsepriteValid(sprite)) {
        UnloadAseprite(sprite);
    }
}

void Ship::Draw() const {
    if (textureLoaded) {
        // Dimensions du sprite
        float w = (float)GetAsepriteWidth(sprite);
        float h = (float)GetAsepriteHeight(sprite);

        // Destination : là où on dessine (centré sur posX, posY)
        Rectangle dest = { posX, posY, w, h };
        
        // Origine : le centre du sprite pour la rotation à 90°
        Vector2 origin = { w / 2.0f, h / 2.0f };

        // On utilise la fonction de dessin spécifique à Aseprite
        // Frame 0 par défaut pour le vaisseau de base
        DrawAsepritePro(sprite, 0, dest, origin, 90.0f, WHITE);
    } else {
        // Fallback si le fichier est manquant
        DrawRectangle(posX - 20, posY - 20, 40, 40, BLUE);
    }
}