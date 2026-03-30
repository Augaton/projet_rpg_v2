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
        // --- HUD variables fictives ---
        int fuel = 12, missiles = 5, scrap = 23;
        const char* nomVaisseau = "KLA'ED - BATTLECRUISER";
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
    // On crée le vaisseau, positionné au centre (calculé dynamiquement à chaque frame)
    Ship ship(0, 0); // position temporaire, ignorée

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

        // --- HUD ---
        // Nom du vaisseau (haut centre)
        int nomWidth = MeasureText(nomVaisseau, 22);
        DrawText(nomVaisseau, screenWidth/2 - nomWidth/2, 10, 22, YELLOW);

        // Ressources (haut droite)
        int resX = screenWidth - 180;
        DrawText(TextFormat("Fuel: %d", fuel), resX, 20, 16, GREEN);
        DrawText(TextFormat("Missiles: %d", missiles), resX, 40, 16, ORANGE);
        DrawText(TextFormat("Scrap: %d", scrap), resX, 60, 16, GOLD);

        // Oscillation verticale (lerp sinus)
        t += GetFrameTime();
        float lerpOffset = sin(t * 1.0f) * 20.0f; // amplitude 20px, vitesse modérée
        float destW = ship.getHeight();
        float destH = ship.getWidth();
        float shipX = screenWidth / 2.0f;
        float shipY = screenHeight / 2.0f + lerpOffset;
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

    CloseWindow();
    return 0;
}
