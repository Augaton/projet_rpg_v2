#include "Ship.hpp"
#include "Weapons.hpp"
#include "../core/Random.hpp"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>

// Petit tampon tournant pour les descriptions (évite une dépendance à raylib)
static const char* TextFormatSafe(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// ─── Données systèmes ─────────────────────────────────────────────────────────

namespace {

struct SystemInfo {
    const char* name;
    const char* shortName;
    const char* description;
    int         maxLevel;
    bool        usesPower;
    bool        isRoom;
    int         installCost;
    int         upgradeBase;
    int         upgradeStep;
};

constexpr SystemInfo SYSTEMS[SYS_COUNT] = {
    { "BOUCLIERS",    "SHLD", "2 barres d'énergie = 1 couche de bouclier. Bloque lasers et ions, pas les missiles.",
      8, true,  true,   0, 20,  8 },
    { "MOTEURS",      "ENGN", "Chaque barre augmente l'esquive et accélère la charge du FTL.",
      8, true,  true,   0, 10,  6 },
    { "ARMES",        "WEAP", "Alimente vos armes. Cliquez sur une arme pour l'allumer.",
      8, true,  true,   0, 15,  8 },
    { "RÉPARATION",   "REPR", "Accélère la réparation des modules en combat. Sans lui, l'équipage répare lentement.",
      3, true,  true,  35, 20, 15 },
    { "INVISIBILITÉ", "INVI", "[C] Devient invisible : +60% d'esquive et les armes ennemies cessent de charger.",
      3, true,  true,  70, 30, 15 },
    { "PIRATAGE",     "HACK", "[H] puis clic sur une salle ennemie : le système piraté est coupé un moment.",
      3, true,  true,  65, 30, 15 },
    { "ANTI-MISSILE", "DEFN", "Tourelle automatique qui abat une partie des missiles ennemis.",
      3, true,  true,  55, 25, 12 },
    { "CAPTEURS",     "SENS", "Hors combat : révèle le type des balises éloignées sur la carte. Sans énergie.",
      2, false, false, 35, 30, 20 },
};

} // namespace

static bool ValidSys(int s) { return s >= 0 && s < SYS_COUNT; }

const char* SystemName(int sys)        { return ValidSys(sys) ? SYSTEMS[sys].name : ""; }
const char* SystemShortName(int sys)   { return ValidSys(sys) ? SYSTEMS[sys].shortName : ""; }
const char* SystemDescription(int sys) { return ValidSys(sys) ? SYSTEMS[sys].description : ""; }
int         SystemMaxLevel(int sys)    { return ValidSys(sys) ? SYSTEMS[sys].maxLevel : 0; }
bool        SystemUsesPower(int sys)   { return ValidSys(sys) && SYSTEMS[sys].usesPower; }
bool        SystemIsRoom(int sys)      { return ValidSys(sys) && SYSTEMS[sys].isRoom; }
int         SystemInstallCost(int sys) { return ValidSys(sys) ? SYSTEMS[sys].installCost : 999; }
int         ReactorUpgradeCost(int reactor) { return 10 + 3 * reactor; }

int SystemUpgradeCost(int sys, int level) {
    if (!ValidSys(sys)) return 999;
    return SYSTEMS[sys].upgradeBase + SYSTEMS[sys].upgradeStep * level;
}

const char* SystemLevelEffect(int sys, int level) {
    if (level <= 0) return "Non installé";
    switch (sys) {
    case SYS_SHIELDS: return level % 2 == 0 ? TextFormatSafe("%d couche(s) de bouclier", level / 2)
                                             : TextFormatSafe("%d couche(s) (+1 barre vers la suivante)", level / 2);
    case SYS_ENGINES: {
        static const int EVASION[9] = { 0, 5, 10, 15, 20, 25, 28, 31, 35 };
        return TextFormatSafe("Esquive jusqu'à %d%%", EVASION[std::clamp(level, 0, 8)]);
    }
    case SYS_WEAPONS: return TextFormatSafe("%d énergie pour les armes", level);
    case SYS_REPAIR:  return TextFormatSafe("Réparation x%.1f en combat", (0.05f + 0.07f * level) / 0.05f);
    case SYS_CLOAK:   return TextFormatSafe("Invisible %.0fs, recharge 22s", 2.0f + 3.0f * level);
    case SYS_HACKING: return TextFormatSafe("Piratage %.0fs, recharge 18s", 1.0f + 3.0f * level);
    case SYS_DEFENSE: return TextFormatSafe("Intercepte %d%% des missiles", level == 1 ? 35 : level == 2 ? 60 : 80);
    case SYS_SENSORS: return level >= 2 ? "Révèle tout le secteur" : "Révèle les balises à 2 sauts";
    default:          return "";
    }
}

static const char* TextFormatSafe(const char* fmt, ...) {
    static char buffers[4][96];
    static int  next = 0;
    char* buf = buffers[next];
    next = (next + 1) % 4;
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof(buffers[0]), fmt, args);
    va_end(args);
    return buf;
}

// ─── Énergie ──────────────────────────────────────────────────────────────────

int Ship::PowerInUse() const {
    int total = 0;
    for (int s = 0; s < SYS_COUNT; s++) total += sys[s].power;
    return total;
}

int Ship::_DesiredOf(int s) const {
    if (s != SYS_WEAPONS) return sys[s].desired;
    int total = 0;
    for (const auto& w : weapons)
        if (!w.Empty() && w.wanted) total += GetWeaponDef(w.id).power;
    return total;
}

int Ship::PowerReserved() const {
    int total = 0;
    for (int s = 0; s < SYS_COUNT; s++) total += _DesiredOf(s);
    return total;
}

// Récupère de l'énergie "réservée" par des systèmes endommagés
// (demandée mais non allouée) pour pouvoir la donner ailleurs.
bool Ship::_Reserve(int amount) {
    int avail = reactor - PowerReserved();
    for (int s = 0; s < SYS_COUNT && avail < amount; s++) {
        if (s == SYS_WEAPONS) {
            for (int i = MAX_WEAPON_SLOTS - 1; i >= 0 && avail < amount; i--) {
                WeaponSlot& w = weapons[i];
                if (!w.Empty() && w.wanted && !w.powered) {
                    w.wanted = false;
                    avail += GetWeaponDef(w.id).power;
                }
            }
        } else {
            while (sys[s].desired > sys[s].power && avail < amount) {
                sys[s].desired--;
                avail++;
            }
        }
    }
    return avail >= amount;
}

bool Ship::AddPower(int s) {
    if (s == SYS_WEAPONS) {
        for (int i = 0; i < MAX_WEAPON_SLOTS; i++)
            if (!weapons[i].Empty() && !weapons[i].wanted && PowerWeapon(i)) return true;
        return false;
    }

    if (!SystemUsesPower(s)) return false;
    ShipSystem& x = sys[s];
    if (!x.Installed() || x.desired >= x.Capacity()) return false;
    if (!_Reserve(1)) return false;
    x.desired++;
    RecomputePower();
    return true;
}

bool Ship::RemovePower(int s) {
    if (s == SYS_WEAPONS) {
        for (int i = MAX_WEAPON_SLOTS - 1; i >= 0; i--) {
            if (!weapons[i].Empty() && weapons[i].wanted) {
                DepowerWeapon(i);
                return true;
            }
        }
        return false;
    }

    ShipSystem& x = sys[s];
    if (x.desired <= 0) return false;
    x.desired = std::max(0, std::min(x.desired, x.power) - 1);
    RecomputePower();
    return true;
}

bool Ship::PowerWeapon(int slot) {
    if (slot < 0 || slot >= MAX_WEAPON_SLOTS) return false;
    WeaponSlot& w = weapons[slot];
    if (w.Empty() || w.wanted) return false;

    int req  = GetWeaponDef(w.id).power;
    int used = sys[SYS_WEAPONS].power;
    if (used + req > sys[SYS_WEAPONS].Capacity()) return false;
    if (!_Reserve(req)) return false;

    w.wanted = true;
    RecomputePower();
    if (!w.powered) {
        w.wanted = false;
        RecomputePower();
        return false;
    }
    return true;
}

bool Ship::SwapWeapons(int a, int b) {
    int n = std::min(weaponSlots, MAX_WEAPON_SLOTS);
    if (a < 0 || b < 0 || a >= n || b >= n || a == b) return false;
    if (weapons[a].Empty() && weapons[b].Empty()) return false;
    // Simple permutation : l'énergie totale des armes ne change pas
    std::swap(weapons[a], weapons[b]);
    return true;
}

void Ship::DepowerWeapon(int slot) {
    if (slot < 0 || slot >= MAX_WEAPON_SLOTS) return;
    weapons[slot].wanted       = false;
    weapons[slot].powered      = false;
    weapons[slot].pendingShots = 0;   // une arme éteinte n'achève pas sa salve
    RecomputePower();
}

void Ship::RecomputePower() {
    for (int s = 0; s < SYS_COUNT; s++) {
        if (s == SYS_WEAPONS) continue;
        ShipSystem& x = sys[s];
        if (!x.Installed() || !SystemUsesPower(s)) { x.desired = 0; x.power = 0; continue; }
        x.power = std::min(x.desired, x.Capacity());
    }

    // Armes : d'abord celles déjà alimentées, puis les autres dans l'ordre
    int cap  = sys[SYS_WEAPONS].Capacity();
    int used = 0;
    bool kept[MAX_WEAPON_SLOTS] = {};

    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        WeaponSlot& w = weapons[i];
        if (w.Empty() || !w.wanted) { w.powered = false; continue; }
        if (!w.powered) continue;
        int req = GetWeaponDef(w.id).power;
        if (used + req <= cap) { used += req; kept[i] = true; }
        else                   { w.powered = false; }
    }
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        WeaponSlot& w = weapons[i];
        if (w.Empty() || !w.wanted || kept[i]) continue;
        int req = GetWeaponDef(w.id).power;
        if (used + req <= cap) { used += req; w.powered = true; }
        else                   { w.powered = false; }
    }
    sys[SYS_WEAPONS].power = used;
}

void Ship::AutoPower() {
    for (int s = 0; s < SYS_COUNT; s++) sys[s].desired = 0;
    for (auto& w : weapons) { w.wanted = false; w.powered = false; }
    RecomputePower();

    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) PowerWeapon(i);

    int shieldTarget = sys[SYS_SHIELDS].Capacity() & ~1;  // nombre pair
    while (sys[SYS_SHIELDS].desired < shieldTarget && AddPower(SYS_SHIELDS)) {}
    while (AddPower(SYS_DEFENSE)) {}
    while (AddPower(SYS_ENGINES)) {}
    while (AddPower(SYS_REPAIR)) {}
}

int Ship::Evasion() const {
    static const int EVASION[9] = { 0, 5, 10, 15, 20, 25, 28, 31, 35 };
    int p = std::clamp(sys[SYS_ENGINES].power, 0, 8);
    return std::min(95, EVASION[p] + (Cloaked() ? 60 : 0) + (HasAug(AUG_THRUSTERS) ? 5 : 0));
}

// En présence d'un ennemi, le FTL se recharge lentement :
// ~40 s avec 1 barre de moteurs, ~31 s avec 2, ~25 s avec 3.
float Ship::FtlChargeRate() const {
    int p = sys[SYS_ENGINES].power;
    if (p <= 0) return 0.0f;
    return (1.0f + 0.3f * (float)(p - 1)) / 40.0f * (HasAug(AUG_FTL_BOOSTER) ? 1.25f : 1.0f);
}

float Ship::RepairRate() const {
    // Équipage : ~20 s par barre. Module de réparation : +0.07 barre/s par niveau alimenté.
    return (0.05f + 0.07f * (float)sys[SYS_REPAIR].power) * (HasAug(AUG_NANOBOTS) ? 1.5f : 1.0f);
}

bool Ship::ActivateCloak() {
    if (!CanCloak()) return false;
    cloakTimer = CloakDuration();
    return true;
}

float Ship::DefenseChance() const {
    static const float CHANCE[4] = { 0.0f, 0.35f, 0.60f, 0.80f };
    return CHANCE[std::clamp(sys[SYS_DEFENSE].power, 0, 3)];
}

// ─── Simulation ───────────────────────────────────────────────────────────────

void Ship::Update(float dt, bool inCombat) {
    hitFlash = std::max(0.0f, hitFlash - dt * 3.0f);

    for (auto& x : sys) {
        if (x.ionTimer  > 0.0f) x.ionTimer  = std::max(0.0f, x.ionTimer  - dt);
        if (x.hackTimer > 0.0f) x.hackTimer = std::max(0.0f, x.hackTimer - dt);
    }

    // Capacités actives
    if (cloakTimer > 0.0f) {
        cloakTimer -= dt;
        if (cloakTimer <= 0.0f) { cloakTimer = 0.0f; cloakCooldown = 22.0f; }
    } else {
        cloakCooldown = std::max(0.0f, cloakCooldown - dt);
    }
    hackCooldown    = std::max(0.0f, hackCooldown - dt);
    defenseCooldown = std::max(0.0f, defenseCooldown - dt);

    RecomputePower();

    // Boucliers
    int maxLayers = MaxShieldLayers();
    if (shieldLayers > maxLayers) shieldLayers = maxLayers;
    if (shieldLayers < maxLayers) {
        shieldCharge += dt / 2.0f * shieldRegenMult * (HasAug(AUG_SHIELD_CAP) ? 1.35f : 1.0f);
        if (shieldCharge >= 1.0f) {
            shieldLayers++;
            shieldCharge = 0.0f;
        }
    } else {
        shieldCharge = 0.0f;
    }

    // Réparation automatique du système le plus endommagé
    // L'équipage répare toujours un peu (même système de réparation détruit) :
    // aucun système ne reste hors service indéfiniment.
    if (inCombat) {
        // Priorité : pouvoir riposter, puis se protéger, puis esquiver
        static constexpr int PRIORITY[] = { SYS_WEAPONS, SYS_SHIELDS, SYS_ENGINES, SYS_REPAIR,
                                            SYS_DEFENSE, SYS_CLOAK, SYS_HACKING, SYS_SENSORS };
        int worst = -1;
        for (int s : PRIORITY)
            if (sys[s].damage > 0) { worst = s; break; }

        if (worst >= 0) {
            ShipSystem& x = sys[worst];
            x.repairProgress += dt * RepairRate();
            if (x.repairProgress >= 1.0f) {
                x.repairProgress = 0.0f;
                x.damage--;
            }
        }
    }

    // Armes
    for (auto& w : weapons) {
        if (w.Empty()) continue;
        const WeaponDef& def = GetWeaponDef(w.id);
        if (!inCombat)       w.charge = 0.0f;
        else if (w.powered)  { if (!chargeFrozen) w.charge = std::min(1.0f, w.charge + dt * chargeMult / def.charge); }
        else                 w.charge = std::max(0.0f, w.charge - dt * 0.5f);
        w.fireAnim = std::max(0.0f, w.fireAnim - dt);
    }

    if (inCombat) ftlCharge = std::min(1.0f, ftlCharge + dt * FtlChargeRate());
}

int Ship::ApplyHit(int targetSys, int damage, int ion) {
    int hullDamage = damage;
    if (hullDamage > 0 && HasAug(AUG_REACTIVE_ARMOR) && rng::Chance(0.15f)) hullDamage--;
    if (hullDamage > 0) {
        hull     = std::max(0, hull - hullDamage);
        hitFlash = 1.0f;
    }

    if (targetSys >= 0 && targetSys < SYS_COUNT && sys[targetSys].Installed()) {
        ShipSystem& x = sys[targetSys];
        x.damage = std::min(x.level, x.damage + damage);
        x.repairProgress = 0.0f;
        if (ion > 0) x.ionTimer = std::max(x.ionTimer, 7.0f * (float)ion);
    }
    RecomputePower();
    return hullDamage;
}

void Ship::RepairSystems() {
    for (auto& x : sys) {
        x.damage = 0;
        x.ionTimer = 0.0f;
        x.hackTimer = 0.0f;
        x.repairProgress = 0.0f;
    }
    RecomputePower();
}

void Ship::PrepareForCombat() {
    RecomputePower();
    for (auto& w : weapons) {
        w.charge       = HasAug(AUG_PRELOADER) ? 0.4f : 0.0f;
        w.target       = -1;
        w.pendingShots = 0;
        w.burstTimer   = 0.0f;
    }
    shieldLayers = MaxShieldLayers();
    shieldCharge = 0.0f;
    ftlCharge    = 0.0f;
    cloakTimer   = 0.0f;
    cloakCooldown = 0.0f;
    hackCooldown  = 0.0f;
    defenseCooldown = 0.0f;
    chargeFrozen  = false;
    shieldRegenMult = 1.0f;
}

// ─── Équipement ───────────────────────────────────────────────────────────────

bool Ship::HasAug(int id) const {
    for (int a : augments) if (a == id) return true;
    return false;
}

bool Ship::AddAugment(int id) {
    if (id < 0 || id >= AUG_COUNT || HasAug(id)) return false;
    for (int& a : augments)
        if (a < 0) { a = id; return true; }
    return false;
}

int Ship::AugmentCount() const {
    int n = 0;
    for (int a : augments) n += a >= 0;
    return n;
}

bool Ship::AddWeapon(int id) {
    for (int i = 0; i < weaponSlots; i++) {
        if (weapons[i].Empty()) {
            weapons[i] = WeaponSlot{};
            weapons[i].id = id;
            return true;
        }
    }
    return false;
}

void Ship::RemoveWeapon(int slot) {
    if (slot < 0 || slot >= MAX_WEAPON_SLOTS) return;
    weapons[slot] = WeaponSlot{};
    RecomputePower();
}

int Ship::WeaponCount() const {
    int n = 0;
    for (const auto& w : weapons) if (!w.Empty()) n++;
    return n;
}

void Ship::AutoPowerNew() {
    for (int s = 0; s < SYS_COUNT; s++) {
        if (!SystemUsesPower(s)) continue;
        if (s == SYS_SHIELDS) {
            // Les boucliers ne servent que par paires de barres
            while (FreePower() >= 2 && sys[s].desired + 2 <= sys[s].Capacity()) {
                AddPower(s);
                AddPower(s);
            }
            continue;
        }
        while (FreePower() > 0 && AddPower(s)) {}
    }
}

bool Ship::UpgradeSystem(int s) {
    if (s < 0 || s >= SYS_COUNT || sys[s].level >= SystemMaxLevel(s)) return false;
    sys[s].level++;
    RecomputePower();
    return true;
}
