#include "Shop.hpp"
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <algorithm>
#include <string>

// ─── Helpers locaux ───────────────────────────────────────────────────────────

static float  rf()              { return (float)(rand() % 1000) / 1000.0f; }
static int    ri(int lo, int hi){ return lo + rand() % (hi - lo + 1); }

static Rarity RollRarity(int zoneLevel) {
    int roll = ri(0, 99) + zoneLevel * 8;
    if (roll >= 95) return Rarity::LEGENDARY;
    if (roll >= 80) return Rarity::EPIC;
    if (roll >= 60) return Rarity::RARE;
    if (roll >= 35) return Rarity::UNCOMMON;
    return Rarity::COMMON;
}

static int RarityMult(Rarity r) {
    switch (r) {
        case Rarity::COMMON:    return 1;
        case Rarity::UNCOMMON:  return 2;
        case Rarity::RARE:      return 4;
        case Rarity::EPIC:      return 8;
        case Rarity::LEGENDARY: return 16;
    }
    return 1;
}

// ─── Catalogue d'armes ────────────────────────────────────────────────────────

struct WeaponTemplate {
    const char* name;
    ProjType    projType;
    float       baseDmg;
    float       baseCd;
    float       baseSpeed;
    const char* sfxKey;
    int         basePrice;
};

static const WeaponTemplate WEAPON_CATALOG[] = {
    { "Light Cannon",     ProjType::BULLET,     8.0f,  0.20f, 1.0f, "laser",   60  },
    { "Heavy Cannon",     ProjType::BIG_BULLET, 18.0f, 0.40f, 0.9f, "gun_1",   120 },
    { "Pulse Laser",      ProjType::LASER,      12.0f, 0.15f, 1.2f, "laser",   150 },
    { "Burst Laser",      ProjType::LASER,       9.0f, 0.10f, 1.3f, "laser",   140 },
    { "Torpedo Launcher", ProjType::TORPEDO,    40.0f, 1.00f, 0.8f, "missile", 280 },
    { "Wave Cannon",      ProjType::WAVE,       20.0f, 0.60f, 0.9f, "laser",   200 },
};
static constexpr int WEAPON_COUNT = 6;

// ─── Factories ────────────────────────────────────────────────────────────────

static Item MakeWeapon(const WeaponTemplate& tmpl, Rarity rar, int rank) {
    int   mult = RarityMult(rar);
    float rm   = 1.0f + (rank - 1) * 0.25f;   // rank 1→×1.0, 5→×2.0

    Item it;
    it.name      = std::string(tmpl.name)
                   + " Mk" + std::to_string(rank)
                   + " [" + Item::RarityName(rar) + "]";
    it.category  = ItemCategory::WEAPON;
    it.rarity    = rar;
    it.rank      = rank;
    it.buyPrice  = (int)(tmpl.basePrice * mult * rm);
    it.sellPrice = it.buyPrice / 2;

    it.weapon.projType      = tmpl.projType;
    it.weapon.damage        = tmpl.baseDmg * rm * (0.9f + rf() * 0.2f);
    it.weapon.cooldown      = std::max(0.08f, tmpl.baseCd / rm);
    it.weapon.projSpeedMult = tmpl.baseSpeed;
    it.weapon.sfxKey        = tmpl.sfxKey;
    return it;
}

static Item MakeMod(ItemCategory cat, Rarity rar, int rank) {
    int   mult      = RarityMult(rar);
    float rm        = 1.0f + (rank - 1) * 0.25f;
    float bonus     = 20.0f * rm;
    const char* baseName;
    int   basePrice;

    if (cat == ItemCategory::SHIELD_MOD) {
        baseName  = "Shield Booster";
        basePrice = 80;
    } else {
        baseName  = "Hull Plating";
        basePrice = 80;
    }

    Item it;
    it.name      = std::string(baseName)
                   + " Mk" + std::to_string(rank)
                   + " [" + Item::RarityName(rar) + "]";
    it.category  = cat;
    it.rarity    = rar;
    it.rank      = rank;
    it.statBonus = bonus;
    it.buyPrice  = (int)(basePrice * mult * rm);
    it.sellPrice = it.buyPrice / 2;
    return it;
}

// ─── GenerateStock ────────────────────────────────────────────────────────────

void Shop::GenerateStock(int zoneLevel) {
    stock.clear();
    int count = 4 + ri(0, 2);
    int rank  = std::clamp(1 + zoneLevel / 2, 1, 5);

    for (int i = 0; i < count; i++) {
        Rarity rar = RollRarity(zoneLevel);
        if (rand() % 4 < 3) {
            int t = ri(0, WEAPON_COUNT - 1);
            stock.push_back(MakeWeapon(WEAPON_CATALOG[t], rar, rank));
        } else {
            auto cat = (rand() % 2) ? ItemCategory::SHIELD_MOD
                                    : ItemCategory::HULL_MOD;
            stock.push_back(MakeMod(cat, rar, rank));
        }
    }
}

// ─── Constantes visuelles ─────────────────────────────────────────────────────

static const Color SH_PANEL  = {  8, 12, 24, 238 };
static const Color SH_BORDER = { 50, 70, 110, 255 };
static const Color SH_LABEL  = { 150, 165, 195, 255 };
static const Color SH_SEL    = { 60, 100, 180, 200 };

// ─── _DrawItemRow ─────────────────────────────────────────────────────────────

void Shop::_DrawItemRow(Font font, const Item& item,
                        float x, float y, float w, float h,
                        bool hovered, bool canAfford) const {
    Color bg = hovered ? Color{ 35, 55, 100, 210 }
                       : Color{ 14, 20, 38, 180 };
    DrawRectangleRounded({ x, y, w, h }, 0.15f, 4, bg);
    DrawRectangleRoundedLinesEx({ x, y, w, h }, 0.15f, 4, 0.8f,
                                 hovered ? SH_BORDER : Color{ 30, 40, 65, 150 });

    // Barre de rareté (gauche)
    Color rar = Item::RarityColor(item.rarity);
    DrawRectangleRounded({ x, y, 4, h }, 0.2f, 4, rar);

    // Nom
    Color nameCol = canAfford ? WHITE : Color{ 130, 130, 130, 200 };
    DrawTextEx(font, item.name.c_str(), { x + 10, y + 5 }, 14, 1, nameCol);

    // Catégorie + rareté
    char catBuf[48];
    snprintf(catBuf, sizeof(catBuf), "%s  •  %s",
             Item::CategoryName(item.category), Item::RarityName(item.rarity));
    DrawTextEx(font, catBuf, { x + 10, y + h - 18 }, 11, 1, ColorAlpha(rar, 0.8f));

    // Stats
    char statBuf[80] = {};
    if (item.category == ItemCategory::WEAPON) {
        snprintf(statBuf, sizeof(statBuf), "DMG %.0f  CD %.2fs  Rank %d",
                 item.weapon.damage, item.weapon.cooldown, item.rank);
    } else {
        snprintf(statBuf, sizeof(statBuf), "+%.0f MAX  Rank %d",
                 item.statBonus, item.rank);
    }
    float sw = MeasureTextEx(font, statBuf, 12, 1).x;
    DrawTextEx(font, statBuf, { x + w - sw - 80, y + 6 }, 12, 1, SH_LABEL);

    // Prix
    char priceBuf[24];
    snprintf(priceBuf, sizeof(priceBuf), "%d cr",
             (_tab == 0) ? item.buyPrice : item.sellPrice);
    float pw = MeasureTextEx(font, priceBuf, 14, 1).x;
    Color priceCol = canAfford ? Color{ 80, 220, 120, 255 }
                               : Color{ 210, 80, 80, 255 };
    DrawTextEx(font, priceBuf, { x + w - pw - 8, y + h / 2 - 8 }, 14, 1, priceCol);
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void Shop::Draw(Font font, const Economy& eco, const Inventory& inv,
                int screenW, int screenH) const {
    if (!isOpen) return;

    // Fond sombre
    DrawRectangle(0, 0, screenW, screenH, { 0, 0, 0, 170 });

    const float PW = std::min(720.0f, (float)screenW - 40.0f);
    const float PH = (float)screenH - 80.0f;
    const float PX = (screenW - PW) / 2.0f;
    const float PY = 40.0f;

    DrawRectangleRounded({ PX, PY, PW, PH }, 0.04f, 6, SH_PANEL);
    DrawRectangleRoundedLinesEx({ PX, PY, PW, PH }, 0.04f, 6, 1.5f, SH_BORDER);

    // ── Titre ─────────────────────────────────────────────────────────────────
    const char* title = "MERCHANT SHOP";
    float tw = MeasureTextEx(font, title, 22, 1).x;
    DrawTextEx(font, title, { PX + (PW - tw) / 2.0f, PY + 12 }, 22, 1,
               { 220, 200, 100, 255 });

    // Crédits
    char credBuf[32];
    snprintf(credBuf, sizeof(credBuf), "Credits: %d", eco.credits);
    DrawTextEx(font, credBuf, { PX + 12, PY + 14 }, 16, 1,
               { 80, 220, 120, 255 });

    // Fermer
    DrawTextEx(font, "[ESC] Close", { PX + PW - 98, PY + 14 }, 13, 1, SH_LABEL);

    // ── Onglets ───────────────────────────────────────────────────────────────
    const float TAB_Y = PY + 48;
    const float TAB_H = 28;
    const float TAB_W = PW / 2;
    const char* tabs[2] = { "BUY", "SELL (INVENTORY)" };
    for (int i = 0; i < 2; i++) {
        bool active = (_tab == i);
        DrawRectangleRounded({ PX + i * TAB_W, TAB_Y, TAB_W, TAB_H },
                              0.1f, 4,
                              active ? Color{ 28, 52, 110, 255 }
                                     : Color{  8, 15,  35, 200 });
        DrawRectangleRoundedLinesEx({ PX + i * TAB_W, TAB_Y, TAB_W, TAB_H },
                                     0.1f, 4, 0.8f,
                                     active ? SH_BORDER : Color{ 30, 40, 65, 130 });
        float ttw = MeasureTextEx(font, tabs[i], 14, 1).x;
        DrawTextEx(font, tabs[i],
                   { PX + i * TAB_W + (TAB_W - ttw) / 2, TAB_Y + 7 },
                   14, 1, active ? WHITE : SH_LABEL);
    }

    // ── Liste items ───────────────────────────────────────────────────────────
    const float LIST_Y  = TAB_Y + TAB_H + 8;
    const float ROW_H   = 54;
    const float ROW_GAP = 4;
    const float ROW_X   = PX + 8;
    const float ROW_W   = PW - 16;

    const std::vector<Item>& list = (_tab == 0) ? stock : inv.items;

    if (list.empty()) {
        const char* msg = (_tab == 0) ? "No items in stock."
                                       : "Inventory empty.";
        float mw = MeasureTextEx(font, msg, 16, 1).x;
        DrawTextEx(font, msg,
                   { PX + (PW - mw) / 2.0f, LIST_Y + 30 },
                   16, 1, SH_LABEL);
    }

    float rowY = LIST_Y;
    for (int i = 0; i < (int)list.size(); i++) {
        if (rowY + ROW_H > PY + PH - 46) break;
        bool hovered   = (i == _selectedIdx);
        bool canAfford = (_tab == 0) ? eco.CanAfford(list[i].buyPrice) : true;
        _DrawItemRow(font, list[i], ROW_X, rowY, ROW_W, ROW_H, hovered, canAfford);
        rowY += ROW_H + ROW_GAP;
    }

    // ── Footer ────────────────────────────────────────────────────────────────
    const char* footer = (_tab == 0)
        ? "[CLICK] Sélectionner  |  [ENTER] Acheter"
        : "[CLICK] Sélectionner  |  [ENTER] Vendre";
    float fw = MeasureTextEx(font, footer, 13, 1).x;
    DrawTextEx(font, footer,
               { PX + (PW - fw) / 2.0f, PY + PH - 30 },
               13, 1, ColorAlpha(SH_LABEL, 0.75f));
}

// ─── Update ───────────────────────────────────────────────────────────────────

void Shop::Update(Vector2 mousePos, Economy& eco, Inventory& inv,
                  ShipStats& stats, int screenW, int screenH) {
    if (!isOpen) return;

    if (IsKeyPressed(KEY_ESCAPE)) { isOpen = false; return; }

    const float PW = std::min(720.0f, (float)screenW - 40.0f);
    const float PH = (float)screenH - 80.0f;
    const float PX = (screenW - PW) / 2.0f;
    const float PY = 40.0f;

    // Onglets
    const float TAB_Y = PY + 48;
    const float TAB_H = 28;
    const float TAB_W = PW / 2;
    for (int i = 0; i < 2; i++) {
        Rectangle tabR = { PX + i * TAB_W, TAB_Y, TAB_W, TAB_H };
        if (CheckCollisionPointRec(mousePos, tabR) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            _tab = i; _selectedIdx = -1;
        }
    }

    // Liste
    const float LIST_Y  = TAB_Y + TAB_H + 8;
    const float ROW_H   = 54;
    const float ROW_GAP = 4;
    const float ROW_X   = PX + 8;
    const float ROW_W   = PW - 16;

    std::vector<Item>& list = (_tab == 0) ? stock : inv.items;

    float rowY = LIST_Y;
    for (int i = 0; i < (int)list.size(); i++) {
        if (rowY + ROW_H > PY + PH - 46) break;
        Rectangle r = { ROW_X, rowY, ROW_W, ROW_H };
        if (CheckCollisionPointRec(mousePos, r) &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _selectedIdx = i;
        rowY += ROW_H + ROW_GAP;
    }

    // ENTER : acheter ou vendre
    if (IsKeyPressed(KEY_ENTER) && _selectedIdx >= 0) {
        if (_tab == 0 && _selectedIdx < (int)stock.size()) {
            // Acheter
            const Item& item = stock[_selectedIdx];
            if (eco.Buy(item.buyPrice) && !inv.IsFull()) {
                // Appliquer bonus de mod immédiatement
                if (item.category == ItemCategory::SHIELD_MOD)
                    stats.maxShield += item.statBonus;
                else if (item.category == ItemCategory::HULL_MOD)
                    stats.maxHull += item.statBonus;
                inv.AddItem(item);
                stock.erase(stock.begin() + _selectedIdx);
                _selectedIdx = std::min(_selectedIdx, (int)stock.size() - 1);
            }
        } else if (_tab == 1 && _selectedIdx < (int)inv.items.size()) {
            // Vendre — retirer le bonus de mod si applicable
            const Item& item = inv.items[_selectedIdx];
            if (item.category == ItemCategory::SHIELD_MOD)
                stats.maxShield = std::max(1.0f, stats.maxShield - item.statBonus);
            else if (item.category == ItemCategory::HULL_MOD)
                stats.maxHull = std::max(1.0f, stats.maxHull - item.statBonus);
            eco.Earn(item.sellPrice);
            inv.RemoveAt(_selectedIdx);
            _selectedIdx = std::min(_selectedIdx, (int)inv.items.size() - 1);
        }
    }
}
