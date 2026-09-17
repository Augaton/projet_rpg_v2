#pragma once
#include "Ship.hpp"

enum class EnemyKind {
    NORMAL,   // pirates / rebelles du secteur
    FLEET,    // flotte rebelle (balise rattrapée)
    DRONE,    // vaisseau automatisé
    BOSS,     // vaisseau amiral du dernier secteur
};

// ─── Vaisseaux jouables ──────────────────────────────────────────────────────

enum PlayerShipId {
    SHIP_AUGATON = 0,   // frégate équilibrée
    SHIP_VIPERE,        // intercepteur rapide et furtif
    SHIP_BASTION,       // croiseur lourdement protégé
    PLAYER_SHIP_COUNT
};

struct PlayerShipInfo {
    const char* name;
    const char* role;
    const char* description;
    const char* unlockHint;
};

const PlayerShipInfo& GetPlayerShipInfo(int id);
Ship MakePlayerShip(int id = SHIP_AUGATON);

// ─── Ennemis ─────────────────────────────────────────────────────────────────

enum EliteTrait {
    TRAIT_ARMORED = 0,  // coque +40 %
    TRAIT_OVERCHARGED,  // armes chargées 25 % plus vite
    TRAIT_SHIELDED,     // +2 barres de bouclier
    TRAIT_EVASIVE,      // +2 barres de moteurs
    TRAIT_COUNT
};
const char* EliteTraitName(int trait);

Ship MakeEnemyShip(int sector, EnemyKind kind);

// ─── Difficulté ──────────────────────────────────────────────────────────────

struct DifficultyDef {
    const char* name;
    float enemyHull;     // multiplicateur de coque ennemie
    float enemyCharge;   // vitesse de charge des armes ennemies
    float reward;        // multiplicateur de ferraille
    float fleet;         // avance de la flotte par saut
};
constexpr int DIFFICULTY_COUNT = 3;
const DifficultyDef& GetDifficulty(int level);   // 0 facile, 1 normal, 2 difficile
