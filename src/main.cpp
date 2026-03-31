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
#include "AudioManager.hpp"

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

#define DUST_PARTICLES 100
typedef struct {
    float x, y, speed;
} Dust;

float randf() {
    return (rand() % 1000) / 1000.0f;
}

int main() {
    srand(time(0));

    SetConfigFlags(FLAG_MSAA_4X_HINT); // Active l'anti-aliasing 4x

    InitWindow(WIDTH, HEIGHT, "FTL-like avec Raylib");
    InitAudioDevice();
    ToggleFullscreen(); // Optionnel selon tes préférences de test
    SetTargetFPS(60);

    // --- CHARGEMENT DU SHADER DE FLOU ---
    Shader blurShader = LoadShader(0, "resources/shaders/blur.fs");
    int locRenderSize = GetShaderLocation(blurShader, "renderSize");
    RenderTexture2D glowTarget = LoadRenderTexture(WIDTH, HEIGHT);

    Star stars[STARS] = {0};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = (float)GetRandomValue(0, GetScreenWidth());
        stars[i].y = (float)GetRandomValue(0, GetScreenHeight());
    
        stars[i].z = 0.1f + randf() * 0.9f; 

        unsigned char brightness = (unsigned char)(stars[i].z * 180);
        
        if (GetRandomValue(0, 10) > 8) {
            stars[i].color = (Color){ brightness, (unsigned char)(brightness * 0.9f), 255, 255 };
        } else {
            stars[i].color = (Color){ brightness, brightness, brightness, 255 };
        }
    }

    Dust dust[DUST_PARTICLES];
    for (int i = 0; i < DUST_PARTICLES; i++) {
        dust[i].x = (float)GetRandomValue(0, WIDTH);
        dust[i].y = (float)GetRandomValue(0, HEIGHT);
        dust[i].speed = 500.0f + randf() * 500.0f; // Très rapide
    }

    // --- INITIALISATION CAMÉRA ET SHAKE ---
    Camera2D camera = { 0 };
    camera.target = (Vector2){ 0, 0 };
    camera.offset = (Vector2){ 0, 0 };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    float ftlLerp = 0.0f;           // 0 à 1
    float ftlTransitionSpeed = 2.5f; 
    float flashAlpha = 0.0f;        // Pour le fondu du flash
    float rotationAngle = 0.0f;     // Pour l'inclinaison du vaisseau
    float shakeIntensity = 0.0f; // Puissance actuelle de la secousse
    float shakeDamping = 0.95f;  // Amortissement (0.95 = lent, 0.8 = rapide)

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

    AudioManager audio;
    audio.LoadMusic("ambience", "asset/Sound/ambience/normal.ogg");
    audio.LoadMusic("engine", "asset/Sound/sfx/engine/engine_idle.ogg");
    audio.LoadSfx("jump", "asset/Sound/sfx/ftl/ftl_boom.wav");

    audio.PlayMusic("ambience");
    audio.PlayMusic("engine");

    float t = 0.0f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        if (screenWidth < 10) screenWidth = WIDTH;
        if (screenHeight < 10) screenHeight = HEIGHT;

        float shipX = screenWidth / 2.0f;
        float shipY = screenHeight / 2.0f + sinf(t * 1.5f) * 15.0f;

        float rSize[2] = { (float)screenWidth, (float)screenHeight };
        SetShaderValue(blurShader, locRenderSize, rSize, SHADER_UNIFORM_VEC2);

        const Aseprite& sSprite = ship.GetSprite();
        float shipW = 128.0f;
        float shipH = 128.0f;

        float targetFtl = IsKeyDown(KEY_SPACE) ? 1.0f : 0.0f;
        ftlLerp += (targetFtl - ftlLerp) * (ftlTransitionSpeed * dt);
        
        bool ftlActive = IsKeyDown(KEY_SPACE);
        if (IsKeyPressed(KEY_SPACE)) {
            flashAlpha = 1.0f;
            shakeIntensity = 20.0f;
            audio.PlaySfx("jump");
        }
        if (flashAlpha > 0.0f) flashAlpha -= 1.5f * dt; // Fade-out du flash

        // Rotation fluide du vaisseau
        rotationAngle = cosf(t * 1.5f) * 10.0f;

        // --- C. DÉPLACEMENTS (Étoiles/Vaisseau) ---
        float baseSpeed = SCROLL_SPEED + (SCROLL_SPEED * 25.0f * ftlLerp);
        if (IsKeyPressed(KEY_SPACE)) shakeIntensity = 20.0f;

        float cruiseShake = ftlLerp * 1.5f; 
        if (shakeIntensity < cruiseShake) shakeIntensity = cruiseShake;
        else shakeIntensity *= shakeDamping;

        // Mise à jour des coordonnées de la caméra (le tremblement)
        if (shakeIntensity > 0.1f) {
            camera.offset.x = (float)GetRandomValue(-100, 100) / 100.0f * shakeIntensity;
            camera.offset.y = (float)GetRandomValue(-100, 100) / 100.0f * shakeIntensity;
        } else {
            camera.offset = { 0, 0 };
        }

        // 3. Mise à jour des étoiles avec la vitesse fraîchement calculée
        for (int i = 0; i < STARS; i++) {
            stars[i].x -= baseSpeed * stars[i].z * 100.0f * dt;
            if (stars[i].x <= 0) {
                stars[i].x += screenWidth;
                stars[i].y = (float)GetRandomValue(0, screenHeight);
            }
        }

        // --- MISE À JOUR AUDIO ---
        audio.Update(); // Nécessaire pour le streaming
        audio.SetMusicPitch("engine", 1.0f + (ftlLerp * 0.5f));
        audio.SetMusicVolume("engine", 0.2f + (ftlLerp * 0.4f));
        audio.SetMusicVolume("ambience", 0.4f - (ftlLerp * 0.2f));

        BeginTextureMode(glowTarget);
            ClearBackground(BLANK);
            for (int i = 0; i < STARS; i++) {
                if (ftlLerp > 0.1f) {
                    // La longueur de la traînée dépend de la transition
                    float streakLength = (15.0f * stars[i].z) * ftlLerp; 
                    DrawLineEx(
                        { stars[i].x, stars[i].y },
                        { stars[i].x + streakLength, stars[i].y },
                        1.0f + stars[i].z,
                        ColorAlpha(stars[i].color, 0.4f + (ftlLerp * 0.3f))
                    );
                } else {
                    float safeRadius = stars[i].z;
                    if (safeRadius < 0.8f) safeRadius = 0.8f; 
                    
                    DrawCircleV({stars[i].x, stars[i].y}, safeRadius, stars[i].color);
                }
            }

            // --- POUSSIÈRE SPATIALE (Fast Particles) ---
            for (int i = 0; i < DUST_PARTICLES; i++) {
                dust[i].x -= (ftlActive ? 2000.0f : 500.0f) * dt;
                
                if (dust[i].x <= 0) {
                    dust[i].x = WIDTH;
                    dust[i].y = (float)GetRandomValue(0, HEIGHT);
                }
                
                DrawPixelV({dust[i].x, dust[i].y}, (Color){ 50, 50, 50, 200 });
            }
        EndTextureMode();


        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode2D(camera);

                for (int i = 0; i < STARS; i++) {
                    DrawCircleV({stars[i].x, stars[i].y}, stars[i].z, stars[i].color);
                }

                BeginShaderMode(blurShader);
                    DrawTextureRec(glowTarget.texture, (Rectangle){ 0, 0, (float)glowTarget.texture.width, (float)-glowTarget.texture.height }, (Vector2){ 0, 0 }, WHITE);
                EndShaderMode();

                if (IsAsepriteValid(sSprite)) {
                    shipW = (float)GetAsepriteWidth(sSprite);
                    shipH = (float)GetAsepriteHeight(sSprite);
                }

                if (engineTex.id > 0) {
                    int engineFrameIdx = (int)(t * 12.0f) % 12;
                    float eFrameW = (float)engineTex.width / 12;
                    float eFrameH = (float)engineTex.height;
                    DrawTexturePro(
                        engineTex, { (engineFrameIdx * eFrameW), 0, eFrameW, eFrameH },
                        { shipX, shipY, eFrameW, eFrameH }, { eFrameW / 2.0f, eFrameH / 2.0f },
                        90.0f, WHITE
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

            EndMode2D();

            if (flashAlpha > 0.0f) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(WHITE, flashAlpha));
            }

            DrawText("ESPACE: Saut FTL", 20, 20, 20, RAYWHITE);
            if (ftlActive) DrawText("ENGAGED", 20, 50, 20, SKYBLUE);
        EndDrawing();
    }

    UnloadTexture(engineTex);
    UnloadTexture(shieldTex);

    UnloadShader(blurShader);
    UnloadRenderTexture(glowTarget);

    ship.Unload();

    CloseWindow();
    return 0;
}