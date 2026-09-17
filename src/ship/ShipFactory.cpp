#include "ShipFactory.hpp"
#include "Hulls.hpp"
#include "Weapons.hpp"
#include "../core/Random.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

struct PlayerShipDef {
    PlayerShipInfo info;
    int  hullType;
    int  hull;
    int  reactor;
    int  levels[SYS_COUNT];
    int  weapons[3];
    int  augment;
    int  fuel, missiles, scrap;
};

//                                    SHLD ENGN WEAP REPR INVI HACK DEFN SENS
const PlayerShipDef PLAYER_SHIPS[PLAYER_SHIP_COUNT] = {
    { { "AUGATON", "Frégate polyvalente",
        "Équilibrée et fiable : rafale laser et missile Artemis. Idéale pour apprendre.",
        "Disponible" },
      HULL_FRIGATE, 30, 8, { 2, 2, 3, 1, 0, 0, 0, 0 },
      { WPN_BURST_LASER_II, WPN_ARTEMIS, -1 }, -1, 16, 12, 30 },
    { { "VIPERE", "Intercepteur furtif",
        "Esquive élevée, invisibilité et puissance de feu laser. Moins de coque, pas de lance-missiles.",
        "Atteindre le secteur 3" },
      HULL_FIGHTER, 26, 11, { 2, 5, 3, 0, 1, 0, 0, 0 },
      { WPN_BURST_LASER_II, WPN_HEAVY_LASER, -1 }, AUG_THRUSTERS, 18, 4, 25 },
    { { "BASTION", "Croiseur lourd",
        "Deux couches de bouclier, anti-missile et trois armes. Lent mais très résistant.",
        "Détruire 20 vaisseaux (total) ou gagner une partie" },
      HULL_BATTLECRUISER, 36, 12, { 4, 1, 4, 2, 0, 0, 1, 0 },
      { WPN_HEAVY_LASER, WPN_BURST_LASER_I, WPN_ION_BLAST }, -1, 14, 6, 20 },
};

constexpr DifficultyDef DIFFICULTIES[DIFFICULTY_COUNT] = {
    { "FACILE",    0.80f, 0.85f, 1.25f, 0.65f },
    { "NORMAL",    1.00f, 1.00f, 1.00f, 0.80f },
    { "DIFFICILE", 1.20f, 1.12f, 0.85f, 0.95f },
};

} // namespace

const PlayerShipInfo& GetPlayerShipInfo(int id) {
    return PLAYER_SHIPS[(id >= 0 && id < PLAYER_SHIP_COUNT) ? id : 0].info;
}

const DifficultyDef& GetDifficulty(int level) {
    return DIFFICULTIES[std::clamp(level, 0, DIFFICULTY_COUNT - 1)];
}

const char* EliteTraitName(int trait) {
    static const char* NAMES[TRAIT_COUNT] = { "BLINDÉ", "SURCHARGÉ", "BOUCLIER RENFORCÉ", "ÉVASIF" };
    return (trait >= 0 && trait < TRAIT_COUNT) ? NAMES[trait] : "";
}

Ship MakePlayerShip(int id) {
    const PlayerShipDef& d = PLAYER_SHIPS[(id >= 0 && id < PLAYER_SHIP_COUNT) ? id : 0];
    Ship s;
    s.name        = d.info.name;
    s.shipType    = (id >= 0 && id < PLAYER_SHIP_COUNT) ? id : 0;
    s.hullType    = d.hullType;
    s.hull        = s.maxHull = d.hull;
    s.reactor     = d.reactor;
    s.weaponSlots = MAX_WEAPON_SLOTS;
    for (int i = 0; i < SYS_COUNT; i++) s.sys[i].level = d.levels[i];
    for (int w : d.weapons) if (w >= 0) s.AddWeapon(w);
    if (d.augment >= 0) s.AddAugment(d.augment);

    s.fuel     = d.fuel;
    s.missiles = d.missiles;
    s.scrap    = d.scrap;

    s.AutoPowerNew();
    s.shieldLayers = s.MaxShieldLayers();
    return s;
}

namespace {

struct Archetype {
    const char*      name;
    int              hullType;
    int              hull;
    int              shields, engines, weapons, repair;
    std::vector<int> arms;
};

const std::vector<Archetype>& Archetypes() {
    static const std::vector<Archetype> list = {
        { "Éclaireur pirate",     HULL_SCOUT,         8, 2, 3, 2, 0, { WPN_BASIC_LASER, WPN_PULSE_CANNON } },
        { "Chasseur rebelle",     HULL_FIGHTER,      10, 2, 2, 3, 1, { WPN_BURST_LASER_I, WPN_BASIC_LASER } },
        { "Bombardier pirate",    HULL_BOMBER,       12, 2, 1, 3, 1, { WPN_ARTEMIS, WPN_HEAVY_LASER } },
        { "Torpilleur",           HULL_TORPEDO,      11, 2, 2, 3, 1, { WPN_ARTEMIS, WPN_ION_BLAST } },
        { "Navire de soutien",    HULL_SUPPORT,      14, 4, 1, 2, 2, { WPN_ION_BLAST, WPN_BASIC_LASER } },
        { "Frégate rebelle",      HULL_FRIGATE,      16, 4, 2, 4, 1, { WPN_BURST_LASER_II, WPN_HEAVY_LASER } },
        { "Croiseur de bataille", HULL_BATTLECRUISER,22, 4, 2, 5, 2, { WPN_BURST_LASER_II, WPN_ARTEMIS, WPN_HEAVY_LASER } },
    };
    return list;
}

// Archétypes disponibles selon le secteur
std::vector<int> PoolFor(int sector) {
    switch (sector) {
    case 0:  return { 0, 1, 2 };
    case 1:  return { 0, 1, 2, 3, 4 };
    case 2:  return { 1, 2, 3, 4, 5 };
    default: return { 3, 4, 5, 6 };
    }
}

} // namespace

Ship MakeEnemyShip(int sector, EnemyKind kind) {
    Ship s;

    if (kind == EnemyKind::BOSS) {
        s.name     = "Vaisseau amiral rebelle";
        s.hullType = HULL_DREADNOUGHT;
        s.hull     = 30;
        s.sys[SYS_SHIELDS].level = 4;
        s.sys[SYS_ENGINES].level = 2;
        s.sys[SYS_WEAPONS].level = 7;
        s.sys[SYS_REPAIR].level  = 2;
        s.sys[SYS_DEFENSE].level = 1;
        s.AddWeapon(WPN_BURST_LASER_II);
        s.AddWeapon(WPN_ARTEMIS);
        s.AddWeapon(WPN_ION_BLAST);
        s.AddWeapon(WPN_HEAVY_LASER);
    } else if (kind == EnemyKind::DRONE) {
        s.name     = "Drone automatisé";
        s.hullType = HULL_SCOUT;
        s.hull     = 7 + sector * 2;
        s.sys[SYS_SHIELDS].level = 2;
        s.sys[SYS_ENGINES].level = 3;
        s.sys[SYS_WEAPONS].level = 2;
        s.AddWeapon(WPN_ION_BLAST);
        s.AddWeapon(WPN_BASIC_LASER);
    } else {
        const auto& all = Archetypes();
        int idx = (kind == EnemyKind::FLEET) ? (sector >= 2 ? 6 : 5) : rng::Pick(PoolFor(sector));
        const Archetype& a = all[idx];

        s.name     = (kind == EnemyKind::FLEET) ? "Avant-garde de la flotte" : a.name;
        s.hullType = a.hullType;
        s.hull     = a.hull + sector * 2 + rng::Int(0, 2);
        // Progression lissée : 2 couches max jusqu'au secteur 3, 3 couches possibles au dernier
        s.sys[SYS_SHIELDS].level = std::min(a.shields + (sector >= 3 ? 2 : sector >= 2 ? 1 : 0), sector >= 3 ? 6 : 4);
        s.sys[SYS_ENGINES].level = a.engines + (sector >= 3 ? 1 : 0);
        s.sys[SYS_WEAPONS].level = a.weapons + (sector >= 3 ? 1 : 0);
        s.sys[SYS_REPAIR].level  = a.repair;
        for (int w : a.arms) s.AddWeapon(w);

        if (kind == EnemyKind::FLEET) {
            s.hull += 6;
            s.sys[SYS_SHIELDS].level = std::max(s.sys[SYS_SHIELDS].level, 4);
        } else {
            // Les secteurs avancés alignent des vaisseaux mieux armés
            if (sector >= 2 && s.AddWeapon(WPN_PULSE_CANNON))  s.sys[SYS_WEAPONS].level += 1;
            if (sector >= 3 && s.AddWeapon(WPN_BURST_LASER_I)) s.sys[SYS_WEAPONS].level += 2;
            // Certains vaisseaux tardifs embarquent une tourelle anti-missile
            if (sector >= 2 && rng::Chance(0.25f + 0.15f * (float)(sector - 2))) s.sys[SYS_DEFENSE].level = sector >= 3 ? 2 : 1;

            // Élite : un trait marqué, de meilleures récompenses
            if (rng::Chance(0.08f + 0.06f * (float)sector)) {
                s.trait = rng::Int(0, TRAIT_COUNT - 1);
                switch (s.trait) {
                case TRAIT_ARMORED:     s.hull = (int)std::lround(s.hull * 1.4f); break;
                case TRAIT_OVERCHARGED: s.chargeMult *= 1.25f;                   break;
                case TRAIT_SHIELDED:    s.sys[SYS_SHIELDS].level += 2;           break;
                case TRAIT_EVASIVE:     s.sys[SYS_ENGINES].level += 2;           break;
                default: break;
                }
                s.name = std::string(a.name) + " d'élite";
            }
        }
    }

    for (int i = 0; i < SYS_COUNT; i++)
        s.sys[i].level = std::min(s.sys[i].level, SystemMaxLevel(i));

    s.maxHull  = s.hull;
    s.reactor  = 0;
    for (const auto& x : s.sys) s.reactor += x.level;
    s.missiles = 99;

    s.AutoPower();
    s.shieldLayers = s.MaxShieldLayers();
    return s;
}
