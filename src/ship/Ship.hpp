#pragma once
#include "Augments.hpp"
#include <string>

// ─── Systèmes ─────────────────────────────────────────────────────────────────

enum SysType {
    SYS_SHIELDS = 0,
    SYS_ENGINES,
    SYS_WEAPONS,
    SYS_REPAIR,
    SYS_CLOAK,      // invisibilité (capacité active)
    SYS_HACKING,    // piratage d'un système ennemi (capacité active)
    SYS_DEFENSE,    // défense anti-missile (passive)
    SYS_SENSORS,    // capteurs longue portée (hors combat, sans énergie)
    SYS_COUNT
};

// Nombre de systèmes des sauvegardes version 1
constexpr int SYS_COUNT_V1 = 4;

const char* SystemName(int sys);
const char* SystemShortName(int sys);    // 4 lettres, pour le HUD
const char* SystemDescription(int sys);
const char* SystemLevelEffect(int sys, int level);   // effet concret au niveau donné
int         SystemMaxLevel(int sys);
bool        SystemUsesPower(int sys);    // consomme de l'énergie du réacteur
bool        SystemIsRoom(int sys);       // salle ciblable en combat
int         SystemInstallCost(int sys);  // achat du module en magasin
int         SystemUpgradeCost(int sys, int level);
int         ReactorUpgradeCost(int reactor);

struct ShipSystem {
    int   level    = 0;      // barres installées (0 = absent)
    int   desired  = 0;      // énergie demandée (sauf armes, gérées par emplacement)
    int   power    = 0;      // énergie réellement allouée
    int   damage   = 0;      // barres endommagées
    float ionTimer = 0.0f;   // > 0 : une barre bloquée par l'ion
    float hackTimer = 0.0f;  // > 0 : système piraté, hors service
    float repairProgress = 0.0f;

    bool Installed() const { return level > 0; }
    int  Capacity()  const {
        if (hackTimer > 0.0f) return 0;
        int c = level - damage - (ionTimer > 0.0f ? 1 : 0);
        return c < 0 ? 0 : c;
    }
};

// ─── Armes ────────────────────────────────────────────────────────────────────

constexpr int MAX_WEAPON_SLOTS = 4;

struct WeaponSlot {
    int   id           = -1;
    bool  wanted       = false;  // le joueur veut l'alimenter
    bool  powered      = false;  // réellement alimentée
    float charge       = 0.0f;   // 0..1
    int   target       = -1;     // système visé chez l'adversaire
    int   preferred    = -1;     // dernière cible choisie, réappliquée au combat suivant
    int   pendingShots = 0;      // projectiles restants dans la salve
    float burstTimer   = 0.0f;
    float fireAnim     = 0.0f;

    bool Empty() const { return id < 0; }
};

// ─── Vaisseau ─────────────────────────────────────────────────────────────────

struct Ship {
    std::string name;
    int hullType = 0;

    int hull    = 30;
    int maxHull = 30;
    int reactor = 8;

    ShipSystem sys[SYS_COUNT];
    WeaponSlot weapons[MAX_WEAPON_SLOTS];
    int        weaponSlots = MAX_WEAPON_SLOTS;

    // Identité
    int   shipType  = 0;       // vaisseau jouable (joueur)
    int   trait     = -1;      // trait d'élite (ennemi)

    // Augmentations passives
    int   augments[MAX_AUGMENTS] = { -1, -1, -1 };

    // Modificateurs de combat (difficulté, traits, dangers)
    float chargeMult      = 1.0f;   // vitesse de charge des armes
    float shieldRegenMult = 1.0f;   // vitesse de recharge des couches

    int   shieldLayers = 0;
    float shieldCharge = 0.0f;   // progression de la prochaine couche
    float ftlCharge    = 0.0f;   // 0..1

    // Ressources (joueur)
    int fuel     = 0;
    int missiles = 0;
    int scrap    = 0;

    float hitFlash = 0.0f;

    // Capacités actives
    float cloakTimer    = 0.0f;   // > 0 : invisible
    float cloakCooldown = 0.0f;
    float hackCooldown  = 0.0f;
    float defenseCooldown = 0.0f;
    bool  chargeFrozen  = false;  // armes gelées (adversaire invisible)

    bool  Destroyed() const { return hull <= 0; }

    // Énergie
    int   PowerInUse() const;
    int   PowerReserved() const;
    int   FreePower() const { return reactor - PowerInUse(); }
    int   MaxShieldLayers() const { return sys[SYS_SHIELDS].power / 2; }
    int   Evasion() const;
    float FtlChargeRate() const;

    bool  AddPower(int s);
    bool  RemovePower(int s);
    bool  PowerWeapon(int slot);
    void  DepowerWeapon(int slot);
    // Échange deux emplacements d'armes (charge, énergie et cible suivent l'arme)
    bool  SwapWeapons(int a, int b);
    void  RecomputePower();
    void  AutoPower();                 // répartition automatique (IA)

    // Simulation
    void  Update(float dt, bool inCombat);
    // Renvoie les dégâts de coque réellement subis (blindage réactif possible)
    int   ApplyHit(int targetSys, int damage, int ion);
    void  RepairSystems();
    float RepairRate() const;                   // barres réparées par seconde en combat

    bool  Cloaked() const        { return cloakTimer > 0.0f; }
    bool  CanCloak() const       { return sys[SYS_CLOAK].power > 0 && cloakTimer <= 0.0f && cloakCooldown <= 0.0f; }
    float CloakDuration() const  { return 2.0f + 3.0f * (float)sys[SYS_CLOAK].power; }     // 5 / 8 / 11 s
    bool  ActivateCloak();
    bool  CanHack() const        { return sys[SYS_HACKING].power > 0 && hackCooldown <= 0.0f; }
    float HackDuration() const   { return 1.0f + 3.0f * (float)sys[SYS_HACKING].power; }  // 4 / 7 / 10 s
    float DefenseChance() const;                // probabilité d'intercepter un missile
    void  PrepareForCombat();

    // Équipement
    bool  HasAug(int id) const;
    bool  AddAugment(int id);
    int   AugmentCount() const;
    bool  AddWeapon(int id);
    void  AutoPowerNew();                       // alimente ce qui peut l'être avec l'énergie libre
    void  RemoveWeapon(int slot);
    int   WeaponCount() const;
    bool  UpgradeSystem(int s);

private:
    int   _DesiredOf(int s) const;
    bool  _Reserve(int amount);
};
