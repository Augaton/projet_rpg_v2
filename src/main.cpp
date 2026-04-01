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

struct Particle {
    Vector2 pos;
    Vector2 vel;
    float   life;
    Color   color;
};

// Définition d'un secteur — seed + palette pour SpaceBackground
struct Sector {
    float seed;
    Color c1;
    Color c2;
    const char* name;
};

// ─── Données secteurs ─────────────────────────────────────────────────────────

static const Sector SECTORS[] = {
    { 1.1f,  { 200,  80,  40, 255 }, {  40,  40,  50, 255 }, "TARANTULA"  },
    { 4.5f,  { 180,  60,  20, 255 }, {  10,  20,  40, 255 }, "RED GIANT"  },
    { 8.9f,  {  40, 150, 180, 255 }, { 200, 100,  40, 255 }, "AZURE RIFT" },
    { 12.3f, { 120,  80, 150, 255 }, { 255, 200, 150, 255 }, "VOID CROWN" },
};
static constexpr int SECTOR_COUNT = 4;

// ─── IA Ennemie ───────────────────────────────────────────────────────────────

enum class EnemyState { APPROACH, ATTACK, FLEE };

struct Enemy {
    Vector2    pos;
    Vector2    vel;
    float      rotation;
    float      hull;
    float      fireTimer;
    EnemyState state;
    bool       active;

    // Sprite (même pipeline que le joueur)
    Aseprite   sprite;
    Texture2D  engineTex;
};

static float randf() { return (rand() % 1000) / 1000.0f; }

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void SpawnEnemy(Enemy& e, float screenW, float screenH) {
    // Spawn hors écran à droite
    e.pos       = { screenW + 80.0f,
                    screenH * 0.3f + randf() * screenH * 0.4f };
    e.vel       = { 0, 0 };
    e.rotation  = 270.0f;   // pointe vers la gauche
    e.hull      = 100.0f;
    e.fireTimer = 2.0f;
    e.state     = EnemyState::APPROACH;
    e.active    = true;
}

static void UpdateEnemy(Enemy& e, Vector2 playerPos,
                         ProjectileManager& projMgr,
                         float dt, float ftlLerp,
                         float& shakeIntensity,
                         ShipStats& shipStats, HUD& hud) {
    if (!e.active) return;

    Vector2 toPlayer = { playerPos.x - e.pos.x, playerPos.y - e.pos.y };
    float   dist     = sqrtf(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y);
    Vector2 dirNorm  = (dist > 0.1f)
                       ? Vector2{ toPlayer.x / dist, toPlayer.y / dist }
                       : Vector2{ -1, 0 };

    // ── Machine à états ───────────────────────────────────────────────────────
    if (e.hull < 30.0f)                        e.state = EnemyState::FLEE;
    else if (dist < 280.0f)                    e.state = EnemyState::ATTACK;
    else if (e.state != EnemyState::FLEE)      e.state = EnemyState::APPROACH;

    const float APPROACH_SPD = 90.0f;
    const float ATTACK_SPD   = 30.0f;
    const float FLEE_SPD     = 150.0f;

    switch (e.state) {
        case EnemyState::APPROACH:
            e.vel.x += (dirNorm.x * APPROACH_SPD - e.vel.x) * dt * 3.0f;
            e.vel.y += (dirNorm.y * APPROACH_SPD - e.vel.y) * dt * 3.0f;
            break;
        case EnemyState::ATTACK:
            // Orbite autour du joueur
            e.vel.x += (dirNorm.x * ATTACK_SPD + dirNorm.y * 60.0f - e.vel.x) * dt * 2.5f;
            e.vel.y += (dirNorm.y * ATTACK_SPD - dirNorm.x * 60.0f - e.vel.y) * dt * 2.5f;
            break;
        case EnemyState::FLEE:
            e.vel.x += (-dirNorm.x * FLEE_SPD - e.vel.x) * dt * 4.0f;
            e.vel.y += (-dirNorm.y * FLEE_SPD - e.vel.y) * dt * 4.0f;
            break;
    }

    // Décalage FTL — l'ennemi est emporté vers la gauche avec le décor
    e.vel.x -= ftlLerp * 300.0f * dt;

    e.pos.x += e.vel.x * dt;
    e.pos.y += e.vel.y * dt;

    // Rotation smooth vers la direction de déplacement
    if (dist > 0.1f) {
        float targetRot = atan2f(toPlayer.y, toPlayer.x) * RAD2DEG + 90.0f;
        float delta = targetRot - e.rotation;
        while (delta >  180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;
        e.rotation += delta * dt * 4.0f;
    }

    // ── Tir ennemi ────────────────────────────────────────────────────────────
    e.fireTimer -= dt;
    if (e.fireTimer <= 0.0f && e.state == EnemyState::ATTACK) {
        float fireAngle = atan2f(toPlayer.y, toPlayer.x) * RAD2DEG;
        projMgr.SpawnEnemy(e.pos, fireAngle);   // voir note ProjectileManager
        e.fireTimer = 1.8f + randf() * 1.2f;
    }

    // ── Désactivation hors écran ──────────────────────────────────────────────
    if (e.pos.x < -200.0f || e.pos.x > 2000.0f)
        e.active = false;
}

static void DrawEnemy(const Enemy& e, float t) {
    if (!e.active) return;

    // Moteur ennemi
    if (e.engineTex.id > 0) {
        int   idx     = (int)(t * 12.0f) % 12;
        float eFrameW = (float)e.engineTex.width / 12.0f;
        float eFrameH = (float)e.engineTex.height;
        DrawTexturePro(
            e.engineTex,
            { idx * eFrameW, 0, eFrameW, eFrameH },
            { e.pos.x, e.pos.y, eFrameW, eFrameH },
            { eFrameW / 2.0f, eFrameH / 2.0f },
            e.rotation, WHITE
        );
    }

    // Corps ennemi
    if (IsAsepriteValid(e.sprite)) {
        float sw = (float)GetAsepriteWidth(e.sprite);
        float sh = (float)GetAsepriteHeight(e.sprite);
        DrawAsepritePro(e.sprite, 0,
                        { e.pos.x, e.pos.y, sw, sh },
                        { sw / 2.0f, sh / 2.0f },
                        e.rotation, WHITE);
    } else {
        // Fallback géométrique si le sprite manque
        DrawPoly(e.pos, 3, 22.0f, e.rotation, { 220, 80, 60, 220 });
        DrawPolyLines(e.pos, 3, 22.0f, e.rotation, { 255, 120, 80, 255 });
    }

    // Barre de vie flottante
    float barW = 40.0f;
    float pct  = e.hull / 100.0f;
    Color barC = (pct > 0.5f) ? Color{ 80, 210, 100, 200 }
               : (pct > 0.25f) ? Color{ 220, 180, 40, 200 }
                                : Color{ 220, 60, 60, 200 };
    DrawRectangle((int)(e.pos.x - barW/2), (int)(e.pos.y - 36),
                  (int)barW, 4, { 20, 20, 30, 180 });
    DrawRectangle((int)(e.pos.x - barW/2), (int)(e.pos.y - 36),
                  (int)(barW * pct), 4, barC);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    srand(time(0));

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "Augaton");
    ToggleFullscreen();
    SetTargetFPS(60);

    GalaxyMap navMap;
    navMap.Generate(WIDTH, HEIGHT);

    // ── Shaders ───────────────────────────────────────────────────────────────
    SpaceBackground bg;
    bg.Load("src/ressources/shader/nebula.fs");
    bg.SetSector(SECTORS[0].seed, SECTORS[0].c1, SECTORS[0].c2);

    Shader          blurShader    = LoadShader(0, "src/ressources/shader/blur.fs");
    int             locBlurSize   = GetShaderLocation(blurShader, "renderSize");
    RenderTexture2D glowTarget    = LoadRenderTexture(WIDTH, HEIGHT);

    // ── Étoiles ───────────────────────────────────────────────────────────────
    Star stars[STARS] = {};
    for (int i = 0; i < STARS; i++) {
        stars[i].x = (float)GetRandomValue(0, GetScreenWidth());
        stars[i].y = (float)GetRandomValue(0, GetScreenHeight());
        stars[i].z = 0.1f + randf() * 0.9f;
        unsigned char b = (unsigned char)(stars[i].z * 180);
        stars[i].color = (GetRandomValue(0, 10) > 8)
                        ? Color{ b, (unsigned char)(b * 0.9f), 255, 255 }
                        : Color{ b, b, b, 255 };
    }

    // ── Caméra & shake ────────────────────────────────────────────────────────
    Camera2D camera      = {};
    camera.zoom          = 1.0f;
    float ftlLerp        = 0.0f;
    float flashAlpha     = 0.0f;
    float shakeIntensity = 0.0f;
    const float shakeDamping = 0.95f;
    float smoothFtl      = 0.0f;
    float shipRotation   = 90.0f;
    float t              = 0.0f;

    // ── Assets joueur ─────────────────────────────────────────────────────────
    Ship      ship(0, 0, "asset/Base/Aseprite/frigate.aseprite");
    Texture2D engineTex = LoadTexture("asset/Engine/PNGs/frigate.png");
    Texture2D shieldTex = LoadTexture("asset/Shield/PNGs/frigate.png");

    if (!IsAsepriteValid(ship.GetSprite()))
        TraceLog(LOG_ERROR, "Vaisseau introuvable !");
    if (engineTex.id == 0)
        TraceLog(LOG_ERROR, "Moteur introuvable !");
    if (shieldTex.id == 0)
        TraceLog(LOG_ERROR, "Bouclier introuvable !");

    // ── Audio ─────────────────────────────────────────────────────────────────
    AudioManager audio;
    audio.LoadMusic("ambience", "asset/Sound/ambience/normal.ogg");
    audio.LoadMusic("engine",   "asset/Sound/sfx/engine/engine_idle.ogg");
    audio.LoadSfx("jump",       "asset/Sound/sfx/ftl/ftl_boom.wav");
    audio.LoadSfx("laser",      "asset/Sound/sfx/weapons/laser_fire.wav");
    audio.LoadSfx("missile",    "asset/Sound/sfx/weapons/missile_fire.wav");
    audio.PlayMusic("ambience");
    audio.PlayMusic("engine");

    // ── HUD & stats ───────────────────────────────────────────────────────────
    ShipStats shipStats;
    HUD       hud;
    hud.LoadFont("resources/fonts/Rajdhani-Medium.ttf", 20);

    // ── Projectiles ───────────────────────────────────────────────────────────
    ProjectileManager projMgr;
    projMgr.Load();

    float laserCooldown   = 0.0f;
    float missileCooldown = 0.0f;
    const float LASER_CD   = 0.18f;
    const float MISSILE_CD = 0.9f;

    projMgr.onImpact = [&](Vector2, ProjType type) {
        if (type == ProjType::TORPEDO) {
            shakeIntensity = std::max(shakeIntensity, 8.0f);
            hud.PushNotification("TORPEDO HIT", { 255, 140, 30, 255 });
        } else {
            shakeIntensity = std::max(shakeIntensity, 3.0f);
            hud.PushNotification("HIT", { 80, 220, 255, 255 });
        }
    };

    // ── Ennemi ────────────────────────────────────────────────────────────────
    Enemy enemy = {};
    enemy.sprite    = LoadAseprite("asset/Base/Aseprite/enemy_frigate.aseprite");
    enemy.engineTex = LoadTexture("asset/Engine/PNGs/frigate.png"); // même sprite pour l'instant
    enemy.active    = false;

    float enemySpawnTimer = 8.0f;  // Premier spawn dans 8s

    // ── Particules ────────────────────────────────────────────────────────────
    std::vector<Particle> particles;
    particles.reserve(128);

    // ── Saut FTL ──────────────────────────────────────────────────────────────
    float jumpTimer        = 0.0f;
    bool  isJumping        = false;
    int   pendingSectorIdx = -1;
    int   currentSectorIdx = 0;
    const float JUMP_DURATION = 3.0f;

    // ─────────────────────────────────────────────────────────────────────────
    // Boucle principale
    // ─────────────────────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        const float screenW = (float)GetScreenWidth();
        const float screenH = (float)GetScreenHeight();
        const float shipX   = screenW / 2.0f;
        const float shipY   = screenH / 2.0f;
        const Vector2 shipPos = { shipX, shipY };

        Vector2 mousePos = GetMousePosition();

        // ── Map de navigation ─────────────────────────────────────────────────
        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_M))
            navMap.Toggle();

        if (navMap.IsActive()) {
            navMap.Update(mousePos);
            if (navMap.IsJumpRequested()) {
                pendingSectorIdx = navMap.GetTargetSector();
                isJumping   = true;
                jumpTimer   = JUMP_DURATION;
                audio.PlaySfx("jump");
                shipStats.fuel = std::max(0.0f, shipStats.fuel - 1.0f);
                hud.PushNotification("FTL JUMP INITIATED", { 80, 180, 255, 255 });
                navMap.ResetJumpRequest();
                // On désactive l'ennemi pendant le saut
                enemy.active = false;
            }
        }

        // ── Cible FTL ─────────────────────────────────────────────────────────
        float ftlTarget = 0.0f;
        if (isJumping) {
            ftlTarget = 1.0f;
            jumpTimer -= dt;

            // Changement de secteur à mi-parcours (écran noir)
            if (jumpTimer <= JUMP_DURATION * 0.5f && pendingSectorIdx != -1) {
                currentSectorIdx = pendingSectorIdx;
                const Sector& s  = SECTORS[currentSectorIdx % SECTOR_COUNT];
                bg.SetSector(s.seed, s.c1, s.c2);
                hud.PushNotification(s.name, { 200, 220, 255, 255 });
                pendingSectorIdx = -1;
            }

            if (jumpTimer <= 0.0f) {
                isJumping      = false;
                flashAlpha     = 1.0f;
                shakeIntensity = 15.0f;
                enemySpawnTimer = 6.0f; // Respawn ennemi après arrivée
            }
        } else {
            ftlTarget = IsKeyDown(KEY_SPACE) ? 1.0f : 0.0f;
            if (IsKeyPressed(KEY_SPACE)) {
                flashAlpha     = 1.0f;
                shakeIntensity = 20.0f;
                audio.PlaySfx("jump");
            }
        }

        // ── FTL lerp (UNE seule mise à jour) ──────────────────────────────────
        ftlLerp += (ftlTarget - ftlLerp) * (2.5f * dt);
        bg.Update(dt, ftlLerp, false); // on passe ftlLerp déjà calculé

        // ── Rotation vaisseau ─────────────────────────────────────────────────
        float ftlTilt   = ftlLerp * -5.0f;
        float targetRot = 90.0f + ftlTilt;
        shipRotation   += (targetRot - shipRotation) * (10.0f * dt);

        // ── Étoiles ───────────────────────────────────────────────────────────
        float baseSpeed = SCROLL_SPEED + SCROLL_SPEED * 25.0f * ftlLerp;
        for (int i = 0; i < STARS; i++) {
            stars[i].x -= baseSpeed * stars[i].z * 100.0f * dt;
            if (stars[i].x <= 0) {
                stars[i].x += screenW;
                stars[i].y  = (float)GetRandomValue(0, (int)screenH);
            }
        }

        // ── Shake ─────────────────────────────────────────────────────────────
        float cruiseShake = ftlLerp * 1.5f;
        shakeIntensity = std::max(shakeIntensity * shakeDamping, cruiseShake);
        if (shakeIntensity > 0.1f) {
            camera.offset.x = (float)GetRandomValue(-100, 100) / 100.0f * shakeIntensity;
            camera.offset.y = (float)GetRandomValue(-100, 100) / 100.0f * shakeIntensity;
        } else {
            camera.offset = { 0, 0 };
        }

        flashAlpha = std::max(0.0f, flashAlpha - 1.5f * dt);

        // ── Audio ─────────────────────────────────────────────────────────────
        smoothFtl += (ftlLerp - smoothFtl) * (5.0f * dt);
        audio.Update();
        audio.SetMusicPitch("engine",    1.0f + smoothFtl * 0.4f);
        audio.SetMusicVolume("engine",   0.2f + smoothFtl * 0.3f);
        audio.SetMusicVolume("ambience", 0.4f - ftlLerp   * 0.2f);

        // ── Tir joueur ────────────────────────────────────────────────────────
        laserCooldown   -= dt;
        missileCooldown -= dt;
        float fireAngle  = shipRotation - 90.0f;

        if (!navMap.IsActive() && !isJumping) {
            if (IsKeyDown(KEY_F) && laserCooldown <= 0) {
                projMgr.Spawn(ProjType::LASER, shipPos, fireAngle);
                laserCooldown = LASER_CD;
                audio.PlaySfx("laser");
            }
            if (IsKeyPressed(KEY_G) && missileCooldown <= 0) {
                projMgr.Spawn(ProjType::TORPEDO, shipPos, fireAngle);
                missileCooldown = MISSILE_CD;
                audio.PlaySfx("missile");
            }
            if (IsKeyPressed(KEY_E)) {
                projMgr.Spawn(ProjType::BIG_BULLET, shipPos, fireAngle);
            }
        }

        // ── Ennemi — spawn & update ───────────────────────────────────────────
        if (!isJumping && ftlLerp < 0.1f) {
            if (!enemy.active) {
                enemySpawnTimer -= dt;
                if (enemySpawnTimer <= 0.0f) {
                    SpawnEnemy(enemy, screenW, screenH);
                    hud.PushNotification("ENEMY DETECTED", { 255, 80, 80, 255 });
                }
            } else {
                UpdateEnemy(enemy, shipPos, projMgr, dt, ftlLerp,
                            shakeIntensity, shipStats, hud);

                // Collision projectiles joueur → ennemi
                for (auto& p : projMgr.GetAllMutable()) {
                    if (!p.active || p.isEnemy) continue;   
                    float dx = p.pos.x - enemy.pos.x;
                    float dy = p.pos.y - enemy.pos.y;
                    if (sqrtf(dx*dx + dy*dy) < 24.0f) {
                        float dmg = (p.type == ProjType::TORPEDO)  ? 35.0f
                                  : (p.type == ProjType::BIG_BULLET) ? 20.0f
                                                                      : 10.0f;
                        enemy.hull -= dmg;
                        p.active    = false;
                        shakeIntensity = std::max(shakeIntensity, dmg * 0.3f);

                        // Particules d'impact
                        for (int i = 0; i < 8; i++) {
                            float ang = randf() * 6.28318f;
                            float spd = 60.0f + randf() * 120.0f;
                            particles.push_back({
                                enemy.pos,
                                { cosf(ang) * spd, sinf(ang) * spd },
                                1.0f,
                                { 255, (unsigned char)(100 + randf() * 80), 30, 255 }
                            });
                        }

                        if (enemy.hull <= 0.0f) {
                            enemy.active    = false;
                            enemySpawnTimer = 10.0f + randf() * 5.0f;
                            hud.PushNotification("ENEMY DESTROYED", { 80, 255, 120, 255 });
                            shakeIntensity = std::max(shakeIntensity, 12.0f);
                            flashAlpha = 0.3f;
                        }
                    }
                }

                // Collision projectiles ennemi → joueur
                for (auto& p : projMgr.GetAllMutable()) {
                    if (!p.active || !p.isEnemy) continue; 
                    float dx = p.pos.x - shipX;
                    float dy = p.pos.y - shipY;
                    if (sqrtf(dx*dx + dy*dy) < 28.0f) {
                        shipStats.TakeDamage(12.0f);
                        p.active = false;
                        shakeIntensity = std::max(shakeIntensity, 6.0f);
                        hud.PushNotification("-12 HIT", { 255, 80, 80, 255 });
                    }
                }
            }
        }

        // ── Particules ────────────────────────────────────────────────────────
        for (auto it = particles.begin(); it != particles.end();) {
            it->pos.x += it->vel.x * dt - ftlLerp * 1500.0f * dt;
            it->pos.y += it->vel.y * dt;
            it->life  -= dt * 2.0f;
            if (it->life <= 0) it = particles.erase(it); else ++it;
        }

        // ── Projectiles update ────────────────────────────────────────────────
        projMgr.Update(dt);
        shipStats.Update(dt);
        hud.Update(dt, shipStats);

        // ── Blur shader uniform ───────────────────────────────────────────────
        float blurSize[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
        SetShaderValue(blurShader, locBlurSize, blurSize, SHADER_UNIFORM_VEC2);

        // ═════════════════════════════════════════════════════════════════════
        // RENDU — passe glow (RenderTexture)
        // ═════════════════════════════════════════════════════════════════════
        BeginTextureMode(glowTarget);
            ClearBackground(BLANK);

            for (int i = 0; i < STARS; i++) {
                if (ftlLerp > 0.1f) {
                    float streakLen = 15.0f * stars[i].z * ftlLerp;
                    DrawLineEx(
                        { stars[i].x, stars[i].y },
                        { stars[i].x + streakLen, stars[i].y },
                        1.0f + stars[i].z,
                        ColorAlpha(stars[i].color, 0.4f + ftlLerp * 0.3f)
                    );
                } else {
                    DrawCircleV({ stars[i].x, stars[i].y },
                                std::max(stars[i].z, 0.8f), stars[i].color);
                }
            }
            projMgr.DrawGlow(t);
        EndTextureMode();

        // ═════════════════════════════════════════════════════════════════════
        // RENDU — frame principale
        // ═════════════════════════════════════════════════════════════════════
        BeginDrawing();
            ClearBackground(BLACK);

            // Fond shader (nébuleuse)
            bg.Draw((int)screenW, (int)screenH);

            // Fondu noir pendant le saut
            if (isJumping) {
                float progress = jumpTimer / JUMP_DURATION;
                float alpha    = sinf(progress * PI);
                DrawRectangle(0, 0, (int)screenW, (int)screenH,
                              ColorAlpha(BLACK, alpha));
            }

            if (!navMap.IsActive()) {

                BeginMode2D(camera);

                    // Étoiles + glow flouté
                    for (int i = 0; i < STARS; i++) {
                        if (ftlLerp > 0.1f) {
                            float streakLen = 15.0f * stars[i].z * ftlLerp;
                            DrawLineEx(
                                { stars[i].x, stars[i].y },
                                { stars[i].x + streakLen, stars[i].y },
                                1.0f + stars[i].z,
                                ColorAlpha(stars[i].color, 0.4f + ftlLerp * 0.3f)
                            );
                        } else {
                            DrawCircleV({ stars[i].x, stars[i].y },
                                        stars[i].z, stars[i].color);
                        }
                    }

                    // Glow layer flou (shader appliqué ici)
                    BeginShaderMode(blurShader);
                        DrawTextureRec(
                            glowTarget.texture,
                            { 0, 0, (float)glowTarget.texture.width,
                                   -(float)glowTarget.texture.height },
                            { 0, 0 }, WHITE
                        );
                    EndShaderMode();

                    // Particules d'explosion
                    for (const auto& p : particles) {
                        DrawCircleV(p.pos, p.life * 4.0f,
                                    ColorAlpha(p.color, p.life));
                    }

                    // Ennemi
                    DrawEnemy(enemy, t);

                    // Moteur joueur
                    if (engineTex.id > 0) {
                        int   idx     = (int)(t * 12.0f) % 12;
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

                    // Bouclier joueur
                    if (shieldTex.id > 0) {
                        const int   SHIELD_FRAMES = 40;
                        const float SHIELD_FPS    = 12.0f;
                        int   frame  = (int)(t * SHIELD_FPS) % SHIELD_FRAMES;
                        float frameW = (float)shieldTex.width  / SHIELD_FRAMES;
                        float frameH = (float)shieldTex.height;
                        unsigned char shieldA =
                            (unsigned char)(80 + shipStats.ShieldPct() * 100);
                        DrawTexturePro(
                            shieldTex,
                            { frame * frameW, 0, frameW, frameH },
                            { shipX, shipY, frameW, frameH },
                            { frameW / 2.0f, frameH / 2.0f },
                            shipRotation, { 100, 200, 255, shieldA }
                        );
                    }

                    // Vaisseau joueur
                    const Aseprite& sSprite = ship.GetSprite();
                    float shipW = IsAsepriteValid(sSprite)
                                  ? (float)GetAsepriteWidth(sSprite)  : 128.0f;
                    float shipH = IsAsepriteValid(sSprite)
                                  ? (float)GetAsepriteHeight(sSprite) : 128.0f;
                    if (IsAsepriteValid(sSprite))
                        DrawAsepritePro(sSprite, 0,
                                        { shipX, shipY, shipW, shipH },
                                        { shipW / 2.0f, shipH / 2.0f },
                                        shipRotation, WHITE);

                    // Projectiles
                    projMgr.Draw();

                EndMode2D();

                // Flash blanc (hors camera shake)
                if (flashAlpha > 0.0f)
                    DrawRectangle(0, 0, (int)screenW, (int)screenH,
                                  ColorAlpha(WHITE, flashAlpha));

                // HUD
                hud.Draw(shipStats, (int)screenW, (int)screenH);

                // Debug / contrôles
                DrawTextEx(hud.GetFont(), "F: Laser  G: Torpedo  E: BigBullet",
                           { 20, 20 }, 16, 1, { 160, 170, 190, 160 });
                DrawTextEx(hud.GetFont(), "ESPACE: FTL  TAB: Map",
                           { 20, 42 }, 16, 1, { 160, 170, 190, 160 });
                DrawTextEx(hud.GetFont(), "H/J/K: Dégâts (test)",
                           { 20, 64 }, 16, 1, { 100, 110, 130, 120 });

                // Indicateur de saut
                if (isJumping) {
                    const char* jumpTxt = "FTL JUMP IN PROGRESS...";
                    float tw = MeasureTextEx(hud.GetFont(), jumpTxt, 20, 1).x;
                    float pulse = 0.6f + 0.4f * sinf(t * 4.0f);
                    DrawTextEx(hud.GetFont(), jumpTxt,
                               { (screenW - tw) / 2.0f, screenH - 50.0f },
                               20, 1, ColorAlpha(SKYBLUE, pulse));
                }

                // Nom du secteur courant (bas droite)
                const char* sName = SECTORS[currentSectorIdx % SECTOR_COUNT].name;
                float snw = MeasureTextEx(hud.GetFont(), sName, 14, 1).x;
                DrawTextEx(hud.GetFont(), sName,
                           { screenW - snw - 16.0f, screenH - 30.0f },
                           14, 1, { 120, 140, 180, 140 });

            } else {
                navMap.Draw(hud.GetFont());
            }

        EndDrawing();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    if (IsAsepriteValid(enemy.sprite)) UnloadAseprite(enemy.sprite);
    UnloadTexture(enemy.engineTex);
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