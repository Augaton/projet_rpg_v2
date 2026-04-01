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

// ─── Définitions de types d'ennemis ──────────────────────────────────────────

struct EnemyDef {
    const char* spritePath;
    const char* enginePath;
    float       maxHull;
    float       fireCooldown;
    float       projDamage;
    ProjType    projType;
    int         creditBase;
    int         minZone;
};

static const EnemyDef ENEMY_DEFS[] = {
    // Zone 0+ : légers
    { "asset/Base/Aseprite/scout.aseprite",
      "asset/Engine/PNGs/scout.png",         60.f, 1.0f,  8.f, ProjType::BULLET,      40, 0 },
    { "asset/Base/Aseprite/fighter.aseprite",
      "asset/Engine/PNGs/fighter.png",        80.f, 1.2f, 12.f, ProjType::LASER,       60, 0 },
    // Zone 1+ : moyens
    { "asset/Base/Aseprite/frigate.aseprite",
      "asset/Engine/PNGs/frigate.png",       100.f, 1.5f, 15.f, ProjType::LASER,       90, 1 },
    { "asset/Base/Aseprite/torpedo_ship.aseprite",
      "asset/Engine/PNGs/torpedo_ship.png",  115.f, 2.0f, 35.f, ProjType::TORPEDO,    130, 1 },
    // Zone 2+ : lourds
    { "asset/Base/Aseprite/bomber.aseprite",
      "asset/Engine/PNGs/bomber.png",        150.f, 2.5f, 22.f, ProjType::BIG_BULLET, 170, 2 },
    { "asset/Base/Aseprite/support_ship.aseprite",
      "asset/Engine/PNGs/support_ship.png",  130.f, 1.0f, 10.f, ProjType::BULLET,     120, 2 },
    // Zone 3+ : boss
    { "asset/Base/Aseprite/battlecruiser.aseprite",
      "asset/Engine/PNGs/battlecruiser.png", 220.f, 1.8f, 20.f, ProjType::LASER,      220, 3 },
    // Zone 4+ : boss ultime
    { "asset/Base/Aseprite/dreadnought.aseprite",
      "asset/Engine/PNGs/dreadnought.png",   320.f, 2.0f, 28.f, ProjType::TORPEDO,    350, 4 },
};
static constexpr int ENEMY_DEF_COUNT = 8;

// Choisit un def d'ennemi adapté au niveau de zone
static int PickEnemyDef(int zoneLevel) {
    int candidates[ENEMY_DEF_COUNT];
    int count = 0;
    for (int i = 0; i < ENEMY_DEF_COUNT; i++) {
        if (ENEMY_DEFS[i].minZone <= zoneLevel)
            candidates[count++] = i;
    }
    if (count == 0) return 0;
    // Favoriser les 3 derniers defs disponibles (plus difficiles)
    int start = std::max(0, count - 3);
    return candidates[start + rand() % (count - start)];
}

struct Enemy {
    Vector2    pos;
    Vector2    basePos;  // centre d'oscillation
    Vector2    vel;
    float      rotation;
    float      hull;
    float      maxHull;
    float      projDmg;
    ProjType   projType;
    float      fireTimer;
    float      movePhase;
    EnemyState state;
    bool       active;
    int        defIdx;

    // Assets (rechargés à chaque spawn)
    Aseprite   sprite;
    Texture2D  engineTex;
};

static float randf() { return (rand() % 1000) / 1000.0f; }
static int   randi(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

// ─── Helpers ──────────────────────────────────────────────────────────────────

static void SpawnEnemy(Enemy& e, float screenW, float screenH, int defIdx) {
    const EnemyDef& def = ENEMY_DEFS[defIdx];

    // Décharger les anciens assets si présents
    if (e.engineTex.id > 0) { UnloadTexture(e.engineTex); e.engineTex = {}; }
    if (IsAsepriteValid(e.sprite)) { UnloadAseprite(e.sprite); e.sprite = {}; }

    // Charger les nouveaux assets
    e.sprite    = LoadAseprite(def.spritePath);
    e.engineTex = LoadTexture(def.enginePath);

    Vector2 center  = { screenW * 0.75f, screenH * 0.5f };
    e.pos           = center;
    e.basePos       = center;
    e.vel           = { 0, 0 };
    e.rotation      = 270.0f;
    e.hull          = def.maxHull;
    e.maxHull       = def.maxHull;
    e.projDmg       = def.projDamage;
    e.projType      = def.projType;
    e.fireTimer     = 2.0f;
    e.movePhase     = randf() * 6.28318f;
    e.state         = EnemyState::APPROACH;
    e.active        = true;
    e.defIdx        = defIdx;
}

static void UpdateEnemy(Enemy& e, Vector2 playerPos,
                         ProjectileManager& projMgr,
                         float dt, float /*ftlLerp*/,
                         float& /*shakeIntensity*/,
                         ShipStats& /*shipStats*/, HUD& /*hud*/) {
    if (!e.active) return;

    // ── Mouvement oscillatoire ────────────────────────────────────────────────
    // Les ennemis légers (scout/fighter) bougent plus vite et plus largement
    float speed  = 1.0f + (float)std::min(e.defIdx, 3) * 0.15f;
    float ampY   = (e.defIdx <= 1) ? 70.0f : 40.0f;
    float ampX   = (e.defIdx <= 1) ? 28.0f :  8.0f;

    e.movePhase += dt * speed;
    float targetY = e.basePos.y + sinf(e.movePhase) * ampY;
    float targetX = e.basePos.x + cosf(e.movePhase * 0.6f) * ampX;
    e.pos.y += (targetY - e.pos.y) * 4.0f * dt;
    e.pos.x += (targetX - e.pos.x) * 3.0f * dt;

    // Inclinaison dynamique (légère)
    float lean  = cosf(e.movePhase) * ((e.defIdx <= 1) ? 12.0f : 5.0f);
    e.rotation  = 270.0f + lean;

    // ── Tir ──────────────────────────────────────────────────────────────────
    Vector2 toPlayer = { playerPos.x - e.pos.x, playerPos.y - e.pos.y };
    e.fireTimer -= dt;
    if (e.fireTimer <= 0.0f) {
        float fireAngle = atan2f(toPlayer.y, toPlayer.x) * RAD2DEG;
        projMgr.SpawnEnemyType(e.pos, fireAngle, e.projType, e.projDmg);

        // Les vaisseaux-torpilles tirent en salve de 2
        if (e.projType == ProjType::TORPEDO) {
            float offAngle = fireAngle + 8.0f;
            projMgr.SpawnEnemyType(e.pos, offAngle, ProjType::BULLET,
                                   e.projDmg * 0.3f);
        }
        e.fireTimer = ENEMY_DEFS[e.defIdx].fireCooldown
                      * (0.8f + randf() * 0.4f);
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
    float barW = 50.0f;
    float pct  = (e.maxHull > 0.0f) ? (e.hull / e.maxHull) : 0.0f;
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
    // Désactive la fermeture automatique par ESC
    SetExitKey(KEY_NULL);

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WIDTH, HEIGHT, "Augaton");
    SetExitKey(0);
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

    bool  inventoryOpen        = false;
    int   inventorySelectedIdx = -1;

    // ─────────────────────────────────────────────────────────────────────────
    // Boucle principale
    // ─────────────────────────────────────────────────────────────────────────
    bool pauseMenu = false;
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

        // Inventaire (touche I, désactivé pendant les jumps)
        if (!shop.isOpen && !navMap.IsActive() && !isJumping &&
            IsKeyPressed(KEY_I))
            inventoryOpen = !inventoryOpen;
        if (inventoryOpen && IsKeyPressed(KEY_ESCAPE)) {
            inventoryOpen = false;
        }

        // Menu pause (ESC hors shop/inventaire/map)
        if (!shop.isOpen && !inventoryOpen && !navMap.IsActive() && !isJumping) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                pauseMenu = !pauseMenu;
            }
        }

        // Quitter par F4 (toujours possible)
        if (IsKeyPressed(KEY_F4)) {
            break;
        }

        // Ouvrir le shop manuellement (B pendant merchant)
        if (!shop.isOpen && gameState == GameState::EVENT_SHIP &&
            currentEvent == EventType::MERCHANT && IsKeyPressed(KEY_B)) {
            shop.Open();
        }
        // ESC pour passer l'événement (seulement si inventaire et shop sont fermés et pas en pause)
        if (!shop.isOpen && !inventoryOpen && !pauseMenu && gameState == GameState::EVENT_SHIP &&
            IsKeyPressed(KEY_ESCAPE)) {
            gameState    = GameState::IDLE;
            currentEvent = EventType::NONE;
        }

        // ── Map de navigation (désactivée en COMBAT) ──────────────────────────
        if (!shop.isOpen && !inventoryOpen &&
            gameState != GameState::COMBAT &&
            (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_M)))
            navMap.Toggle();

        if (navMap.IsActive()) {
            navMap.Update(mousePos, dt);
            if (navMap.IsJumpRequested()) {
                bool isExit      = navMap.IsLastJumpToExit();
                // Le secteur n'avance qu'en franchissant la sortie de zone
                pendingSectorIdx = isExit
                                   ? (currentSectorIdx + 1) % SECTOR_COUNT
                                   : currentSectorIdx;
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

        // Si menu pause actif, bloquer le jeu (affichage + input)
        if (pauseMenu) {
            BeginDrawing();
            ClearBackground(Color{ 10, 14, 28, 220 });
            float mw = 340.0f, mh = 220.0f;
            float mx = ((float)GetScreenWidth() - mw) * 0.5f;
            float my = ((float)GetScreenHeight() - mh) * 0.5f;
            DrawRectangleRounded({ mx, my, mw, mh }, 0.08f, 8, Color{ 18, 24, 44, 245 });
            DrawRectangleRoundedLinesEx({ mx, my, mw, mh }, 0.08f, 8, 2.0f, Color{ 80, 120, 200, 255 });
            Font font = hud.GetFont();
            const char* title = "PAUSE";
            float tw = MeasureTextEx(font, title, 32, 1).x;
            DrawTextEx(font, title, { mx + (mw - tw) * 0.5f, my + 24 }, 32, 1, { 220, 200, 100, 255 });
            // Boutons
            const char* btns[2] = { "Résumé", "Quitter le jeu" };
            int hovered = -1;
            for (int i = 0; i < 2; ++i) {
                float bx = mx + 60;
                float by = my + 80 + i * 60;
                Rectangle br = { bx, by, mw - 120, 44 };
                bool isHover = CheckCollisionPointRec(GetMousePosition(), br);
                Color bg = isHover ? Color{ 60, 100, 180, 220 } : Color{ 30, 40, 65, 180 };
                DrawRectangleRounded(br, 0.18f, 6, bg);
                DrawRectangleRoundedLinesEx(br, 0.18f, 6, 1.5f, Color{ 80, 120, 200, 255 });
                float btw = MeasureTextEx(font, btns[i], 22, 1).x;
                DrawTextEx(font, btns[i], { mx + (mw - btw) * 0.5f, by + 10 }, 22, 1, WHITE);
                if (isHover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) hovered = i;
            }
            // Actions boutons
            if (hovered == 0) pauseMenu = false;
            if (hovered == 1) break;
            EndDrawing();
            continue;
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
                    int defIdx = PickEnemyDef(navMap.GetZoneLevel());
                    SpawnEnemy(enemy, screenW, screenH, defIdx);
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

        // Auto-aim : calculer l'angle vers l'ennemi actif si présent
        float fireAngle = shipRotation - 90.0f;
        if (enemy.active && gameState == GameState::COMBAT) {
            // Prédiction simple : viser la position future de l'ennemi
            Vector2 toEnemy = { enemy.pos.x - shipPos.x, enemy.pos.y - shipPos.y };
            float enemySpeed = sqrtf(enemy.vel.x * enemy.vel.x + enemy.vel.y * enemy.vel.y);
            float projSpeed = 600.0f; // Vitesse approx. des projectiles (ajuster si besoin)
            float dist = sqrtf(toEnemy.x * toEnemy.x + toEnemy.y * toEnemy.y);
            float t_impact = projSpeed > 1.0f ? dist / projSpeed : 0.0f;
            Vector2 predicted = { enemy.pos.x + enemy.vel.x * t_impact,
                                  enemy.pos.y + enemy.vel.y * t_impact };
            Vector2 aimVec = { predicted.x - shipPos.x, predicted.y - shipPos.y };
            fireAngle = atan2f(aimVec.y, aimVec.x) * RAD2DEG;
        }

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
                // Mise à jour position et tir (ennemi bouge librement sur la droite)
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
                    // Ligne fine uniquement — plus de gradient qui assombrit le côté droit
                    DrawRectangle((int)midX - 1, 0, 2, (int)screenH,
                                  ColorAlpha({ 60, 100, 180, 120 }, alpha));
                }

                // ── HUD joueur ────────────────────────────────────────────────
                hud.Draw(shipStats, (int)screenW, (int)screenH);
                hud.DrawCredits(economy.credits, (int)screenW, (int)screenH);

                // ── HUD ennemi ────────────────────────────────────────────────
                if (enemy.active && gameState == GameState::COMBAT) {
                    ShipStats enemyHudStats;
                    enemyHudStats.hull      = enemy.hull;
                    enemyHudStats.maxHull   = enemy.maxHull;
                    enemyHudStats.shield    = 0.0f;
                    enemyHudStats.maxShield = 1.0f;
                    enemyHudStats.fuel      = 0.0f;
                    enemyHudStats.maxFuel   = 1.0f;
                    hud.DrawForEnemy(enemyHudStats, (int)screenW, (int)screenH);
                }

                // ── Event overlay marchand ────────────────────────────────────
                if (!shop.isOpen && !inventoryOpen &&
                    gameState == GameState::EVENT_SHIP &&
                    currentEvent == EventType::MERCHANT) {
                    hud.DrawEventBox("MERCHANT VESSEL",
                                     "A friendly ship hails you.",
                                     (int)screenW, (int)screenH);
                }

                // ── Barre d'action (boutons en haut à gauche) ─────────────────
                {
                    auto drawBtn = [&](float bx, float by,
                                       const char* label, const char* key,
                                       bool disabled, bool active) {
                        Color bg2 = active    ? Color{ 30, 80, 50, 220 }
                                  : disabled  ? Color{ 20, 20, 30, 120 }
                                              : Color{ 20, 38, 80, 200 };
                        Color bc2 = active    ? Color{ 60, 200, 100, 255 }
                                  : disabled  ? Color{ 50, 55, 70, 120 }
                                              : Color{ 60, 100, 190, 255 };
                        Color tc2 = active    ? Color{ 80, 255, 130, 255 }
                                  : disabled  ? Color{ 70, 75, 90, 160 }
                                              : WHITE;
                        DrawRectangleRounded({ bx, by, 96.0f, 24.0f }, 0.4f, 6, bg2);
                        DrawRectangleRoundedLinesEx({ bx, by, 96.0f, 24.0f },
                                                    0.4f, 6, 1.0f, bc2);
                        char buf[32];
                        snprintf(buf, sizeof(buf), "[%s] %s", key, label);
                        float tw2 = MeasureTextEx(hud.GetFont(), buf, 12, 1).x;
                        DrawTextEx(hud.GetFont(), buf,
                                   { bx + (96.0f - tw2) * 0.5f, by + 6 },
                                   12, 1, tc2);
                    };

                    bool mapOk = (gameState != GameState::COMBAT);
                    drawBtn(12, 12, "MAP",       "TAB", !mapOk,      navMap.IsActive());
                    drawBtn(114, 12, "INVENTAIRE", "I", false,        inventoryOpen);
                }

                // Arme équipée (sous les boutons)
                if (equipped) {
                    char ewBuf[80];
                    snprintf(ewBuf, sizeof(ewBuf), "EQUIPPED: %s",
                             equipped->name.c_str());
                    float ewW = MeasureTextEx(hud.GetFont(), ewBuf, 12, 1).x;
                    DrawTextEx(hud.GetFont(), ewBuf, { (screenW - ewW) * 0.5f, 42 }, 12, 1,
                               Item::RarityColor(equipped->rarity));
                }

                // ── Indicateur de saut ────────────────────────────────────────
                if (isJumping) {
                    const char* jumpTxt = "FTL JUMP IN PROGRESS...";
                    float tw = MeasureTextEx(hud.GetFont(), jumpTxt, 20, 1).x;
                    float pulse = 0.6f + 0.4f * sinf(t * 4.0f);
                    DrawTextEx(hud.GetFont(), jumpTxt,
                               { (screenW - tw) * 0.5f, screenH - 50.0f },
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

            // ── Inventaire overlay ─────────────────────────────────────────────
            if (inventoryOpen && !shop.isOpen) {
                const float PW = std::min(780.0f, (float)screenW - 40.0f);
                const float PH = (float)screenH - 50.0f;
                const float PX = (screenW - PW) / 2.0f;
                const float PY = 25.0f;
                Font font = hud.GetFont();

                DrawRectangle(0, 0, (int)screenW, (int)screenH, { 0, 0, 0, 185 });
                DrawRectangleRounded({ PX, PY, PW, PH }, 0.03f, 6,
                                     { 8, 12, 24, 245 });
                DrawRectangleRoundedLinesEx({ PX, PY, PW, PH }, 0.03f, 6,
                                            1.2f, { 50, 70, 110, 255 });

                // Titre
                const char* invTitle = "INVENTAIRE & VAISSEAUX";
                float itw = MeasureTextEx(font, invTitle, 20, 1).x;
                DrawTextEx(font, invTitle,
                           { (screenW - itw) * 0.5f, PY + 10 },
                           20, 1, { 220, 200, 100, 255 });

                const float LEFT_W = PW * 0.48f;
                const float RIGHT_X = PX + LEFT_W + 16;
                const float RIGHT_W = PW - LEFT_W - 24;
                const float LIST_Y  = PY + 46.0f;
                const float ROW_H   = 50.0f;
                const float ROW_GAP = 3.0f;

                // Navigation clavier dans la liste
                int itemCount = (int)inventory.items.size();
                if (IsKeyPressed(KEY_UP)   && inventorySelectedIdx > 0)
                    inventorySelectedIdx--;
                if (IsKeyPressed(KEY_DOWN) && inventorySelectedIdx < itemCount - 1)
                    inventorySelectedIdx++;
                if (itemCount == 0) inventorySelectedIdx = -1;

                // Clic souris
                { float ry = LIST_Y;
                  for (int i = 0; i < itemCount; i++) {
                    if (ry + ROW_H > PY + PH - 38) break;
                    Rectangle r = { PX + 8, ry, LEFT_W - 8, ROW_H };
                    if (CheckCollisionPointRec(GetMousePosition(), r) &&
                        IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
                        inventorySelectedIdx = i;
                    ry += ROW_H + ROW_GAP;
                  }
                }

                // E = équiper selon catégorie
                if (IsKeyPressed(KEY_E) && inventorySelectedIdx >= 0 &&
                    inventorySelectedIdx < itemCount) {
                    const Item& sel = inventory.items[inventorySelectedIdx];
                    if      (sel.category == ItemCategory::WEAPON)
                        inventory.EquipWeapon(inventorySelectedIdx);
                    else if (sel.category == ItemCategory::SHIELD_MOD)
                        inventory.EquipShield(inventorySelectedIdx);
                    else if (sel.category == ItemCategory::HULL_MOD)
                        inventory.EquipHull(inventorySelectedIdx);
                }

                // ── Liste items (gauche) ───────────────────────────────────────
                { float ry = LIST_Y;
                  for (int i = 0; i < itemCount; i++) {
                    if (ry + ROW_H > PY + PH - 38) break;
                    const Item& item = inventory.items[i];
                    bool  isSel = (i == inventorySelectedIdx);
                    bool  isEqW = (i == inventory.equippedWeaponIdx);
                    bool  isEqS = (i == inventory.equippedShieldIdx);
                    bool  isEqH = (i == inventory.equippedHullIdx);
                    bool  isEq  = (isEqW || isEqS || isEqH);

                    Color bg2 = isSel ? Color{ 35, 55, 100, 210 }
                                      : Color{ 14, 20, 38, 160 };
                    DrawRectangleRounded({ PX + 8, ry, LEFT_W - 8, ROW_H },
                                         0.12f, 4, bg2);
                    DrawRectangleRoundedLinesEx({ PX + 8, ry, LEFT_W - 8, ROW_H },
                                                 0.12f, 4, 0.8f,
                                                 isSel ? Color{ 70, 110, 180, 255 }
                                                       : Color{ 30, 40, 65, 150 });
                    Color rc = Item::RarityColor(item.rarity);
                    DrawRectangleRounded({ PX + 8, ry, 3, ROW_H }, 0.15f, 4, rc);

                    // Tronquer nom
                    std::string nm = item.name;
                    const float MAX_NW = LEFT_W * 0.70f;
                    while (nm.size() > 4 &&
                           MeasureTextEx(font, nm.c_str(), 13, 1).x > MAX_NW)
                        nm = nm.substr(0, nm.size() - 4) + "...";
                    DrawTextEx(font, nm.c_str(), { PX + 16, ry + 6 }, 13, 1, WHITE);

                    char cat2[48];
                    snprintf(cat2, sizeof(cat2), "%s • %s",
                             Item::CategoryName(item.category),
                             Item::RarityName(item.rarity));
                    DrawTextEx(font, cat2, { PX + 16, ry + ROW_H - 18 },
                               11, 1, ColorAlpha(rc, 0.8f));

                    if (isEq) {
                        DrawTextEx(font, "EQUIPÉ",
                                   { PX + LEFT_W - 58, ry + 6 },
                                   11, 1, { 80, 220, 80, 255 });
                    }
                    ry += ROW_H + ROW_GAP;
                  }
                                    if (itemCount == 0) {
                                        const char* emptyTxt = "Inventaire vide.";
                                        float etw = MeasureTextEx(font, emptyTxt, 14, 1).x;
                                        DrawTextEx(font, emptyTxt,
                                                             { (screenW - etw) * 0.5f, LIST_Y + 20 },
                                                             14, 1, { 120, 130, 150, 200 });
                                    }
                }

                // Séparateur
                DrawRectangle((int)(RIGHT_X - 8), (int)LIST_Y,
                              1, (int)(PH - 58), { 40, 55, 90, 120 });

                // ── Stats & équipement (droite) ───────────────────────────────
                { float ry = LIST_Y;
                  DrawTextEx(font, "ÉQUIPEMENT",
                             { RIGHT_X, ry }, 15, 1, { 180, 200, 255, 225 });
                  ry += 22;

                  auto drawSlot = [&](const char* label,
                                      const Item* eq, float& ry2) {
                    DrawTextEx(font, label, { RIGHT_X, ry2 }, 12, 1,
                               { 140, 155, 185, 200 });
                    if (eq) {
                        char sb[80];
                        snprintf(sb, sizeof(sb), "%s", eq->name.c_str());
                        // tronquer
                        std::string eqN = sb;
                        while (eqN.size() > 4 &&
                               MeasureTextEx(font, eqN.c_str(), 12, 1).x > RIGHT_W - 4)
                            eqN = eqN.substr(0, eqN.size() - 4) + "...";
                        DrawTextEx(font, eqN.c_str(),
                                   { RIGHT_X + 90, ry2 }, 12, 1,
                                   Item::RarityColor(eq->rarity));
                    } else {
                        DrawTextEx(font, "[vide]",
                                   { RIGHT_X + 90, ry2 }, 12, 1,
                                   { 80, 85, 100, 180 });
                    }
                    ry2 += 18;
                  };

                  const Item* eqW = inventory.GetEquippedWeapon();
                  const Item* eqS = inventory.GetEquippedShield();
                  const Item* eqH = inventory.GetEquippedHull();
                  drawSlot("ARME  :", eqW, ry);
                  if (eqW) {
                    char wd[48];
                    snprintf(wd, sizeof(wd), "  DMG %.0f  CD %.2fs",
                             eqW->weapon.damage, eqW->weapon.cooldown);
                    DrawTextEx(font, wd, { RIGHT_X, ry }, 11, 1,
                               { 160, 170, 190, 200 });
                    ry += 16;
                  }
                  drawSlot("BOUCLIER:", eqS, ry);
                  drawSlot("COQUE :", eqH, ry);
                  ry += 10;

                  DrawTextEx(font, "STATISTIQUES",
                             { RIGHT_X, ry }, 14, 1, { 180, 200, 255, 225 });
                  ry += 20;

                  char hBuf2[48], sBuf3[48], fBuf2[48], crBuf2[36];
                  snprintf(hBuf2,  sizeof(hBuf2),
                           "Coque        %.0f / %.0f",
                           shipStats.hull, shipStats.maxHull);
                  snprintf(sBuf3,  sizeof(sBuf3),
                           "Bouclier     %.0f / %.0f",
                           shipStats.shield, shipStats.maxShield);
                  snprintf(fBuf2,  sizeof(fBuf2),
                           "Carburant    %.0f / %.0f",
                           shipStats.fuel, shipStats.maxFuel);
                  snprintf(crBuf2, sizeof(crBuf2),
                           "Crédits      %d", economy.credits);
                  DrawTextEx(font, hBuf2,  { RIGHT_X, ry },      12, 1,
                             { 80, 210, 120, 255 });
                  DrawTextEx(font, sBuf3,  { RIGHT_X, ry + 16 }, 12, 1,
                             { 80, 180, 255, 255 });
                  DrawTextEx(font, fBuf2,  { RIGHT_X, ry + 32 }, 12, 1,
                             { 220, 160, 50, 255 });
                  DrawTextEx(font, crBuf2, { RIGHT_X, ry + 48 }, 12, 1,
                             { 255, 210, 80, 255 });
                  ry += 70;

                  char znBuf2[64];
                  snprintf(znBuf2, sizeof(znBuf2), "Zone %d  •  %s",
                           navMap.GetZoneLevel() + 1,
                           SECTORS[currentSectorIdx % SECTOR_COUNT].name);
                  DrawTextEx(font, znBuf2, { RIGHT_X, ry }, 11, 1,
                             { 120, 140, 180, 200 });
                }

                // Footer
                const char* invHint =
                    "[↑↓] Naviguer   [E] Équiper   [ESC] Fermer";
                float ihw = MeasureTextEx(font, invHint, 12, 1).x;
                DrawTextEx(font, invHint,
                           { (screenW - ihw) * 0.5f, PY + PH - 22 },
                           12, 1, { 100, 110, 140, 200 });
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