#pragma once
#include "raylib.h"
#include "../projectile/Projectile.hpp"
#include <string>

// ─── Rareté / Catégorie ───────────────────────────────────────────────────────

enum class Rarity { COMMON = 0, UNCOMMON, RARE, EPIC, LEGENDARY };

enum class ItemCategory { WEAPON, SHIELD_MOD, HULL_MOD };

// ─── Définition d'une arme ────────────────────────────────────────────────────

struct WeaponDef {
    ProjType    projType      = ProjType::LASER;
    float       damage        = 10.0f;
    float       cooldown      = 0.18f;
    float       projSpeedMult = 1.0f;
    const char* sfxKey        = "laser";
};

// ─── Item générique ───────────────────────────────────────────────────────────

struct Item {
    std::string  name;
    ItemCategory category  = ItemCategory::WEAPON;
    Rarity       rarity    = Rarity::COMMON;
    int          rank      = 1;       // 1 – 5
    int          buyPrice  = 50;
    int          sellPrice = 25;

    WeaponDef    weapon;              // valide si WEAPON
    float        statBonus = 0.0f;    // bonus max si SHIELD/HULL_MOD

    // ── Helpers statiques ─────────────────────────────────────────────────────

    static const char* RarityName(Rarity r) noexcept {
        switch (r) {
            case Rarity::COMMON:    return "COMMON";
            case Rarity::UNCOMMON:  return "UNCOMMON";
            case Rarity::RARE:      return "RARE";
            case Rarity::EPIC:      return "EPIC";
            case Rarity::LEGENDARY: return "LEGENDARY";
        }
        return "";
    }

    static Color RarityColor(Rarity r) noexcept {
        switch (r) {
            case Rarity::COMMON:    return { 180, 180, 180, 255 };
            case Rarity::UNCOMMON:  return {  80, 200,  80, 255 };
            case Rarity::RARE:      return {  80, 140, 255, 255 };
            case Rarity::EPIC:      return { 180,  80, 255, 255 };
            case Rarity::LEGENDARY: return { 255, 180,  40, 255 };
        }
        return WHITE;
    }

    static const char* CategoryName(ItemCategory c) noexcept {
        switch (c) {
            case ItemCategory::WEAPON:     return "WEAPON";
            case ItemCategory::SHIELD_MOD: return "SHIELD MOD";
            case ItemCategory::HULL_MOD:   return "HULL MOD";
        }
        return "";
    }
};
