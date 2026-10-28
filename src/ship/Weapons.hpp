#pragma once
#include "../projectile/Projectile.hpp"

// Les identifiants sont sauvegardés : toute nouvelle arme s'ajoute à la fin.
enum WeaponId {
    WPN_BASIC_LASER = 0,
    WPN_BURST_LASER_I,
    WPN_BURST_LASER_II,
    WPN_HEAVY_LASER,
    WPN_PULSE_CANNON,
    WPN_ARTEMIS,
    WPN_HERMES,
    WPN_ION_BLAST,
    WPN_BURST_LASER_III,
    WPN_PIERCING_LASER,
    WPN_HEAVY_ION,
    WPN_PEGASUS,
    WPN_DUAL_CANNON,
    WPN_COUNT
};

struct WeaponDef {
    const char* name;
    const char* shortName;
    ProjType    proj;
    int         power;     // énergie requise
    float       charge;    // secondes de charge
    int         shots;     // projectiles par salve
    int         damage;    // dégâts coque + système par projectile
    int         ion;       // dégâts ioniques
    bool        missile;   // traverse les boucliers, consomme un missile par salve
    int         pierce;    // passe à travers jusqu'à N couches de bouclier (au-delà : bloqué)
    int         cost;      // prix en ferraille
    const char* sfx;       // "laser", "missile" ou "gun"
};

inline constexpr WeaponDef WEAPON_DEFS[WPN_COUNT] = {
    { "Laser basique",     "LASER",      ProjType::LASER,      1,  9.0f, 1, 1, 0, false, 0, 30, "laser"   },
    { "Laser rafale I",    "RAFALE I",   ProjType::LASER,      2, 10.0f, 2, 1, 0, false, 0, 45, "laser"   },
    { "Laser rafale II",   "RAFALE II",  ProjType::LASER,      2, 11.0f, 3, 1, 0, false, 0, 70, "laser"   },
    { "Laser lourd",       "LOURD",      ProjType::BIG_BULLET, 1,  9.0f, 1, 2, 0, false, 0, 50, "gun"     },
    { "Canon à impulsion", "CANON",      ProjType::BULLET,     1,  6.5f, 1, 1, 0, false, 0, 35, "gun"     },
    { "Missile Artemis",   "ARTEMIS",    ProjType::TORPEDO,    1, 10.0f, 1, 2, 0, true,  0, 55, "missile" },
    { "Missile Hermes",    "HERMES",     ProjType::TORPEDO,    2, 13.0f, 1, 3, 0, true,  0, 75, "missile" },
    { "Canon ionique",     "ION",        ProjType::WAVE,       1,  8.0f, 1, 0, 1, false, 0, 35, "laser"   },
    { "Laser rafale III",  "RAFALE III", ProjType::LASER,      3, 12.0f, 4, 1, 0, false, 0, 95, "laser"   },
    { "Laser perforant",   "PERFORANT",  ProjType::BIG_BULLET, 2, 11.0f, 1, 2, 0, false, 1, 80, "gun"     },
    { "Ion lourd",         "ION LOURD",  ProjType::WAVE,       2, 11.0f, 2, 0, 1, false, 0, 65, "laser"   },
    { "Missile Pégase",    "PÉGASE",     ProjType::TORPEDO,    3, 17.0f, 2, 2, 0, true,  0, 90, "missile" },
    { "Canon double",      "CANON II",   ProjType::BULLET,     2,  7.5f, 2, 1, 0, false, 0, 55, "gun"     },
};

inline const WeaponDef& GetWeaponDef(int id) {
    return WEAPON_DEFS[(id >= 0 && id < WPN_COUNT) ? id : 0];
}
