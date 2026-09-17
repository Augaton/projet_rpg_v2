#include "Store.hpp"
#include "../audio/AudioManager.hpp"
#include "../core/UI.hpp"
#include "../hud/HUD.hpp"
#include "../ship/Weapons.hpp"
#include <algorithm>
#include <random>
#include <string>

using namespace ui;

static constexpr Rectangle PANEL = { 130, 56, 1020, 608 };

static constexpr int FUEL_COST    = 3;
static constexpr int MISSILE_COST = 4;
static constexpr int REPAIR_COST  = 2;

void Store::Generate(int sector, int seed) {
    if (seed == _seed && !_offers.empty()) return;
    _seed   = seed;
    _sector = sector;

    std::mt19937 gen((unsigned)seed * 7919u + 17u);
    std::vector<int> pool;
    for (int i = 0; i < WPN_COUNT; i++) pool.push_back(i);
    std::shuffle(pool.begin(), pool.end(), gen);

    _offers.clear();
    for (int i = 0; i < 3; i++) _offers.push_back({ pool[i], false });

    // Modules proposés : 3 parmi les modules optionnels
    std::vector<int> mods = { SYS_REPAIR, SYS_CLOAK, SYS_HACKING, SYS_DEFENSE, SYS_SENSORS };
    std::shuffle(mods.begin(), mods.end(), gen);
    _modules.assign(mods.begin(), mods.begin() + 3);
    _augment     = (int)(gen() % AUG_COUNT);
    _augmentSold = false;
}

// Ligne d'article : titre, sous-titre et bouton d'achat
static bool Row(float x, float y, float w, const char* title, const char* sub,
                const char* btnLabel, bool enabled, Color accent) {
    DrawRectangleRec({ x, y, w, 50 }, { 16, 16, 18, 240 });
    DrawRectangleRec({ x, y, 4, 50 }, enabled ? accent : C_BORDER);
    Label(title, x + 14, y + 8, 20, enabled ? C_TEXT : C_LABEL);
    if (sub) Text(sub, x + 14, y + 30, 13, C_TEXT_DIM);
    float size = MeasureLabel(btnLabel, 20) > 78 ? 10 : 20;   // libellés longs (INSTALLÉ…)
    return Button({ x + w - 96, y + 10, 86, 30 }, btnLabel, enabled, accent, size);
}

void Store::Draw(Ship& s, HUD& hud, AudioManager& audio) {
    if (!_open) return;

    Dim(0.6f);
    Panel(PANEL, C_GOOD, 0.3f, C_PANEL_BG, "COMMERCE // AVANT-POSTE");

    Label("AVANT-POSTE MARCHAND", PANEL.x + 28, PANEL.y + 26, 30, C_TEXT);
    Text("Paiement en ferraille. Maj + clic : acheter par 5.", PANEL.x + 30, PANEL.y + 64, 15, C_TEXT_DIM);
    LabelRight(TextFormat("FERRAILLE %d", s.scrap), PANEL.x + PANEL.width - 28, PANEL.y + 28, 30, C_SCRAP);
    DrawRectangleRec({ PANEL.x + 24, PANEL.y + 90, PANEL.width - 48, 2 }, C_BORDER);

    bool bulk = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    auto buy = [&](int cost, const std::string& what) {
        s.scrap = std::max(0, s.scrap - cost);
        hud.Log("Achat : " + what, C_GOOD);
        audio.PlaySfx("gun_1", 1.6f, 0.35f);
    };
    // Quantité achetable (1 ou 5 avec Maj), limitée par la ferraille et un plafond
    auto qty = [&](int unitCost, int cap) {
        int n = bulk ? 5 : 1;
        n = std::min(n, cap);
        n = std::min(n, unitCost > 0 ? s.scrap / unitCost : n);
        return std::max(n, 0);
    };

    const float colW = 310.0f;
    const float top  = PANEL.y + 110;
    float x0 = PANEL.x + 24, x1 = x0 + colW + 20, x2 = x1 + colW + 20;

    // ── Ressources ───────────────────────────────────────────────────────────
    Label("RESSOURCES", x0, top, 10, C_ACCENT);
    float y = top + 20;

    int nf = qty(FUEL_COST, 999 - s.fuel);
    if (Row(x0, y, colW, bulk ? "CARBURANT x5" : "CARBURANT", TextFormat("En soute : %d", s.fuel),
            TextFormat("%d", std::max(1, nf) * FUEL_COST), nf > 0, C_FUEL)) {
        buy(nf * FUEL_COST, TextFormat("%d carburant", nf)); s.fuel += nf;
    }
    y += 58;
    int nm = qty(MISSILE_COST, 999 - s.missiles);
    if (Row(x0, y, colW, bulk ? "MISSILE x5" : "MISSILE", TextFormat("En soute : %d", s.missiles),
            TextFormat("%d", std::max(1, nm) * MISSILE_COST), nm > 0, C_MISSILE)) {
        buy(nm * MISSILE_COST, TextFormat("%d missile(s)", nm)); s.missiles += nm;
    }
    y += 58;
    int missing = std::max(0, s.maxHull - s.hull);
    int nr = qty(REPAIR_COST, missing);
    if (Row(x0, y, colW, bulk ? "REPARER x5" : "REPARER 1", TextFormat("Coque %d / %d", s.hull, s.maxHull),
            TextFormat("%d", std::max(1, nr) * REPAIR_COST), nr > 0, C_HULL_FULL)) {
        buy(nr * REPAIR_COST, "réparation"); s.hull += nr;
    }
    y += 58;
    int fullCost = missing * REPAIR_COST;
    if (Row(x0, y, colW, "REPAR. TOTALE", TextFormat("+%d coque", missing),
            TextFormat("%d", fullCost), missing > 0 && s.scrap >= fullCost, C_HULL_FULL)) {
        buy(fullCost, "réparation complète"); s.hull = s.maxHull;
    }

    // Augmentation
    y += 76;
    Label("AUGMENTATION", x0, y - 18, 10, C_ACCENT);
    if (_augment >= 0) {
        const AugDef& a = GetAugDef(_augment);
        bool owned = s.HasAug(_augment);
        bool full  = s.AugmentCount() >= MAX_AUGMENTS;
        std::string label = _augmentSold || owned ? "ACQUIS" : full ? "PLEIN" : TextFormat("%d", a.cost);
        if (Row(x0, y, colW, a.name, TextFormat("%d/%d emplacements", s.AugmentCount(), MAX_AUGMENTS), label.c_str(),
                !_augmentSold && !owned && !full && s.scrap >= a.cost, C_SCRAP)) {
            buy(a.cost, a.name);
            s.AddAugment(_augment);
            _augmentSold = true;
        }
        TextWrapped(a.description, { x0 + 14, y + 54, colW - 20, 0 }, 12, C_TEXT_DIM);
    }

    // ── Armes ────────────────────────────────────────────────────────────────
    Label("ARMES", x1, top, 10, C_ACCENT);
    y = top + 20;
    bool freeSlot = false;
    for (int i = 0; i < s.weaponSlots && i < MAX_WEAPON_SLOTS; i++) freeSlot |= s.weapons[i].Empty();

    for (auto& o : _offers) {
        const WeaponDef& d = GetWeaponDef(o.weaponId);
        std::string sub = TextFormat("Énergie %d · %ds · ", d.power, (int)d.charge);
        sub += d.ion > 0 ? "ionique" : TextFormat("%d x%d%s", d.damage, d.shots, d.missile ? " missile" : "");
        std::string label = o.sold ? "VENDU" : TextFormat("%d", d.cost);
        if (Row(x1, y, colW, d.shortName, sub.c_str(), label.c_str(), !o.sold && freeSlot && s.scrap >= d.cost, C_ENEMY)) {
            buy(d.cost, d.name);
            s.AddWeapon(o.weaponId);
            s.AutoPowerNew();
            o.sold = true;
        }
        y += 58;
    }
    if (!freeSlot) Text("Soutes pleines : revendez une arme.", x1, y, 13, C_WARNING);

    y += 24;
    Label("REVENDRE", x1, y, 10, C_ACCENT);
    y += 18;
    for (int i = 0; i < s.weaponSlots && i < MAX_WEAPON_SLOTS; i++) {
        if (s.weapons[i].Empty()) continue;
        const WeaponDef& d = GetWeaponDef(s.weapons[i].id);
        int value = d.cost / 2;
        DrawRectangleRec({ x1, y, colW, 32 }, { 16, 16, 18, 230 });
        Label(d.shortName, x1 + 14, y + 7, 20, C_LABEL);
        if (Button({ x1 + colW - 96, y + 3, 86, 26 }, TextFormat("+%d", value), s.WeaponCount() > 1, C_SCRAP, 20)) {
            s.scrap = std::min(s.scrap + value, 99999);
            hud.Log(std::string("Revendu : ") + d.name, C_SCRAP);
            s.RemoveWeapon(i);
        }
        y += 38;
    }

    // ── Modules ──────────────────────────────────────────────────────────────
    Label("MODULES", x2, top, 10, C_ACCENT);
    y = top + 20;

    for (int sys : _modules) {
        bool owned = s.sys[sys].Installed();
        int  cost  = SystemInstallCost(sys);
        std::string label = owned ? "INSTALLÉ" : TextFormat("%d", cost);
        if (Row(x2, y, colW, SystemName(sys), SystemLevelEffect(sys, 1), label.c_str(),
                !owned && s.scrap >= cost, C_ION)) {
            buy(cost, SystemName(sys));
            s.sys[sys].level = 1;
            s.RecomputePower();
            s.AutoPowerNew();
        }
        TextWrapped(SystemDescription(sys), { x2 + 14, y + 52, colW - 20, 0 }, 12, C_TEXT_DIM);
        y += 58 + TextWrapped(SystemDescription(sys), { 0, 0, colW - 20, 0 }, 12, WHITE, false, false) + 8;
    }

    Text("Améliorer vos modules : menu VAISSEAU [U]", x2, y + 4, 13, C_ACCENT);

    if (Button({ PANEL.x + PANEL.width - 210, PANEL.y + PANEL.height - 58, 180, 40 }, "QUITTER", true, C_ACCENT, 20))
        _closeRequested = true;
}
