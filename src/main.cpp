// main.cpp - Point d'entrée du jeu FTL-like avec Raylib
// Auteur : Votre nom
// Date : 2026
// Ce fichier initialise une fenêtre Raylib et affiche un écran de base.



#include "raylib.h"
#include "Ship.hpp"
#include <stdlib.h>
#include <time.h>
#include <math.h>



#define WIDTH 800
#define HEIGHT 600
#define STARS 200
#define SCROLL_SPEED 2

typedef struct {
    float x, y, z;
} Star;

float randf() {
    return (rand() % 1000) / 1000.0f;
}

int main() {
    srand(time(0));

    InitWindow(WIDTH, HEIGHT, "FTL-like avec Raylib");
    ToggleFullscreen();
    SetTargetFPS(60);

    // Fond étoilé
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    Star stars[STARS] = {0};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = GetRandomValue(0, screenWidth);
        stars[i].y = GetRandomValue(0, screenHeight);
        stars[i].z = randf();
    }

    // Création du vaisseau de base (centré comme dans FTL)
    Ship ship(0, 0); // position temporaire, ignorée
    // Charge la texture moteur (engine)
    Texture2D engineTex = LoadTexture("asset/Engine/PNGs/Kla'ed - Battlecruiser - Engine.png");

    // Boucle principale du jeu
    float t = 0.0f;
    while (!WindowShouldClose()) {
        // Mettre à jour la taille de l'écran si besoin
        screenWidth = GetScreenWidth();
        screenHeight = GetScreenHeight();
        // Scroll des étoiles
        for (int i = 0; i < STARS; i++) {
            stars[i].x -= SCROLL_SPEED * (stars[i].z / 1);
            if (stars[i].x <= 0) {
                stars[i].x += screenWidth;
                stars[i].y = GetRandomValue(0, screenHeight);
            }
        }

        BeginDrawing();
        ClearBackground((Color){0, 0, 0, 255});
        // Dessin des étoiles
        for (int i = 0; i < STARS; i++) {
            DrawPixel(stars[i].x, stars[i].y, WHITE);
        }
        
        // Oscillation verticale (lerp sinus)
        t += GetFrameTime();
        float lerpOffset = sin(t * 1.0f) * 20.0f; // amplitude 20px, vitesse modérée
        float destW = ship.getHeight();
        float destH = ship.getWidth();
        float shipX = screenWidth / 2.0f;
        float shipY = screenHeight / 2.0f + lerpOffset;
        // Effet moteur animé (sous le vaisseau)
        if (ship.isTextureLoaded() && engineTex.id > 0) {
            // Spritesheet moteur : 12 frames horizontales
            const int engineFrames = 12;
            int frameW = engineTex.width / engineFrames;
            int frameH = engineTex.height;
            int frameIdx = ((int)(t * 6.0f)) % engineFrames; // 18 fps
            Rectangle srcRect = { (float)(frameW * frameIdx), 0.0f, (float)frameW, (float)frameH };
            // Animation moteur : variation alpha et taille
            float engineAlpha = 0.7f + 0.3f;
            float engineScale = 1.0f + 0.1f;
            float engineW = frameW * engineScale;
            float engineH = frameH * engineScale;
            float engineX = shipX + 3.9f;
            float engineY = shipY;
            Color engineColor = WHITE;
            engineColor.a = (unsigned char)(engineAlpha * 255);
            DrawTexturePro(
                engineTex,
                srcRect,
                (Rectangle){engineX, engineY, engineW, engineH},
                (Vector2){engineW/2.0f, engineH/2.0f},
                90.0f,
                engineColor
            );
        }
        // Vaisseau
        if (ship.isTextureLoaded()) {
            DrawTexturePro(
                ship.texture,
                (Rectangle){0, 0, ship.getWidth(), ship.getHeight()},
                (Rectangle){shipX, shipY, destW, destH},
                (Vector2){destW/2.0f, destH/2.0f},
                90.0f,
                WHITE
            );
        } else {
            DrawRectangle(shipX - destW/2.0f, shipY - destH/2.0f, destW, destH, BLUE);
        }

        EndDrawing();
    }

    UnloadTexture(engineTex);
    CloseWindow();
    return 0;
}
