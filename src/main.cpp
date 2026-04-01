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
#include "audio/random_gun_sfx.hpp"
#include "ship/Ship.hpp"
#include "projectile/ProjectileManager.hpp"
#include "ship/ShipStats.hpp"
#include "effect/SpaceBackground.hpp"
#include "map/galaxymap.hpp"
#include "hud/HUD.hpp"
#include "game/GameState.hpp"
#include "inventory/Item.hpp"
#include "inventory/Inventory.hpp"
#include "economy/Economy.hpp"
#include "shop/Shop.hpp"

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



// ─── Données secteurs ─────────────────────────────────────────────────────────

// Utilise la structure SectorDef de galexymap.hpp
#include "map/galaxymap.hpp"
static const SectorDef SECTORS[] = {
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
static int   randi(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void SpawnEnemy(Enemy& e, float screenW, float screenH) {
    // Positionné à droite de l'écran (split-screen)
    e.pos       = { screenW * 0.75f, screenH * 0.5f };
    e.vel       = { 0, 0 };
    e.rotation  = 270.0f;   // pointe vers la gauche
    e.hull      = 100.0f;
    e.fireTimer = 2.0f;
    e.state     = EnemyState::APPROACH;
    e.active    = true;
}

static void UpdateEnemy(Enemy& e, Vector2 playerPos,
                         ProjectileManager& projMgr,
                         float dt, float /*ftlLerp*/,
                         float& /*shakeIntensity*/,
                         ShipStats& /*shipStats*/, HUD& /*hud*/) {
    if (!e.active) return;

    // Ennemi fixe sur la droite du split-screen, pointant vers le joueur
    e.rotation = 270.0f;

    // ── Tir ennemi ────────────────────────────────────────────────────────────
    Vector2 toPlayer = { playerPos.x - e.pos.x, playerPos.y - e.pos.y };
    e.fireTimer -= dt;
    if (e.fireTimer <= 0.0f) {
        float fireAngle = atan2f(toPlayer.y, toPlayer.x) * RAD2DEG;
        projMgr.SpawnEnemy(e.pos, fireAngle);
        e.fireTimer = 1.5f + randf() * 1.5f;
    }
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
    navMap.Generate(WIDTH, HEIGHT, SECTORS, SECTOR_COUNT);

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

    // ── Layout split-screen (0=centré, 1=splitté) ─────────────────────────────
    float splitFactor = 0.0f;

    // ── Systèmes RPG ──────────────────────────────────────────────────────────
    GameState  gameState    = GameState::IDLE;
    EventType  currentEvent = EventType::NONE;
    Economy    economy;
    Inventory  inventory;
    Shop       shop;

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
    audio.LoadSfx("gun_1",      "asset/Sound/sfx/weapons/gun_1.wav");
    audio.LoadSfx("gun_2",      "asset/Sound/sfx/weapons/gun_2.wav");
    audio.LoadSfx("gun_3",      "asset/Sound/sfx/weapons/gun_3.wav");
    audio.LoadSfx("gun_4",      "asset/Sound/sfx/weapons/gun_4.wav");
    audio.LoadSfx("gun_5",      "asset/Sound/sfx/weapons/gun_5.wav");
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
    float bigBulletCd     = 0.0f;
    float bulletCd        = 0.0f;
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
    enemy.sprite    = LoadAseprite("asset/Base/Aseprite/frigate.aseprite");
    enemy.engineTex = LoadTexture("asset/Engine/PNGs/frigate.png");
    enemy.active    = false;

    float enemySpawnTimer = 99.0f;  // géré par événements carte
    (void)enemySpawnTimer;

    // ── Particules ────────────────────────────────────────────────────────────
    std::vector<Particle> particles;
    particles.reserve(128);

    // ── Saut FTL ──────────────────────────────────────────────────────────────
    float jumpTimer        = 0.0f;
    bool  isJumping        = false;
    int   pendingSectorIdx = -1;
    int   currentSectorIdx = 0;
    const float JUMP_DURATION = 3.0f;
    bool  derelictPending  = false;

    // ─────────────────────────────────────────────────────────────────────────
    // Boucle principale
    // ─────────────────────────────────────────────────────────────────────────
    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        t += dt;

        const float screenW = (float)GetScreenWidth();
        const float screenH = (float)GetScreenHeight();

        // ── splitFactor : transition douce entre centré et split ──────────────
        float targetSplit = (gameState == GameState::COMBAT ||
                             gameState == GameState::EVENT_SHIP) ? 1.0f : 0.0f;
        splitFactor += (targetSplit - splitFactor) * 5.0f * dt;

        // Position joueur selon split (0.5 centré → 0.25 split gauche)
        const float shipX = screenW * (0.5f - 0.25f * splitFactor);
        const float shipY = screenH * 0.5f;
        const Vector2 shipPos     = { shipX, shipY };
        const Vector2 enemyFixedPos = { screenW * 0.75f, screenH * 0.5f };

        Vector2 mousePos = GetMousePosition();

        // ── Loot épave automatique ────────────────────────────────────────────
        if (derelictPending) {
            derelictPending = false;
            int loot = randi(40, 180);
            economy.Earn(loot);
            char dbuf[48];
            snprintf(dbuf, sizeof(dbuf), "+%d CREDITS (DERELICT)", loot);
            hud.PushNotification(dbuf, { 200, 170, 60, 255 });
            gameState = GameState::IDLE;
        }

        // ── Shop update ───────────────────────────────────────────────────────
        if (shop.isOpen) {
            shop.Update(mousePos, economy, inventory, shipStats,
                        (int)screenW, (int)screenH);
            if (!shop.isOpen) gameState = GameState::IDLE;
        }

        // Ouvrir le shop manuellement (B pendant merchant)
        if (!shop.isOpen && gameState == GameState::EVENT_SHIP &&
            currentEvent == EventType::MERCHANT && IsKeyPressed(KEY_B)) {
            shop.Open();
        }
        // ESC pour passer l'événement
        if (!shop.isOpen && gameState == GameState::EVENT_SHIP &&
            IsKeyPressed(KEY_ESCAPE)) {
            gameState    = GameState::IDLE;
            currentEvent = EventType::NONE;
        }

        // ── Map de navigation ─────────────────────────────────────────────────
        if (!shop.isOpen && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_M)))
            navMap.Toggle();

        if (navMap.IsActive()) {
            navMap.Update(mousePos, dt);
            if (navMap.IsJumpRequested()) {
                bool isExit      = navMap.IsLastJumpToExit();
                pendingSectorIdx = navMap.GetTargetSector();
                isJumping        = true;
                jumpTimer        = JUMP_DURATION;
                audio.PlaySfx("jump");
                shipStats.fuel = std::max(0.0f, shipStats.fuel - 1.0f);
                if (isExit)
                    hud.PushNotification("ZONE COMPLETE - NEXT SECTOR",
                                         { 255, 200, 60, 255 });
                else
                    hud.PushNotification("FTL JUMP INITIATED",
                                         { 80, 180, 255, 255 });
                navMap.ResetJumpRequest();
                enemy.active = false;
                gameState    = GameState::IDLE;
                currentEvent = EventType::NONE;
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
                const SectorDef& s  = SECTORS[currentSectorIdx % SECTOR_COUNT];
                bg.SetSector(s.seed, s.c1, s.c2);

                NodeEvent ne = navMap.GetLastJumpEvent();

                if (navMap.IsLastJumpToExit()) {
                    navMap.Regenerate((int)screenW, (int)screenH);
                    hud.PushNotification(s.name, { 200, 220, 255, 255 });
                    currentEvent = EventType::NONE;
                } else {
                    hud.PushNotification(s.name, { 200, 220, 255, 255 });
                    switch (ne) {
                        case NodeEvent::COMBAT:
                            currentEvent = EventType::COMBAT; break;
                        case NodeEvent::MERCHANT:
                            currentEvent = EventType::MERCHANT; break;
                        case NodeEvent::DERELICT:
                            currentEvent = EventType::DERELICT;
                            derelictPending = true; break;
                        default:
                            currentEvent = EventType::NONE; break;
                    }
                }
                pendingSectorIdx = -1;
            }

            if (jumpTimer <= 0.0f) {
                isJumping      = false;
                flashAlpha     = 1.0f;
                shakeIntensity = 15.0f;
                projMgr.Clear();

                if (currentEvent == EventType::COMBAT) {
                    SpawnEnemy(enemy, screenW, screenH);
                    gameState = GameState::COMBAT;
                    hud.PushNotification("ENEMY DETECTED", { 255, 80, 80, 255 });
                } else if (currentEvent == EventType::MERCHANT) {
                    shop.GenerateStock(navMap.GetZoneLevel());
                    gameState = GameState::EVENT_SHIP;
                    hud.PushNotification("MERCHANT VESSEL DETECTED",
                                         { 80, 220, 80, 255 });
                } else if (currentEvent == EventType::DERELICT) {
                    gameState = GameState::EVENT_NONE;
                } else {
                    gameState = GameState::IDLE;
                }
            }
        } else {
            ftlTarget = 0.0f;
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
        bigBulletCd     -= dt;
        bulletCd        -= dt;
        float fireAngle  = shipRotation - 90.0f;

        const Item* equipped = inventory.GetEquippedWeapon();

        if (!navMap.IsActive() && !isJumping && !shop.isOpen &&
            gameState == GameState::COMBAT) {

            if (IsKeyDown(KEY_F) && laserCooldown <= 0) {
                float cd  = equipped ? equipped->weapon.cooldown : 0.18f;
                float dmg = equipped ? equipped->weapon.damage   : -1.0f;
                ProjType pt = equipped ? equipped->weapon.projType : ProjType::LASER;
                projMgr.Spawn(pt, shipPos, fireAngle, dmg);
                laserCooldown = cd;
                audio.PlaySfx(equipped ? equipped->weapon.sfxKey : "laser");
            }
            if (IsKeyPressed(KEY_G) && missileCooldown <= 0) {
                projMgr.Spawn(ProjType::TORPEDO, shipPos, fireAngle);
                missileCooldown = MISSILE_CD;
                audio.PlaySfx("missile");
            }
            if (IsKeyPressed(KEY_E) && bigBulletCd <= 0) {
                projMgr.Spawn(ProjType::BIG_BULLET, shipPos, fireAngle);
                bigBulletCd = 0.4f;
                audio.PlaySfx(random_gun_sfx<5>("gun_").c_str());
            }
            if (IsKeyPressed(KEY_SPACE) && bulletCd <= 0) {
                projMgr.Spawn(ProjType::BULLET, shipPos, fireAngle);
                bulletCd = 0.2f;
                audio.PlaySfx(random_gun_sfx<5>("gun_").c_str());
            }
        }

        // ── Ennemi (combat uniquement) ────────────────────────────────────────
        if (gameState == GameState::COMBAT && !isJumping && ftlLerp < 0.1f) {
            if (enemy.active) {
                // L'ennemi est fixe sur la droite du split-screen
                enemy.pos = enemyFixedPos;
                UpdateEnemy(enemy, shipPos, projMgr, dt, ftlLerp,
                            shakeIntensity, shipStats, hud);

                // Collision projectiles joueur → ennemi
                for (auto& p : projMgr.GetAllMutable()) {
                    if (!p.active || p.isEnemy) continue;   
                    float dx = p.pos.x - enemy.pos.x;
                    float dy = p.pos.y - enemy.pos.y;
                    if (sqrtf(dx*dx + dy*dy) < 24.0f) {
                        enemy.hull -= p.damage;
                        p.active    = false;
                        shakeIntensity = std::max(shakeIntensity, p.damage * 0.3f);

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
                            enemy.active = false;
                            gameState    = GameState::IDLE;
                            currentEvent = EventType::NONE;

                            int zoneBonus = navMap.GetZoneLevel() * 20;
                            int reward    = randi(40 + zoneBonus, 120 + zoneBonus);
                            economy.Earn(reward);
                            char rbuf[48];
                            snprintf(rbuf, sizeof(rbuf),
                                     "ENEMY DESTROYED  +%d CR", reward);
                            hud.PushNotification(rbuf, { 80, 255, 120, 255 });
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
                        shipStats.TakeDamage(p.damage);
                        p.active = false;
                        shakeIntensity = std::max(shakeIntensity, 6.0f);
                        char hitbuf[24];
                        snprintf(hitbuf, sizeof(hitbuf), "-%.0f HIT", p.damage);
                        hud.PushNotification(hitbuf, { 255, 80, 80, 255 });
                    }
                }
            } else {
                gameState = GameState::IDLE;
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

                    // Étoiles
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

                    // Glow layer flou
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

                    // Ennemi (combat)
                    DrawEnemy(enemy, t);

                    // Vaisseau marchand (placeholder géométrique)
                    if (gameState == GameState::EVENT_SHIP &&
                        currentEvent == EventType::MERCHANT) {
                        DrawPoly(enemyFixedPos, 6, 24.0f, t * 10.0f,
                                 { 80, 220, 80, 200 });
                        DrawPolyLines(enemyFixedPos, 6, 24.0f, t * 10.0f,
                                      { 100, 255, 100, 255 });
                    }

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

                // Flash blanc
                if (flashAlpha > 0.0f)
                    DrawRectangle(0, 0, (int)screenW, (int)screenH,
                                  ColorAlpha(WHITE, flashAlpha));

                // ── Séparateur split-screen (fade si pas de split) ────────────
                if (splitFactor > 0.05f) {
                    float midX  = screenW * 0.5f;
                    float alpha = std::min(splitFactor * 2.0f, 1.0f);
                    DrawRectangle((int)midX - 1, 0, 2, (int)screenH,
                                  ColorAlpha({ 40, 70, 120, 100 }, alpha));
                    DrawRectangleGradientH(
                        (int)midX, 0, (int)(screenW * 0.12f), (int)screenH,
                        ColorAlpha({ 0, 0, 10, 80 }, alpha),
                        ColorAlpha({ 0, 0, 0, 0 }, alpha));
                }

                // ── HUD joueur ────────────────────────────────────────────────
                hud.Draw(shipStats, (int)screenW, (int)screenH);
                hud.DrawCredits(economy.credits, (int)screenW, (int)screenH);

                // ── HUD ennemi ────────────────────────────────────────────────
                if (enemy.active && gameState == GameState::COMBAT) {
                    ShipStats enemyHudStats;
                    enemyHudStats.hull      = enemy.hull;
                    enemyHudStats.maxHull   = 100.0f;
                    enemyHudStats.shield    = 0.0f;
                    enemyHudStats.maxShield = 1.0f;
                    enemyHudStats.fuel      = 0.0f;
                    enemyHudStats.maxFuel   = 1.0f;
                    hud.DrawForEnemy(enemyHudStats, (int)screenW, (int)screenH);
                }

                // ── Event overlay marchand ────────────────────────────────────
                if (!shop.isOpen && gameState == GameState::EVENT_SHIP &&
                    currentEvent == EventType::MERCHANT) {
                    hud.DrawEventBox("MERCHANT VESSEL",
                                     "A friendly ship hails you.",
                                     (int)screenW, (int)screenH);
                }

                // ── Contrôles ─────────────────────────────────────────────────
                DrawTextEx(hud.GetFont(),
                           "F: Fire  G: Torpedo  E: BigShot  SPACE: Bullet",
                           { 20, 20 }, 14, 1, { 160, 170, 190, 130 });
                DrawTextEx(hud.GetFont(),
                           "TAB: Map  B: Shop (merchant)",
                           { 20, 38 }, 14, 1, { 160, 170, 190, 130 });

                // Arme équipée
                if (equipped) {
                    char ewBuf[80];
                    snprintf(ewBuf, sizeof(ewBuf), "EQUIPPED: %s",
                             equipped->name.c_str());
                    DrawTextEx(hud.GetFont(), ewBuf, { 20, 56 }, 13, 1,
                               Item::RarityColor(equipped->rarity));
                }

                // ── Indicateur de saut ────────────────────────────────────────
                if (isJumping) {
                    const char* jumpTxt = "FTL JUMP IN PROGRESS...";
                    float tw = MeasureTextEx(hud.GetFont(), jumpTxt, 20, 1).x;
                    float pulse = 0.6f + 0.4f * sinf(t * 4.0f);
                    DrawTextEx(hud.GetFont(), jumpTxt,
                               { (screenW - tw) / 2.0f, screenH - 50.0f },
                               20, 1, ColorAlpha(SKYBLUE, pulse));
                }

                // ── Secteur + zone courants ────────────────────────────────────
                const char* sName = SECTORS[currentSectorIdx % SECTOR_COUNT].name;
                char zoneLabel[48];
                snprintf(zoneLabel, sizeof(zoneLabel), "%s  [Zone %d]",
                         sName, navMap.GetZoneLevel() + 1);
                float snw = MeasureTextEx(hud.GetFont(), zoneLabel, 14, 1).x;
                DrawTextEx(hud.GetFont(), zoneLabel,
                           { (screenW - snw) * 0.5f, screenH - 24.0f },
                           14, 1, { 120, 140, 180, 140 });

            } else {
                navMap.SetFuelInfo((int)shipStats.fuel, (int)shipStats.maxFuel);
                navMap.Draw(hud.GetFont());
            }

            // ── Shop overlay ──────────────────────────────────────────────────
            if (shop.isOpen)
                shop.Draw(hud.GetFont(), economy, inventory,
                          (int)screenW, (int)screenH);

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