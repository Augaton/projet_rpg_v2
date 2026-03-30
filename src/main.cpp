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
    Color color; // On stocke la couleur pour varier la luminosité
} Star;

float randf() {
    return (rand() % 1000) / 1000.0f;
}

int main() {
    srand(time(0));

    InitWindow(WIDTH, HEIGHT, "FTL-like avec Raylib");
    ToggleFullscreen(); // Optionnel selon tes préférences de test
    SetTargetFPS(60);

    Star stars[STARS] = {0};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = (float)GetRandomValue(0, GetScreenWidth());
        stars[i].y = (float)GetRandomValue(0, GetScreenHeight());
        
        // z : profondeur (0.1 = très loin, 1.0 = proche)
        stars[i].z = 0.1f + randf() * 0.9f; 

        // Luminosité réduite (max 180 au lieu de 255) pour plus de discrétion
        unsigned char brightness = (unsigned char)(stars[i].z * 180);
        
        // Légère teinte bleutée aléatoire pour le réalisme
        if (GetRandomValue(0, 10) > 8) {
            stars[i].color = (Color){ brightness, (unsigned char)(brightness * 0.9f), 255, 255 };
        } else {
            stars[i].color = (Color){ brightness, brightness, brightness, 255 };
        }
    }

    // Chargement des assets
    Ship ship(0, 0, "asset/Base/Aseprite/frigate.aseprite");
    Texture2D engineTex = LoadTexture("asset/Engine/PNGs/frigate.png");
    Texture2D shieldTex = LoadTexture("asset/Shield/PNGs/frigate.png");

    if (!IsAsepriteValid(ship.GetSprite())) {
        TraceLog(LOG_ERROR, "ERREUR : Fichier vaisseau introuvable ! Vérifie le chemin.");
    }
    if (engineTex.id == 0) {
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
        float baseSpeed = ftlActive ? SCROLL_SPEED * 20.0f : SCROLL_SPEED;

        for (int i = 0; i < STARS; i++) {
            // La vitesse est multipliée par z : les étoiles devant (z=1) vont plus vite
            stars[i].x -= baseSpeed * stars[i].z * 100.0f * dt;

            // Si ftlActive, on peut ajouter un effet d'étirement (streak)
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

        // --- MOTEUR (Système Texture2D) ---
        if (engineTex.id > 0) {
            int totalEngineFrames = 12;
            float engineAnimSpeed = 12.0f; 
            int engineFrameIdx = (int)(t * engineAnimSpeed) % totalEngineFrames;

            float eFrameW = (float)engineTex.width / totalEngineFrames;
            float eFrameH = (float)engineTex.height;

            Rectangle eSourceRec = { engineFrameIdx * eFrameW, 0, eFrameW, eFrameH };
            Vector2 ePos = { shipX, shipY };
            Vector2 eOrigin = { eFrameW / 2.0f, eFrameH / 2.0f };

            // Effet FTL (Bloom)
            for (int i = 0; i < STARS; i++) {
                if (ftlActive) {
                    // Traînée FTL plus fine et plus sombre
                    float streakLength = 8.0f * stars[i].z; 
                    DrawLineEx(
                        { stars[i].x, stars[i].y },
                        { stars[i].x + streakLength, stars[i].y },
                        0.5f + (stars[i].z * 1.0f), // Plus fin
                        ColorAlpha(stars[i].color, 0.5f + stars[i].z * 0.5f) // Plus transparent
                    );
                } else {
                    // Étoile normale : on dessine un pixel ou un tout petit point
                    if (stars[i].z < 0.5f) {
                        // Très loin : un simple pixel
                        DrawPixelV({ stars[i].x, stars[i].y }, stars[i].color);
                    } else {
                        // Plus proche : un petit point de taille 1
                        DrawCircleV({ stars[i].x, stars[i].y }, 1.0f, stars[i].color);
                    }
                }
            }

            // Moteur principal
            DrawTexturePro(
                engineTex,
                eSourceRec,
                { ePos.x, ePos.y, eFrameW, eFrameH },
                eOrigin,
                90.0f,
                WHITE
            );
        }

        // --- SHIELD (Système Texture2D animé) ---
        if (shieldTex.id > 0) {
            int totalFrames = 40;
            float animSpeed = 12.0f; // Vitesse : 12 images par seconde
            

            int currentFrame = (int)(t * animSpeed) % totalFrames;
            float frameWidth = (float)shieldTex.width / totalFrames;
            float frameHeight = (float)shieldTex.height;

            Rectangle sourceRec = { 
                currentFrame * frameWidth, 
                0, 
                frameWidth, 
                frameHeight 
            };

            Rectangle destRec = { shipX, shipY, frameWidth, frameHeight };
            Vector2 origin = { frameWidth / 2.0f, frameHeight / 2.0f };
            Color shieldColor = { 100, 200, 255, (unsigned char)(180 + sinf(t * 4.0f) * 20) };

            DrawTexturePro(
                shieldTex,
                sourceRec,
                destRec,
                origin,
                90.0f,
                shieldColor
            );
        }

        // --- VAISSEAU ---
        if (IsAsepriteValid(sSprite)) {
            DrawAsepritePro(sSprite, 0, { shipX, shipY, shipW, shipH }, { shipW/2, shipH/2 }, 90.0f, WHITE);
        }

        DrawText("ESPACE: Saut FTL", 20, 20, 20, RAYWHITE);
        EndDrawing();
    }

    UnloadTexture(engineTex);
    UnloadTexture(shieldTex);

    ship.Unload();

    CloseWindow();
    return 0;
}