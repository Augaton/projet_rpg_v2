// main.cpp - Point d'entrée du jeu FTL-like avec Raylib
// Auteur : Votre nom
// Date : 2026
// Ce fichier initialise une fenêtre Raylib et affiche un écran de base.



#include "raylib.h"
#include "Ship.hpp"
#include <stdlib.h>
#include <time.h>



#define WIDTH 800
#define HEIGHT 600
#define STARS 200
#define SCROLL_SPEED 3

typedef struct {
    float x, y, z;
} Star;

float randf() {
    return (rand() % 1000) / 1000.0f;
}

int main() {
    srand(time(0));
    InitWindow(WIDTH, HEIGHT, "FTL-like avec Raylib");
    SetTargetFPS(60);

    // Fond étoilé
    Star stars[STARS] = {0};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = GetRandomValue(0, WIDTH);
        stars[i].y = GetRandomValue(0, HEIGHT);
        stars[i].z = randf();
    }

    // Création du vaisseau de base (centré comme dans FTL)
    Ship tempShip(0, 0);
    float shipWidth = 100;
    float shipHeight = 40;
    if (tempShip.isTextureLoaded()) {
        shipWidth = tempShip.getWidth();
        shipHeight = tempShip.getHeight();
    }
    float centerX = (WIDTH - shipWidth) / 2;
    float centerY = (HEIGHT - shipHeight) / 2;
    Ship ship(centerX, centerY);

    // Boucle principale du jeu
    while (!WindowShouldClose()) {
        // Scroll des étoiles
        for (int i = 0; i < STARS; i++) {
            stars[i].x -= SCROLL_SPEED * (stars[i].z / 1);
            if (stars[i].x <= 0) {
                stars[i].x += WIDTH;
                stars[i].y = GetRandomValue(0, HEIGHT);
            }
        }

        BeginDrawing();
        ClearBackground((Color){0, 0, 0, 255});
        // Dessin des étoiles
        for (int i = 0; i < STARS; i++) {
            DrawPixel(stars[i].x, stars[i].y, WHITE);
        }
        ship.Draw();
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
