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
*/

#include "raylib.h"

#define RAYLIB_ASEPRITE_IMPLEMENTATION
#include "lib/raylib-aseprite.h"

#include "audio/AudioManager.hpp"
#include "ship/Ship.hpp"
#include "projectile/ProjectileManager.hpp"
#include "ship/ShipStats.hpp"
#include "hud/HUD.hpp"

#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <vector>
#include <algorithm>

// ─── Constantes ───────────────────────────────────────────────────────────────

#define WIDTH        800
#define HEIGHT       600
#define STARS        200
#define SCROLL_SPEED 2

// ─── Structs ──────────────────────────────────────────────────────────────────

struct Star {
    float x, y, z;
    Color color;
};

struct Dust {
    float x, y, speed;
};

#define DUST_PARTICLES 100

// ─── Helpers ──────────────────────────────────────────────────────────────────

static float randf() {
    return (rand() % 1000) / 1000.0f;
}

float noise(float x, float y, float t) {
    return (
        sinf(x * 0.02f + t * 0.2f) +
        cosf(y * 0.02f - t * 0.15f) +
        sinf((x + y) * 0.01f + t * 0.1f)
    ) * 0.5f;
}

int main() {
    srand(time(0));

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "Augaton");
    ToggleFullscreen();
    SetTargetFPS(60);

    // ── Shader ─────────────────────────────────────────────────
    Shader         blurShader  = LoadShader(0, "resources/shaders/blur.fs");
    int            locRenderSize = GetShaderLocation(blurShader, "renderSize");
    RenderTexture2D glowTarget  = LoadRenderTexture(WIDTH, HEIGHT);

    // ── Étoiles / Poussière ────────────────────────────────────────────────────
    Star stars[STARS] = {};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = (float)GetRandomValue(0, GetScreenWidth());
        stars[i].y = (float)GetRandomValue(0, GetScreenHeight());
        stars[i].z = 0.1f + randf() * 0.9f;

        unsigned char b = (unsigned char)(stars[i].z * 180);
        stars[i].color  = (GetRandomValue(0, 10) > 8)
                         ? Color{ b, (unsigned char)(b * 0.9f), 255, 255 }
                         : Color{ b, b, b, 255 };
    }

    Dust dust[DUST_PARTICLES];
    for (int i = 0; i < DUST_PARTICLES; i++) {
        dust[i].x     = (float)GetRandomValue(0, WIDTH);
        dust[i].y     = (float)GetRandomValue(0, HEIGHT);
        dust[i].speed = 500.0f + randf() * 500.0f;
    }

    // ── Caméra & shake ────────────────────────────────────────────────────────
    Camera2D camera       = {};
    camera.target         = { 0, 0 };
    camera.offset         = { 0, 0 };
    camera.rotation       = 0.0f;
    camera.zoom           = 1.0f;

    float ftlLerp           = 0.0f;
    float flashAlpha        = 0.0f;
    float shakeIntensity    = 0.0f;
    const float shakeDamping = 0.95f;
    float smoothFtl         = 0.0f;
    float shipRotation      = 90.0f;
    float t                 = 0.0f;

    // ── Assets ───────────────────────────────────────────────────────
    Ship      ship(0, 0, "asset/Base/Aseprite/frigate.aseprite");
    Texture2D engineTex = LoadTexture("asset/Engine/PNGs/frigate.png");
    Texture2D shieldTex = LoadTexture("asset/Shield/PNGs/frigate.png");

    if (!IsAsepriteValid(ship.GetSprite()))
        TraceLog(LOG_ERROR, "ERREUR : Fichier vaisseau introuvable !");
    if (engineTex.id == 0)
        TraceLog(LOG_ERROR, "ERREUR : Fichier moteur introuvable !");
    if (shieldTex.id == 0)
        TraceLog(LOG_ERROR, "ERREUR : Fichier bouclier introuvable !");

    AudioManager audio;
    audio.LoadMusic("ambience", "asset/Sound/ambience/normal.ogg");
    audio.LoadMusic("engine",   "asset/Sound/sfx/engine/engine_idle.ogg");
    audio.LoadSfx("jump",       "asset/Sound/sfx/ftl/ftl_boom.wav");
    audio.LoadSfx("laser",      "asset/Sound/sfx/weapons/laser_fire.wav"); 
    audio.LoadSfx("missile",    "asset/Sound/sfx/weapons/missile_fire.wav");
    audio.PlayMusic("ambience");
    audio.PlayMusic("engine");

    ShipStats shipStats;
    HUD       hud;
    hud.LoadFont("resources/fonts/Rajdhani-Medium.ttf", 20);

    // ── Projectiles ───────────────────────────────────────────────────────────
    ProjectileManager projMgr;
    projMgr.Load();

    projMgr.onImpact = [&](Vector2, ProjType type) {
        if (type == ProjType::TORPEDO) {
            shakeIntensity = std::max(shakeIntensity, 8.0f);
            hud.PushNotification("TORPEDO HIT", { 255, 140, 30, 255 });
        } else {
            shakeIntensity = std::max(shakeIntensity, 3.0f);
            hud.PushNotification("HIT", { 80, 220, 255, 255 });
        }
    };

    float laserCooldown   = 0.0f;
    float missileCooldown = 0.0f;
    const float LASER_CD   = 0.18f;
    const float MISSILE_CD = 0.9f;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();
        if (screenW < 10) screenW = WIDTH;
        if (screenH < 10) screenH = HEIGHT;

        float shipX = screenW / 2.0f;
        float shipY = screenH / 2.0f;
        Vector2 shipPos = { shipX, shipY };

        float rSize[2] = { (float)screenW, (float)screenH };
        SetShaderValue(blurShader, locRenderSize, rSize, SHADER_UNIFORM_VEC2);

        const Aseprite& sSprite = ship.GetSprite();
        float shipW = IsAsepriteValid(sSprite) ? (float)GetAsepriteWidth(sSprite)  : 128.0f;
        float shipH = IsAsepriteValid(sSprite) ? (float)GetAsepriteHeight(sSprite) : 128.0f;

        bool  ftlActive  = IsKeyDown(KEY_SPACE);
        float targetFtl  = ftlActive ? 1.0f : 0.0f;
        ftlLerp += (targetFtl - ftlLerp) * (2.5f * dt);

        if (IsKeyPressed(KEY_SPACE)) {
            flashAlpha     = 1.0f;
            shakeIntensity = 20.0f;
            audio.PlaySfx("jump");
            shipStats.fuel = std::max(0.0f, shipStats.fuel - 1.0f);
            hud.PushNotification("-1 FUEL (saut FTL)", { 220, 160, 50, 255 });
        }

        flashAlpha = std::max(0.0f, flashAlpha - 1.5f * dt);

        float baseSpeed = SCROLL_SPEED + (SCROLL_SPEED * 25.0f * ftlLerp);

        for (int i = 0; i < STARS; i++) {
            stars[i].x -= baseSpeed * stars[i].z * 100.0f * dt;
            if (stars[i].x <= 0) {
                stars[i].x += screenW;
                stars[i].y  = (float)GetRandomValue(0, screenH);
            }
        }

        float cruiseShake = ftlLerp * 1.5f;
        shakeIntensity = std::max(shakeIntensity * shakeDamping, cruiseShake);

        if (shakeIntensity > 0.1f) {
            camera.offset.x = (float)GetRandomValue(-100, 100) / 100.0f * shakeIntensity;
            camera.offset.y = (float)GetRandomValue(-100, 100) / 100.0f * shakeIntensity;
        } else {
            camera.offset = { 0, 0 };
        }

        smoothFtl += (ftlLerp - smoothFtl) * (5.0f * dt);

        laserCooldown   -= dt;
        missileCooldown -= dt;

        float fireAngle = 0.0f;

        if (IsKeyDown(KEY_F) && laserCooldown <= 0 && !ftlActive) {
            projMgr.Spawn(ProjType::LASER,     shipPos, fireAngle);
            laserCooldown = LASER_CD;
            audio.PlaySfx("laser", 1.0f, 0.3f);
        }
        if (IsKeyPressed(KEY_G) && missileCooldown <= 0 && !ftlActive) {
            projMgr.Spawn(ProjType::TORPEDO, shipPos, fireAngle);
            missileCooldown = MISSILE_CD;
            audio.PlaySfx("missile", 1.0f, 0.3f);
        }


        audio.Update();
        audio.SetMusicPitch("engine",   1.0f + smoothFtl * 0.4f);
        audio.SetMusicVolume("engine",  0.2f + smoothFtl * 0.3f);
        audio.SetMusicVolume("ambience", 0.4f - ftlLerp * 0.2f);
    

        projMgr.Update(dt);

        shipStats.Update(dt);
        hud.Update(dt, shipStats);

        // Touches de test (à remplacer par vraie logique de combat)
        if (IsKeyDown(KEY_F) && laserCooldown <= 0 && !ftlActive) {
            projMgr.Spawn(ProjType::LASER, shipPos, fireAngle);
            laserCooldown = LASER_CD;
        }
        if (IsKeyPressed(KEY_G) && missileCooldown <= 0 && !ftlActive) {
            projMgr.Spawn(ProjType::TORPEDO, shipPos, fireAngle);
            missileCooldown = MISSILE_CD;
        }
        if (IsKeyPressed(KEY_E)) {
            projMgr.Spawn(ProjType::BIG_BULLET, shipPos, fireAngle);
        }


        BeginTextureMode(glowTarget);
            ClearBackground(BLANK);

            for (int i = 0; i < STARS; i++) {
                if (ftlLerp > 0.1f) {
                    float streakLen = (15.0f * stars[i].z) * ftlLerp;
                    DrawLineEx(
                        { stars[i].x, stars[i].y },
                        { stars[i].x + streakLen, stars[i].y },
                        1.0f + stars[i].z,
                        ColorAlpha(stars[i].color, 0.4f + ftlLerp * 0.3f)
                    );
                } else {
                    float r = std::max(stars[i].z, 0.8f);
                    DrawCircleV({ stars[i].x, stars[i].y }, r, stars[i].color);
                }
            }

            projMgr.DrawGlow(t);

            for (int i = 0; i < DUST_PARTICLES; i++) {
                dust[i].x -= (ftlActive ? 2000.0f : 500.0f) * dt;
                if (dust[i].x <= 0) {
                    dust[i].x = (float)WIDTH;
                    dust[i].y = (float)GetRandomValue(0, HEIGHT);
                }
                DrawPixelV({ dust[i].x, dust[i].y }, { 50, 50, 50, 200 });
            }
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);

        for (int y = 0; y < screenH; y += 2) {
            for (int x = 0; x < screenW; x += 2) {

                float nx = x + t * 40.0f;
                float ny = y + sinf(t * 0.3f) * 30.0f;

                float n = noise(nx, ny, t);
                n = powf((n + 1.0f) * 0.5f, 2.0f);

                float gradient = (float)x / screenW;

                float r = (0.6f + 0.4f * sinf(n * 3 + t)) * gradient;
                float g = (0.5f + 0.5f * sinf(n * 2 + t * 0.7f)) * (1.0f - gradient);
                float b = (0.7f + 0.3f * sinf(n * 4 - t * 0.5f));

                DrawRectangle(x, y, 2, 2, {
                    (unsigned char)(r * 255),
                    (unsigned char)(g * 255),
                    (unsigned char)(b * 255),
                    35
                });
            }
        }

            BeginMode2D(camera);

                for (int i = 0; i < STARS; i++)
                    DrawCircleV({ stars[i].x, stars[i].y }, stars[i].z, stars[i].color);

                BeginShaderMode(blurShader);
                    DrawTextureRec(
                        glowTarget.texture,
                        { 0, 0, (float)glowTarget.texture.width, -(float)glowTarget.texture.height },
                        { 0, 0 },
                        WHITE
                    );
                EndShaderMode();

                // Moteur
                if (engineTex.id > 0) {
                    int   idx    = (int)(t * 12.0f) % 12;
                    float eFrameW = (float)engineTex.width / 12.0f;
                    float eFrameH = (float)engineTex.height;
                    DrawTexturePro(
                        engineTex,
                        { idx * eFrameW, 0, eFrameW, eFrameH },
                        { shipX, shipY, eFrameW, eFrameH },
                        { eFrameW / 2.0f, eFrameH / 2.0f },
                        shipRotation, WHITE
                    );
                }

                // Bouclier
                if (shieldTex.id > 0) {
                    const int   SHIELD_FRAMES = 40;
                    const float SHIELD_FPS    = 12.0f;
                    int   frame      = (int)(t * SHIELD_FPS) % SHIELD_FRAMES;
                    float frameW     = (float)shieldTex.width  / SHIELD_FRAMES;
                    float frameH     = (float)shieldTex.height;

                    unsigned char shieldA = (unsigned char)(80 + shipStats.ShieldPct() * 100);
                    Color shieldColor = { 100, 200, 255, shieldA };

                    DrawTexturePro(
                        shieldTex,
                        { frame * frameW, 0, frameW, frameH },
                        { shipX, shipY, frameW, frameH },
                        { frameW / 2.0f, frameH / 2.0f },
                        shipRotation, shieldColor
                    );
                }

                // Vaisseau
                if (IsAsepriteValid(sSprite))
                    DrawAsepritePro(sSprite, 0,
                                    { shipX, shipY, shipW, shipH },
                                    { shipW / 2.0f, shipH / 2.0f },
                                    shipRotation, WHITE);

                // Projectiles (dans le world space)
                projMgr.Draw();

            EndMode2D();


            if (flashAlpha > 0.0f)
                DrawRectangle(0, 0, screenW, screenH,
                              ColorAlpha(WHITE, flashAlpha));


            hud.Draw(shipStats, screenW, screenH);

            // ── Contrôles (debug) ─────────────────────────────────────────────
            DrawTextEx(hud.GetFont(), "ESPACE: Saut FTL",   { 20, 20 }, 16, 1, { 160, 170, 190, 180 });
            DrawTextEx(hud.GetFont(), "F: Laser  G: Missile", { 20, 42 }, 16, 1, { 160, 170, 190, 180 });
            DrawTextEx(hud.GetFont(), "H/J/K: Dégâts (test)", { 20, 64 }, 16, 1, { 100, 110, 130, 140 });
            if (ftlActive)
                DrawTextEx(hud.GetFont(), "FTL ENGAGED", { 20, 90 }, 18, 1, SKYBLUE);

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    UnloadTexture(engineTex);
    UnloadTexture(shieldTex);
    UnloadShader(blurShader);
    UnloadRenderTexture(glowTarget);
    ship.Unload();
    hud.Unload();
    projMgr.Unload();
    CloseWindow();
    return 0;
}