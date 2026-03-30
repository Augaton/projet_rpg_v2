/*
   █████████                                  █████                       
  ███░░░░░███                                ░░███                        
 ░███    ░███  █████ ████  ███████  ██████   ███████    ██████  ████████  
 ░███████████ ░░███ ░███  ███░░███ ░░░░░███ ░░░███░    ███░░███░░███░░███ 
 ░███░░░░░███  ░███ ░███ ░███ ░███  ███████   ░███    ░███ ░███ ░███ ░███ 
 ░███    ░███  ░███ ░███ ░███ ░███ ███░░███   ░███ ███░███ ░███ ░███ ░███ 
 █████   █████ ░░████████░░███████░░████████  ░░█████ ░░██████  ████ █████
░░░░░   ░░░░░   ░░░░░░░░  ░░░░░███ ░░░░░░░░    ░░░░░   ░░░░░░  ░░░░ ░░░░░ 
                          ███ ░███                                        
                         ░░██████                                         
                          ░░░░░░                                          

Prototype game
*/                                      


#include "raylib.h"

#define RAYLIB_ASEPRITE_IMPLEMENTATION
#include "lib/raylib-aseprite.h"

#include "Ship.hpp"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <vector>

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
    ToggleFullscreen(); // Optionnel selon tes préférences de test
    SetTargetFPS(60);

    // Fond étoilé
    Star stars[STARS] = {0};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = (float)GetRandomValue(0, GetScreenWidth());
        stars[i].y = (float)GetRandomValue(0, GetScreenHeight());
        stars[i].z = randf();
    }

    // Chargement des assets
    Ship ship(0, 0, "asset/Base/Aseprite/Kla'ed - Battlecruiser - Base.aseprite");
    Aseprite engineSprite = LoadAseprite("asset/Engine/Aseprite/Kla'ed - Battlecruiser - Engine.aseprite");
    Texture2D shieldTex = LoadTexture("asset/Shield/Aseprite/Kla'ed - Battlecruiser - Shield.png");

    if (!IsAsepriteValid(ship.GetSprite())) {
        TraceLog(LOG_ERROR, "ERREUR : Fichier vaisseau introuvable ! Vérifie le chemin.");
    }
    if (!IsAsepriteValid(engineSprite)) {
        TraceLog(LOG_ERROR, "ERREUR : Fichier moteur introuvable !");
    }
    if (shieldTex.id == 0) {
        TraceLog(LOG_ERROR, "ERREUR : Fichier bouclier introuvable !");
    }

    float t = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Si pour une raison X ou Y la fenêtre n'est pas encore prête
        if (screenWidth < 10) screenWidth = WIDTH;
        if (screenHeight < 10) screenHeight = HEIGHT;

        float shipX = screenWidth / 2.0f;
        float shipY = screenHeight / 2.0f + sinf(t * 1.5f) * 15.0f;

        // On récupère le sprite du vaisseau proprement
        const Aseprite& sSprite = ship.GetSprite();
        float shipW = 128.0f; // Valeur par défaut (vue dans tes logs)
        float shipH = 128.0f;

        bool ftlActive = IsKeyDown(KEY_SPACE);
        float starSpeed = ftlActive ? SCROLL_SPEED * 15.0f : SCROLL_SPEED;

        // Scroll des étoiles
        for (int i = 0; i < STARS; i++) {
            stars[i].x -= starSpeed * (stars[i].z + 0.1f) * 100.0f * dt;
            if (stars[i].x <= 0) {
                stars[i].x += (float)screenWidth;
                stars[i].y = (float)GetRandomValue(0, screenHeight);
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);

        for (int i = 0; i < STARS; i++) {
            DrawPixel((int)stars[i].x, (int)stars[i].y, WHITE);
        }

        if (IsAsepriteValid(sSprite)) {
            shipW = (float)GetAsepriteWidth(sSprite);
            shipH = (float)GetAsepriteHeight(sSprite);
        }

        // --- MOTEUR ---
        if (IsAsepriteValid(engineSprite)) {
            int frameIdx = (int)(t * 12.0f) % 12; // 12 frames
            float eW = (float)GetAsepriteWidth(engineSprite);
            float eH = (float)GetAsepriteHeight(engineSprite);
            // On décale le moteur légèrement vers l'arrière
            Vector2 ePos = { shipX, shipY }; 

            if (ftlActive) {
                for (int b = 3; b >= 1; b--) {
                    float bloomScale = 1.0f + b * 0.2f;
                    Color bloomColor = ColorAlpha(SKYBLUE, 0.15f * b);
                    DrawAsepritePro(engineSprite, frameIdx, {ePos.x, ePos.y, eW * bloomScale, eH * bloomScale}, { (eW*bloomScale)/2, (eH*bloomScale)/2 }, 90.0f, bloomColor);
                }
            }
            DrawAsepritePro(engineSprite, frameIdx, {ePos.x, ePos.y, eW, eH}, {eW/2, eH/2}, 90.0f, WHITE);
        }

        // --- SHIELD ---
        if (shieldTex.id > 0) {
            float shieldW = shieldTex.width / 16.0f;
            float shieldH = shieldTex.height;

            DrawTexturePro(
                shieldTex,
                (Rectangle){0, 0, shieldW, shieldH},
                (Rectangle){shipX, shipY, shieldW, shieldH},
                (Vector2){shieldW/2.0f, shieldH/2.0f},
                90.0f,
                (Color){100,200,255,120}
            );
        }

        // --- VAISSEAU ---
        if (IsAsepriteValid(sSprite)) {
            DrawAsepritePro(sSprite, 0, { shipX, shipY, shipW, shipH }, { shipW/2, shipH/2 }, 90.0f, WHITE);
        }

        DrawText("ESPACE: Saut FTL", 20, 20, 20, RAYWHITE);
        EndDrawing();
    }

    UnloadAseprite(engineSprite);
    UnloadTexture(shieldTex);

    ship.Unload();

    CloseWindow();
    return 0;
}