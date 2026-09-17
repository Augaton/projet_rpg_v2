#pragma once

// Augmentations : améliorations passives, 3 emplacements par vaisseau
enum AugId {
    AUG_SCRAP_ARM = 0,   // +15 % de ferraille en combat
    AUG_REACTIVE_ARMOR,  // 15 % de chance d'ignorer 1 dégât de coque
    AUG_PRELOADER,       // armes à 40 % de charge au début du combat
    AUG_SHIELD_CAP,      // couches de bouclier rechargées 35 % plus vite
    AUG_FTL_BOOSTER,     // charge FTL +25 % en combat
    AUG_RECYCLER,        // 25 % de chance de ne pas consommer un missile
    AUG_NANOBOTS,        // réparation des modules +50 %
    AUG_THRUSTERS,       // +5 % d'esquive
    AUG_COUNT
};

constexpr int MAX_AUGMENTS = 3;

struct AugDef {
    const char* name;
    const char* shortName;   // 4 lettres pour le HUD
    const char* description;
    int         cost;
};

inline constexpr AugDef AUG_DEFS[AUG_COUNT] = {
    { "Bras de récupération", "RECU", "+15 % de ferraille récupérée après chaque combat.",                 50 },
    { "Blindage réactif",     "BLIN", "15 % de chance d'ignorer 1 dégât de coque à chaque impact.",        70 },
    { "Préchargeur d'armes",  "PREC", "Les armes commencent chaque combat chargées à 40 %.",               60 },
    { "Condensateur",         "COND", "Les couches de bouclier se rechargent 35 % plus vite.",             65 },
    { "Booster FTL",          "BFTL", "Le propulseur FTL se recharge 25 % plus vite en combat.",           45 },
    { "Recycleur de missiles","RECY", "25 % de chance qu'un missile tiré ne soit pas consommé.",           55 },
    { "Nanorobots",           "NANO", "La réparation des modules en combat est 50 % plus rapide.",         55 },
    { "Propulseurs latéraux", "PROP", "+5 % d'esquive permanente.",                                        50 },
};

inline const AugDef& GetAugDef(int id) { return AUG_DEFS[(id >= 0 && id < AUG_COUNT) ? id : 0]; }
