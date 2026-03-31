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
#include "effect/SpaceBackground.hpp"
#include "map/galaxymap.hpp"
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

struct Particle {
    Vector2 pos;
    Vector2 vel;
    float life;
};
std::vector<Particle> particles;

struct Sector {
    float seed;
    Color c1;
    Color c2;
};

// Exemples basés sur tes images (Tarantula, Piliers, etc.)
std::vector<Sector> galaxyMap = {
    { 1.1f, { 200, 80, 40, 255 },  { 40, 40, 50, 255 } },   // Orange/Gris (Image 1)
    { 4.5f, { 180, 60, 20, 255 },  { 10, 20, 40, 255 } },   // Nébuleuse Rouge (Image 2)
    { 8.9f, { 40, 150, 180, 255 }, { 200, 100, 40, 255 } }, // Bleu/Or (Image 3)
    { 12.3f, { 120, 80, 150, 255 }, { 255, 200, 150, 255 } } // Violet/Lumineux (Image 4)
};
int currentSectorIdx = 0;

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

    GalaxyMap navMap;
    navMap.Generate(WIDTH, HEIGHT);

    // ── Shader ─────────────────────────────────────────────────
    SpaceBackground bg;
    bg.Load("src/ressources/shader/nebula.fs");

    Shader         blurShader  = LoadShader(0, "src/ressources/shader/blur.fs");
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

    bool globalImpact = false; // À passer à true quand on se fait toucher

    float jumpTimer = 0.0f;
    bool isJumping = false;
    int pendingSectorIdx = -1;
    const float JUMP_DURATION = 3.0f; // Durée du voyage en secondes

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        // DÉCLARATION CLAIRE DES POSITIONS (Utilisées partout après)
        float screenW = (float)GetScreenWidth();
        float screenH = (float)GetScreenHeight();
        float shipX = screenW / 2.0f;
        float shipY = screenH / 2.0f;

        Vector2 mousePos = GetMousePosition();

        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_M)) {
            navMap.Toggle();
            TraceLog(LOG_INFO, "Menu Map Toggle: %s", navMap.IsActive() ? "OPEN" : "CLOSED");
        }

        if (navMap.IsActive()) {
            navMap.Update(mousePos);
            
            if (navMap.IsJumpRequested()) {
                // On ne change pas le shader ici ! On prépare juste le voyage.
                pendingSectorIdx = navMap.GetTargetSector();
                isJumping = true;
                jumpTimer = JUMP_DURATION;
                
                // On déclenche le son et l'agitation au départ
                audio.PlaySfx("jump");
                shipStats.fuel -= 1.0f;
                
                navMap.ResetJumpRequest();
            }
        }

        float currentFtlTarget = 0.0f;
        float blackOverlayAlpha = 0.0f;

        if (isJumping) {
            jumpTimer -= dt;
            currentFtlTarget = 1.0f; // On veut l'effet tunnel

            // Calcul du noir (monte à 1.0 au milieu du saut)
            float progress = jumpTimer / JUMP_DURATION; 
            blackOverlayAlpha = sinf(progress * PI); 

            // Changement de secteur à la moitié
            if (jumpTimer <= JUMP_DURATION / 2.0f && pendingSectorIdx != -1) {
                Sector s = galaxyMap[pendingSectorIdx];
                bg.SetSector(s.seed, s.c1, s.c2);
                pendingSectorIdx = -1;
            }

            if (jumpTimer <= 0) {
                isJumping = false;
                flashAlpha = 1.0f;
                shakeIntensity = 15.0f;
            }
        } else {
            currentFtlTarget = IsKeyDown(KEY_SPACE) ? 1.0f : 0.0f;
        }

        bg.Update(dt, currentFtlTarget, globalImpact);
        ftlLerp = bg.GetFtlLerp();

        ftlLerp += (currentFtlTarget - ftlLerp) * 5.0f * dt;
        bg.Update(dt, currentFtlTarget, globalImpact);

        globalImpact = false; 

        for (auto it = particles.begin(); it != particles.end();) {
            it->pos.x += it->vel.x * dt - (ftlLerp * 1500 * dt);
            it->pos.y += it->vel.y * dt;
            it->life -= dt * 2.5f;
            if (it->life <= 0) it = particles.erase(it); else ++it;
        }

        const Aseprite& sSprite = ship.GetSprite();
        float shipW = IsAsepriteValid(sSprite) ? (float)GetAsepriteWidth(sSprite)  : 128.0f;
        float shipH = IsAsepriteValid(sSprite) ? (float)GetAsepriteHeight(sSprite) : 128.0f;

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

        audio.Update();
        audio.SetMusicPitch("engine",   1.0f + smoothFtl * 0.4f);
        audio.SetMusicVolume("engine",  0.2f + smoothFtl * 0.3f);
        audio.SetMusicVolume("ambience", 0.4f - ftlLerp * 0.2f);
    

        projMgr.Update(dt);

        shipStats.Update(dt);
        hud.Update(dt, shipStats);

        for (auto it = particles.begin(); it != particles.end();) {
            it->pos.x += it->vel.x * dt - (ftlLerp * 1000 * dt); // Elles filent avec le décor
            it->pos.y += it->vel.y * dt;
            it->life -= dt * 2.0f;
            if (it->life <= 0) it = particles.erase(it); else ++it;
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

        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);

            bg.Draw(WIDTH, HEIGHT);

            if (isJumping) {
                float progress = jumpTimer / JUMP_DURATION;
                float alpha = sinf(progress * PI); // Monte à 1.0 au milieu, redescend à 0
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, alpha));
            }

            if (!navMap.IsActive()) {

                BeginMode2D(camera);

                    for (int i = 0; i < STARS; i++) {
                        if (ftlLerp > 0.1f) {
                            float streakLen = (15.0f * stars[i].z) * ftlLerp;
                            DrawLineEx({ stars[i].x, stars[i].y }, { stars[i].x + streakLen, stars[i].y }, 1.0f + stars[i].z, ColorAlpha(stars[i].color, 0.4f + ftlLerp * 0.3f));
                        } else {
                            DrawCircleV({ stars[i].x, stars[i].y }, stars[i].z, stars[i].color);
                        }
                    }

                    for (const auto& p : particles) {
                        Color pCol = ColorAlpha(IsKeyDown(KEY_SPACE) ? SKYBLUE : ORANGE, p.life);
                        DrawCircleV(p.pos, p.life * 4.0f, pCol);
                    }

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

                if (isJumping) {
                    DrawText("SAUT FTL EN COURS...", screenW/2 - 100, screenH - 50, 20, SKYBLUE);
                }
            }  else {
                navMap.Draw(hud.GetFont());
            }
        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    UnloadTexture(engineTex);
    UnloadTexture(shieldTex);
    UnloadShader(blurShader);
    bg.Unload();
    UnloadRenderTexture(glowTarget);
    ship.Unload();
    hud.Unload();
    projMgr.Unload();
    CloseWindow();
    return 0;
}