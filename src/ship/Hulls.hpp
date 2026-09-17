#pragma once

enum HullType {
    HULL_FRIGATE = 0,
    HULL_SCOUT,
    HULL_FIGHTER,
    HULL_BOMBER,
    HULL_TORPEDO,
    HULL_SUPPORT,
    HULL_BATTLECRUISER,
    HULL_DREADNOUGHT,
    HULL_COUNT
};

// Chemins des sprites Kla'ed pour chaque coque (nullptr = pas d'asset)
struct HullDef {
    const char* base;
    const char* engine;
    const char* shield;
    const char* weapons;
    const char* destruction;
    float       scale;
};

inline constexpr HullDef HULL_DEFS[HULL_COUNT] = {
    { "asset/Base/PNGs/frigate.png", "asset/Engine/PNGs/frigate.png", "asset/Shield/PNGs/frigate.png",
      "asset/Weapons/PNGs/Kla'ed - Frigate - Weapons.png", "asset/Destruction/PNGs/Kla'ed - Frigate - Destruction.png", 3.0f },
    { "asset/Base/PNGs/scout.png", "asset/Engine/PNGs/scout.png", "asset/Shield/PNGs/scout.png",
      "asset/Weapons/PNGs/Kla'ed - Scout - Weapons.png", "asset/Destruction/PNGs/Kla'ed - Scout - Destruction.png", 3.0f },
    { "asset/Base/PNGs/fighter.png", "asset/Engine/PNGs/fighter.png", "asset/Shield/PNGs/fighter.png",
      "asset/Weapons/PNGs/Kla'ed - Fighter - Weapons.png", "asset/Destruction/PNGs/Kla'ed - Fighter - Destruction.png", 3.0f },
    { "asset/Base/PNGs/bomber.png", "asset/Engine/PNGs/bomber.png", "asset/Shield/PNGs/bomber.png",
      nullptr, "asset/Destruction/PNGs/Kla'ed - Bomber - Destruction.png", 3.0f },
    { "asset/Base/PNGs/torpedo_ship.png", "asset/Engine/PNGs/torpedo_ship.png", "asset/Shield/PNGs/torpedo_ship.png",
      "asset/Weapons/PNGs/Kla'ed - Torpedo Ship - Weapons.png", "asset/Destruction/PNGs/Kla'ed - Torpedo Ship - Destruction.png", 3.0f },
    { "asset/Base/PNGs/support_ship.png", "asset/Engine/PNGs/support_ship.png", nullptr,
      nullptr, "asset/Destruction/PNGs/Kla'ed - Support ship - Destruction.png", 3.0f },
    { "asset/Base/PNGs/battlecruiser.png", "asset/Engine/PNGs/battlecruiser.png", "asset/Shield/PNGs/battlecruiser.png",
      "asset/Weapons/PNGs/Kla'ed - Battlecruiser - Weapons.png", "asset/Destruction/PNGs/Kla'ed - Battlecruiser - Destruction.png", 1.9f },
    { "asset/Base/PNGs/dreadnought.png", "asset/Engine/PNGs/dreadnought.png", "asset/Shield/PNGs/dreadnought.png",
      "asset/Weapons/PNGs/Kla'ed - Dreadnought - Weapons.png", "asset/Destruction/PNGs/Kla'ed - Dreadnought - Destruction.png", 2.1f },
};
