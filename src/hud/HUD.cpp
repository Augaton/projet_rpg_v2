#include "HUD.hpp"
#include "../core/Config.hpp"
#include "../audio/AudioManager.hpp"
#include "../core/UI.hpp"
#include "../ship/ShipFactory.hpp"
#include "../ship/Weapons.hpp"
#include <algorithm>
#include <cmath>

using namespace ui;

// ─── Layout ──────────────────────────────────────────────────────────────────
// Pas de gros panneaux : des éléments flottants sur des dégradés discrets.

static constexpr Rectangle STATUS_AREA  = {   0,   0, 500, 116 };
static constexpr Rectangle FTL_AREA     = { 506,   8, 166,  66 };
static constexpr Rectangle SYSTEMS_AREA = {   0, 530, 350, 190 };
static constexpr Rectangle WEAPONS_AREA = { 354, 616, 616, 104 };

static constexpr float PIP_BASELINE = 670.0f;   // bas des barres d'énergie
static constexpr float PIP_AREA_H   = 104.0f;
static constexpr float RES_Y        = 76.0f;

Rectangle HUD::JumpButtonRect() { return { 512, 42, 150, 26 }; }
Vector2   HUD::ScrapAnchor()    { return { 258, RES_Y + 10 }; }
Rectangle HUD::AugmentRect(int slot) { return { 18.0f + slot * 58.0f, RES_Y + 24, 52, 16 }; }

Rectangle HUD::SystemColumnRect(int slot) {
    return { 60.0f + slot * 42.0f, PIP_BASELINE - PIP_AREA_H - 4, 36.0f, PIP_AREA_H + 30 };
}

Rectangle HUD::WeaponCardRect(int slot) {
    return { WEAPONS_AREA.x + 8 + slot * 150.0f, WEAPONS_AREA.y + 12, 144, 74 };
}

// Modules alimentables installés, dans l'ordre d'affichage des colonnes
int HUD::VisibleSystems(const Ship& p, int out[SYS_COUNT]) {
    int n = 0;
    for (int s = 0; s < SYS_COUNT; s++)
        if (SystemUsesPower(s) && p.sys[s].Installed()) out[n++] = s;
    return n;
}

static Color HullColor(float pct) {
    return pct > 0.5f ? C_HULL_FULL : pct > 0.25f ? C_HULL_MID : C_HULL_LOW;
}

static void TextShadow(const char* text, float x, float y, float size, Color color) {
    Text(text, x + 1, y + 1, size, ColorAlpha(BLACK, color.a / 255.0f * 0.9f));
    Text(text, x, y, size, color);
}

// ─── Update / input ──────────────────────────────────────────────────────────

void HUD::Update(float dt, const Ship& p) {
    if (_hullDisplayed < 0.0f) {
        _hullDisplayed = _hullLag = (float)p.hull;
        _prevHull   = p.hull;
        _prevLayers = p.shieldLayers;
        _prevScrap  = p.scrap;
    }

    if (p.hull < _prevHull)           _hullFlash   = 1.0f;
    // Alarme quand la coque passe sous 25 %
    if (_audio && p.hull < _prevHull && p.hull * 4 < p.maxHull && _prevHull * 4 >= p.maxHull)
        _audio->PlaySfx("sx_alarm", 1.0f, 0.7f);
    if (p.shieldLayers < _prevLayers) _shieldFlash = 1.0f;
    if (p.scrap > _prevScrap)         _scrapBump   = 1.0f;
    _prevHull   = p.hull;
    _prevLayers = p.shieldLayers;
    _prevScrap  = p.scrap;

    float hull = (float)p.hull;
    _hullDisplayed += (hull - _hullDisplayed) * std::min(dt * 10.0f, 1.0f);
    if (hull > _hullLag) _hullLag = hull;
    else                 _hullLag += (hull - _hullLag) * std::min(dt * 2.5f, 1.0f);

    _hullFlash   = std::max(0.0f, _hullFlash   - dt * 3.0f);
    _shieldFlash = std::max(0.0f, _shieldFlash - dt * 3.0f);
    _scrapBump   = std::max(0.0f, _scrapBump   - dt * 4.0f);

    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        float c = p.weapons[i].Empty() ? 0.0f : p.weapons[i].charge;
        if (_prevCharge[i] < 1.0f && c >= 1.0f) {
            _readyFlash[i] = 1.0f;
            if (_audio) _audio->PlaySfx("sx_ready", 1.0f + 0.08f * (float)i, 0.35f);
        }
        _prevCharge[i] = c;
        _readyFlash[i] = std::max(0.0f, _readyFlash[i] - dt * 3.0f);
    }

    for (auto& l : _log) l.age += dt;
    std::erase_if(_log, [](const LogLine& l) { return l.age > 10.0f; });

    Vector2 m = GetMousePosition();
    _hoverSys = -1;
    _hoverWeapon = -1;
    _hoverAug = -1;
    for (int i = 0; i < MAX_AUGMENTS; i++)
        if (p.augments[i] >= 0 && CheckCollisionPointRec(m, AugmentRect(i))) _hoverAug = p.augments[i];
    int vis[SYS_COUNT];
    int nv = VisibleSystems(p, vis);
    for (int i = 0; i < nv; i++)
        if (CheckCollisionPointRec(m, SystemColumnRect(i))) _hoverSys = vis[i];
    for (int i = 0; i < p.weaponSlots && i < MAX_WEAPON_SLOTS; i++)
        if (CheckCollisionPointRec(m, WeaponCardRect(i))) _hoverWeapon = i;
}

int HUD::_WeaponSlotAt(const Ship& p, Vector2 m) const {
    for (int i = 0; i < p.weaponSlots && i < MAX_WEAPON_SLOTS; i++)
        if (CheckCollisionPointRec(m, WeaponCardRect(i))) return i;
    return -1;
}

void HUD::_DropWeapon(Ship& p, Vector2 m, int* selected, bool inCombat) {
    int from = _dragSlot;
    bool wasDragging = _dragging;
    _dragSlot = -1;
    _dragging = false;
    if (from < 0 || from >= MAX_WEAPON_SLOTS) return;

    int to = _WeaponSlotAt(p, m);
    if (!wasDragging) {
        if (to == from) _ClickWeapon(p, from, selected, inCombat);
        return;
    }
    // Lâchée hors du bandeau : le combat peut l'utiliser pour viser une salle
    if (to < 0) {
        _dropSlot = from;
        _dropPos  = m;
        return;
    }
    if (to < 0 || to == from || !p.SwapWeapons(from, to)) return;
    std::swap(_prevCharge[from], _prevCharge[to]);
    std::swap(_readyFlash[from], _readyFlash[to]);
    if (selected) {
        if      (*selected == from) *selected = to;
        else if (*selected == to)   *selected = from;
    }
    if (_audio) _audio->PlaySfx("sx_click", 0.8f, 0.5f);
}

void HUD::_ClickWeapon(Ship& p, int i, int* selected, bool inCombat) {
    WeaponSlot& w = p.weapons[i];
    if (w.Empty()) return;
    if (!w.wanted) {
        if (!p.PowerWeapon(i)) {
            if (_audio) _audio->PlaySfx("sx_error", 1.0f, 0.4f);
            const WeaponDef& def = GetWeaponDef(w.id);
            if (p.sys[SYS_WEAPONS].power + def.power > p.sys[SYS_WEAPONS].Capacity())
                Log("Système d'armes insuffisant pour " + std::string(def.name), C_WARNING);
            else
                Log("Réacteur : plus d'énergie disponible", C_WARNING);
        }
    } else if (inCombat && selected) {
        *selected = (*selected == i) ? -1 : i;
    }
}

bool HUD::HandleInput(Ship& p, int* selected, bool inCombat) {
    Vector2 m     = GetMousePosition();
    bool    left  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    bool    right = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    float   wheel = GetMouseWheelMove();
    bool    shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    _dropSlot = -1;   // une dépose hors des cartes n'est valable qu'une frame

    // Carte d'arme saisie : clic court = action habituelle, glisser = échange d'emplacement
    if (_dragSlot >= 0) {
        if (_dragSlot >= std::min(p.weaponSlots, MAX_WEAPON_SLOTS) || p.weapons[_dragSlot].Empty()) {
            _dragSlot = -1;
            _dragging = false;
        } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            _dragMouse = m;
            float dx = m.x - _dragStart.x, dy = m.y - _dragStart.y;
            if (!_dragging && dx * dx + dy * dy > 8.0f * 8.0f) _dragging = true;
            return true;
        } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            _DropWeapon(p, m, selected, inCombat);
            return true;
        } else {
            // Relâché pendant un autre écran : on abandonne la saisie
            _dragSlot = -1;
            _dragging = false;
        }
    }

    int vis[SYS_COUNT];
    int nv = VisibleSystems(p, vis);
    for (int i = 0; i < nv; i++) {
        int s = vis[i];
        if (!CheckCollisionPointRec(m, SystemColumnRect(i))) continue;

        // Maj + clic : tout de suite au maximum / tout couper
        if (shift && (left || right)) {
            int n = 0;
            if (left) while (n < 16 && p.AddPower(s)) n++;
            else      while (n < 16 && p.RemovePower(s)) n++;
            if (n > 0 || right) return true;
        }
        if ((left || wheel > 0.0f) && !p.AddPower(s) && left) {
            if (_audio) _audio->PlaySfx("sx_error", 1.0f, 0.4f);
            if (s == SYS_WEAPONS)
                Log("Aucune arme ne peut être alimentée", C_WARNING);
            else if (p.sys[s].desired >= p.sys[s].Capacity())
                Log(std::string(SystemName(s)) + " : capacité maximale", C_WARNING);
            else
                Log("Réacteur : plus d'énergie disponible", C_WARNING);
        }
        if (right || wheel < 0.0f) p.RemovePower(s);
        return true;
    }

    int slot = _WeaponSlotAt(p, m);
    if (slot >= 0) {
        // Maj + clic : allumer / éteindre toutes les armes
        if (shift && (left || right)) {
            int n = 0;
            for (int i = 0; i < std::min(p.weaponSlots, MAX_WEAPON_SLOTS); i++) {
                if (p.weapons[i].Empty()) continue;
                if (left && !p.weapons[i].wanted && p.PowerWeapon(i)) n++;
                if (right && p.weapons[i].wanted) {
                    p.DepowerWeapon(i);
                    p.weapons[i].target = -1;
                    n++;
                }
            }
            if (right && selected) *selected = -1;
            if (left && n == 0 && _audio) _audio->PlaySfx("sx_error", 1.0f, 0.4f);
            return true;
        }
        if (!p.weapons[slot].Empty()) {
            if (wheel > 0.0f && !p.weapons[slot].wanted) p.PowerWeapon(slot);
            if (wheel < 0.0f && p.weapons[slot].wanted) {
                p.DepowerWeapon(slot);
                if (selected && *selected == slot) *selected = -1;
            }
        }
        if (left && !p.weapons[slot].Empty()) {
            _dragSlot  = slot;
            _dragging  = false;
            _dragStart = _dragMouse = m;
        }
        if (right && !p.weapons[slot].Empty()) {
            p.DepowerWeapon(slot);
            p.weapons[slot].target = -1;
            if (selected && *selected == slot) *selected = -1;
        }
        return true;
    }

    for (Rectangle r : { STATUS_AREA, WEAPONS_AREA, Rectangle{ 0, 540, 350, 180 } })
        if (CheckCollisionPointRec(m, r)) return true;
    if (inCombat && CheckCollisionPointRec(m, FTL_AREA)) return true;

    return false;
}

void HUD::Log(const std::string& text, Color color) {
    if (!_log.empty() && _log.back().text == text && _log.back().age < 1.0f) {
        _log.back().age = 0.0f;
        return;
    }
    _log.push_back({ text, color, 0.0f });
    if (_log.size() > 6) _log.erase(_log.begin());
}

// ─── Blocs communs ───────────────────────────────────────────────────────────

void HUD::_DrawHullBar(const Ship& s, Rectangle a, float displayed, float lag, float flash) const {
    int   n   = std::clamp(s.maxHull, 1, 200);
    float gap = n > 30 ? 1.0f : 2.0f;
    float pw  = (a.width - gap * (n - 1)) / n;
    float pct = (float)s.hull / (float)n;
    Color col = LerpColor(HullColor(pct), WHITE, flash * 0.8f);

    // Tremblement de la barre quand la coque encaisse
    float jx = flash > 0.0f ? std::round(std::sin(flash * 60.0f) * 3.0f * flash) : 0.0f;

    DrawRectangleRec({ a.x - 2 + jx, a.y - 2, a.width + 4, a.height + 4 }, ColorAlpha(BLACK, 0.45f));
    for (int i = 0; i < n; i++) {
        Rectangle r = { std::floor(a.x + jx + i * (pw + gap)), a.y, std::max(1.0f, std::floor(pw)), a.height };
        if ((float)i < displayed) {
            DrawRectangleRec(r, col);
            DrawRectangleRec({ r.x, r.y, r.width, 3 }, ColorAlpha(WHITE, 0.25f));
        } else if ((float)i < lag) {
            DrawRectangleRec(r, C_HULL_TRAIL);
        } else {
            DrawRectangleRec(r, ColorAlpha(C_BAR_BG, 0.8f));
        }
    }
}

// Renvoie la largeur occupée
float HUD::_DrawShieldCells(const Ship& s, float x, float y, float flash, bool alignRight) const {
    int potential = std::min(s.sys[SYS_SHIELDS].level / 2, 8);
    int maxLayers = s.MaxShieldLayers();
    const float S = 14.0f, STEP = 18.0f;
    float width = potential > 0 ? potential * STEP - (STEP - S) : 0.0f;
    if (alignRight) x -= width;

    for (int i = 0; i < potential; i++) {
        Rectangle r = { x + i * STEP, y, S, S };
        if (i < s.shieldLayers) {
            DrawRectangleRec(r, LerpColor(C_SHIELD, WHITE, flash * 0.7f));
            DrawRectangleRec({ r.x + 2, r.y + 2, 4, 4 }, ColorAlpha(WHITE, 0.7f));
        } else if (i < maxLayers) {
            float k = (i == s.shieldLayers) ? s.shieldCharge : 0.0f;
            DrawRectangleRec(r, ColorAlpha(BLACK, 0.5f));
            DrawRectangleRec({ r.x, r.y + S * (1.0f - k), S, S * k }, ColorAlpha(C_SHIELD, 0.55f));
            DrawRectangleLinesEx(r, 1.0f, C_SHIELD);
        } else {
            DrawRectangleLinesEx(r, 1.0f, ColorAlpha(C_BORDER, 0.8f));
        }
        if (s.sys[SYS_SHIELDS].ionTimer > 0.0f)
            DrawRectangleLinesEx({ r.x - 2, r.y - 2, S + 4, S + 4 }, 1.0f, C_ION);
    }
    return width;
}

// ─── Statut (coque, boucliers, ressources) ───────────────────────────────────

void HUD::_DrawStatus(const Ship& p, bool /*inCombat*/, float t) const {
    DrawRectangleGradientH(0, 0, (int)STATUS_AREA.width, (int)STATUS_AREA.height, ColorAlpha(BLACK, 0.6f), BLANK);

    Label(p.name.c_str(), 18, 12, 10, C_ACCENT);
    _DrawHullBar(p, { 18, 28, 360, 14 }, _hullDisplayed, _hullLag, _hullFlash);
    Label(TextFormat("%d/%d", p.hull, p.maxHull), 392, 25, 20, HullColor((float)p.hull / (float)std::max(1, p.maxHull)));

    float w = _DrawShieldCells(p, 18, 52, _shieldFlash, false);
    Label(TextFormat("ESQ %d%%", p.Evasion()), 18 + w + (w > 0 ? 12 : 0), 55, 10, p.Evasion() > 0 ? C_LABEL : C_HULL_LOW);

    struct Res { const char* label; int value; Color color; bool low; float bump; };
    Res res[3] = {
        { "CARB", p.fuel,     C_FUEL,    p.fuel <= 3,     0.0f },
        { "MISS", p.missiles, C_MISSILE, p.missiles == 0, 0.0f },
        { "FERR", p.scrap,    C_SCRAP,   false,           _scrapBump },
    };
    float x = 18;
    for (const auto& r : res) {
        bool blink = r.low && std::fmod(t, 0.8f) < 0.4f;
        DrawRectangleRec({ x, RES_Y + 4, 8, 8 }, blink ? C_HULL_LOW : r.color);
        Label(r.label, x + 14, RES_Y + 3, 10, C_TEXT_DIM);
        float size = r.bump > 0.4f ? 30.0f : 20.0f;
        Label(TextFormat("%d", r.value), x + 60, RES_Y - (size - 20) * 0.5f - 1, size,
              r.bump > 0.0f ? LerpColor(r.color, WHITE, r.bump) : (r.low ? C_HULL_LOW : C_TEXT));
        x += 120;
    }

    // Augmentations
    for (int i = 0; i < MAX_AUGMENTS; i++) {
        Rectangle r = AugmentRect(i);
        if (p.augments[i] < 0) {
            DrawRectangleLinesEx(r, 1.0f, ColorAlpha(C_BORDER, 0.5f));
            continue;
        }
        bool hover = _hoverAug == p.augments[i];
        DrawRectangleRec(r, hover ? C_SCRAP : ColorAlpha(C_SCRAP, 0.25f));
        DrawRectangleLinesEx(r, 1.0f, C_SCRAP);
        LabelCentered(GetAugDef(p.augments[i]).shortName, r.x + r.width / 2, r.y + 4, 10, hover ? C_INK : C_SCRAP);
    }


}

// ─── Réacteur & systèmes ─────────────────────────────────────────────────────

void HUD::_DrawSystems(const Ship& p) const {
    DrawRectangleGradientV((int)SYSTEMS_AREA.x, (int)SYSTEMS_AREA.y, (int)SYSTEMS_AREA.width, (int)SYSTEMS_AREA.height,
                           BLANK, ColorAlpha(BLACK, 0.65f));
    Label("ENERGIE", 18, 546, 10, C_POWER);
    Text("G+  D-  molette", 92, 544, 12, C_TEXT_DIM);

    // Réacteur : énergie libre en vert
    {
        int   n    = std::clamp(p.reactor, 1, 40);
        float gap  = n > 12 ? 1.0f : 2.0f;
        float ph   = std::max(1.0f, std::min(10.0f, std::floor((PIP_AREA_H - gap * (n - 1)) / n)));
        int   free = p.FreePower();
        float x    = 18;
        for (int i = 0; i < n; i++) {
            Rectangle pr = { x, PIP_BASELINE - (i + 1) * ph - i * gap, 24, ph };
            DrawRectangleRec(pr, i < free ? C_POWER : ColorAlpha(C_BAR_BG, 0.8f));
        }
        LabelCentered(TextFormat("%d", free), x + 12, PIP_BASELINE + 6, 20, free > 0 ? C_POWER : C_TEXT_DIM);
        LabelCentered("LIBRE", x + 12, PIP_BASELINE + 28, 10, C_TEXT_DIM);
    }

    int vis[SYS_COUNT];
    int nv = VisibleSystems(p, vis);
    for (int slot = 0; slot < nv; slot++) {
        int s = vis[slot];
        const ShipSystem& x = p.sys[s];
        Rectangle col = SystemColumnRect(slot);
        bool hover = (s == _hoverSys);

        if (hover) DrawRectangleRec(col, ColorAlpha(C_ACCENT, 0.10f));

        const float ph = 10.0f, gap = 3.0f;
        int ion = (x.ionTimer > 0.0f && x.damage < x.level) ? 1 : 0;
        for (int i = 0; i < x.level; i++) {
            Rectangle pr = { col.x + 6, PIP_BASELINE - (i + 1) * ph - i * gap, col.width - 12, ph };
            bool isBroken = i >= x.level - x.damage;
            bool isIon    = !isBroken && ion && i == x.level - x.damage - 1;

            if (x.hackTimer > 0.0f)  DrawRectangleLinesEx(pr, 2.0f, C_ION);
            else if (isBroken)       DrawRectangleRec(pr, ColorAlpha(C_HULL_LOW, 0.8f));
            else if (isIon)          DrawRectangleLinesEx(pr, 2.0f, C_ION);
            else if (i < x.power)    DrawRectangleRec(pr, C_POWER);
            else                     DrawRectangleLinesEx(pr, 1.0f, ColorAlpha(C_LABEL, 0.5f));
        }
        // Réparation passive des modules en combat : barre de progression bien visible
        if (x.damage > 0 && x.repairProgress > 0.0f) {
            float top = PIP_BASELINE - x.level * (ph + gap) - 8;
            DrawRectangleRec({ col.x + 4, top, col.width - 8, 5 }, ColorAlpha(BLACK, 0.6f));
            DrawRectangleRec({ col.x + 4, top, (col.width - 8) * x.repairProgress, 5 }, C_WARNING);
        }

        Color nameCol = x.damage >= x.level ? C_HULL_LOW
                      : x.damage > 0        ? C_WARNING
                      : hover               ? C_TEXT : C_LABEL;
        LabelCentered(SystemShortName(s), col.x + col.width / 2, PIP_BASELINE + 8, 10, nameCol);
        LabelCentered(TextFormat("%d/%d", x.power, x.level), col.x + col.width / 2, PIP_BASELINE + 24, 10, C_TEXT_DIM);
    }
}

// ─── Armes ───────────────────────────────────────────────────────────────────

void HUD::_DrawWeapons(const Ship& p, int selected, bool inCombat, float t) const {
    DrawRectangleGradientV((int)WEAPONS_AREA.x, (int)WEAPONS_AREA.y, (int)WEAPONS_AREA.width, (int)WEAPONS_AREA.height,
                           BLANK, ColorAlpha(BLACK, 0.6f));

    for (int i = 0; i < p.weaponSlots && i < MAX_WEAPON_SLOTS; i++) {
        Rectangle r = WeaponCardRect(i);
        const WeaponSlot& w = p.weapons[i];

        // Emplacement d'origine de la carte saisie : cadre pointillé
        if (_dragging && i == _dragSlot) {
            DrawRectangleLinesEx(r, 1.0f, ColorAlpha(C_BORDER, 0.8f));
            continue;
        }
        if (w.Empty()) {
            for (float x = r.x; x < r.x + r.width; x += 12) {
                DrawRectangleRec({ x, r.y, 6, 1 }, ColorAlpha(C_BORDER, 0.7f));
                DrawRectangleRec({ x, r.y + r.height - 1, 6, 1 }, ColorAlpha(C_BORDER, 0.7f));
            }
            LabelCentered("VIDE", r.x + r.width / 2, r.y + 32, 10, ColorAlpha(C_TEXT_DIM, 0.7f));
            continue;
        }

        _DrawWeaponCard(p, i, r, selected, inCombat, t);
    }

    // Carte en cours de déplacement : suit la souris, destination encadrée
    if (_dragging && _dragSlot >= 0 && _dragSlot < std::min(p.weaponSlots, MAX_WEAPON_SLOTS) && !p.weapons[_dragSlot].Empty()) {
        int to = _WeaponSlotAt(p, _dragMouse);
        if (to >= 0 && to != _dragSlot)
            CornerBrackets(WeaponCardRect(to), 10.0f, C_ACCENT);
        if (_dragMouse.y < WEAPONS_AREA.y - 30.0f) {
            // Hors du bandeau : pastille compacte pour ne pas masquer les salles visées
            const char* name = GetWeaponDef(p.weapons[_dragSlot].id).shortName;
            Color c = inCombat ? C_ENEMY : C_TEXT_DIM;
            float w = MeasureLabel(name, 10) + 34;
            Rectangle chip = { _dragMouse.x - w - 16, _dragMouse.y - 32, w, 18 };   // à gauche : l'info-bulle de salle est à droite
            DrawRectangleRec(chip, ColorAlpha(C_INK, 0.85f));
            DrawRectangleLinesEx(chip, 1.0f, c);
            DrawRectangleRec({ chip.x, chip.y, 16, 18 }, c);
            Label(TextFormat("%d", _dragSlot + 1), chip.x + 4, chip.y + 4, 10, C_INK);
            Label(name, chip.x + 22, chip.y + 4, 10, C_TEXT);
            float rr = 10.0f;
            DrawRectangleLinesEx({ _dragMouse.x - rr, _dragMouse.y - rr, rr * 2, rr * 2 }, 2.0f, c);
        } else {
            Rectangle r = WeaponCardRect(_dragSlot);
            r.x += _dragMouse.x - _dragStart.x;
            r.y += _dragMouse.y - _dragStart.y;
            DrawRectangleRec({ r.x + 5, r.y + 5, r.width, r.height }, ColorAlpha(BLACK, 0.6f));
            _DrawWeaponCard(p, _dragSlot, r, selected, inCombat, t);
        }
    }
}

void HUD::_DrawWeaponCard(const Ship& p, int i, Rectangle r, int selected, bool inCombat, float t) const {
    const WeaponSlot& w = p.weapons[i];
    bool hover = (i == _hoverWeapon) || (_dragging && i == _dragSlot);

    const WeaponDef& def = GetWeaponDef(w.id);
    bool ready  = w.powered && w.charge >= 1.0f;
    bool isSel  = (i == selected);
    bool noAmmo = def.missile && p.missiles <= 0;
    float pulse = 0.5f + 0.5f * std::sin(t * 6.0f);

    Color accent = isSel ? C_ENEMY : ready ? C_GOOD : w.powered ? C_POWER : C_BORDER;

    // Halo « prêt » + flash au moment où la charge se termine
    if (ready && inCombat)
        DrawRectangleRec({ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, ColorAlpha(accent, 0.12f + 0.12f * pulse));
    DrawRectangleRec(r, ColorAlpha(isSel ? Color{ 60, 10, 8, 255 } : BLACK, w.powered ? 0.6f : 0.4f));
    DrawRectangleLinesEx(r, (isSel || hover || ready) ? 2.0f : 1.0f, ColorAlpha(accent, w.powered ? 1.0f : 0.6f));
    if (_readyFlash[i] > 0.0f)
        DrawRectangleRec(r, ColorAlpha(WHITE, _readyFlash[i] * 0.5f));

    DrawRectangleRec({ r.x, r.y, 14, 14 }, accent);
    Label(TextFormat("%d", i + 1), r.x + 3, r.y + 2, 10, C_INK);
    Label(def.shortName, r.x + 22, r.y + 5, 20, w.powered ? C_TEXT : C_LABEL);

    for (int k = 0; k < def.power; k++) {
        Rectangle pr = { r.x + r.width - 12 - k * 10.0f, r.y + 32, 6, 12 };
        if (w.powered) DrawRectangleRec(pr, C_POWER);
        else           DrawRectangleLinesEx(pr, 1.0f, C_LABEL);
    }

    const char* status;
    Color       statusCol;
    if (!w.powered)          { status = "HORS TENSION"; statusCol = C_TEXT_DIM; }
    else if (noAmmo)         { status = "PAS DE MISSILE"; statusCol = C_HULL_LOW; }
    else if (!inCombat) {
        status    = def.ion > 0 ? TextFormat("ION x%d", def.shots)
                                : TextFormat("%d DEG x%d%s", def.damage, def.shots, def.missile ? " MSL" : "");
        statusCol = C_TEXT_DIM;
    }
    else if (!ready) {
        float left = (1.0f - w.charge) * def.charge;
        status    = w.target >= 0 ? TextFormat("%s %.1fs", SystemShortName(w.target), left)
                                  : TextFormat("CHARGE %.1fs", left);
        statusCol = w.target >= 0 ? C_ENEMY : C_LABEL;
    }
    else if (w.target >= 0)  { status = TextFormat("FEU > %s", SystemShortName(w.target)); statusCol = C_ENEMY; }
    else                     { status = "PRÊT : CIBLER"; statusCol = pulse > 0.5f ? C_GOOD : C_TEXT; }
    Label(status, r.x + 8, r.y + 34, 10, statusCol);

    // Barre de charge fine et segmentée
    Rectangle bar = { r.x + 8, r.y + r.height - 14, r.width - 16, 6 };
    DrawRectangleRec(bar, ColorAlpha(C_BAR_BG, 0.8f));
    const int SEG = 12;
    float segW = bar.width / SEG;
    int filled = (int)(w.charge * SEG + 0.001f);
    for (int k = 0; k < filled; k++)
        DrawRectangleRec({ bar.x + k * segW + 1, bar.y, segW - 2, bar.height },
                         ready ? LerpColor(C_GOOD, WHITE, 0.3f * pulse) : C_POWER);
}

// ─── Info-bulle ──────────────────────────────────────────────────────────────

void HUD::_DrawTooltip(const Ship& p) const {
    if (_dragging) return;
    std::string title, body;

    if (_hoverSys >= 0) {
        const ShipSystem& x = p.sys[_hoverSys];
        title = TextFormat("%s  NIV.%d", SystemName(_hoverSys), x.level);
        body  = SystemDescription(_hoverSys);
        if (_hoverSys == SYS_ENGINES) body += TextFormat(" Esquive actuelle : %d%%.", p.Evasion());
        if (x.damage > 0)      body += TextFormat(" Endommagé : %d barre(s).", x.damage);
        if (x.ionTimer > 0.0f) body += TextFormat(" Ionisé (%.0fs).", x.ionTimer);
    } else if (_hoverAug >= 0) {
        title = GetAugDef(_hoverAug).name;
        body  = GetAugDef(_hoverAug).description;
    } else if (_hoverWeapon >= 0 && !p.weapons[_hoverWeapon].Empty()) {
        const WeaponDef& d = GetWeaponDef(p.weapons[_hoverWeapon].id);
        title = d.name;
        body  = TextFormat("Énergie %d · Charge %.0fs · %d projectile(s)", d.power, d.charge, d.shots);
        if (d.damage > 0) body += TextFormat(" · %d dégât(s)", d.damage);
        if (d.ion > 0)    body += ". Ionique : retire une couche de bouclier ou bloque un système.";
        if (d.missile)    body += ". Traverse les boucliers, consomme 1 missile.";
        body += "\nG : allumer / viser    D : éteindre    Maj : toutes les armes\nGlisser : changer d'emplacement, ou sur une salle ennemie pour la viser";
    } else {
        return;
    }

    const float W = 360.0f;
    float h = TextWrapped(body.c_str(), { 0, 0, W - 28, 0 }, 14, WHITE, false, false);
    Rectangle r = { 12, SYSTEMS_AREA.y - h - 44, W, h + 40 };
    if (_hoverWeapon >= 0) r = { WeaponCardRect(_hoverWeapon).x, WEAPONS_AREA.y - h - 44, W, h + 40 };
    if (_hoverAug >= 0)    r = { 18, STATUS_AREA.y + STATUS_AREA.height + 4, W, h + 40 };
    r.x = std::min(r.x, (float)VIRTUAL_W - W - 12);

    DrawRectangleRec(r, ColorAlpha(C_INK, 0.88f));
    DrawRectangleLinesEx(r, 1.0f, ColorAlpha(C_ACCENT, 0.8f));
    DrawRectangleRec({ r.x, r.y, 3, r.height }, C_ACCENT);
    Label(title.c_str(), r.x + 14, r.y + 10, 10, C_ACCENT);
    TextWrapped(body.c_str(), { r.x + 14, r.y + 28, W - 28, h }, 14, C_LABEL);
}

// ─── Draw public ─────────────────────────────────────────────────────────────

void HUD::DrawPlayer(const Ship& p, int selected, bool inCombat, float t) const {
    _DrawStatus(p, inCombat, t);
    _DrawSystems(p);
    _DrawWeapons(p, selected, inCombat, t);
    _DrawTooltip(p);

    if ((float)p.hull / (float)std::max(1, p.maxHull) < 0.25f && std::fmod(t, 0.6f) < 0.4f)
        LabelCentered("!! COQUE CRITIQUE !!", VIRTUAL_W / 2.0f, 190, 20, C_HULL_LOW);
    if (inCombat && p.sys[SYS_SHIELDS].Installed() && p.MaxShieldLayers() == 0)
        LabelCentered("BOUCLIERS HORS LIGNE", VIRTUAL_W / 2.0f, 214, 10, C_SHIELD);
}

void HUD::DrawFtlPanel(const Ship& p, bool inCombat, const char* label, bool enabled) const {
    bool charged = !inCombat || p.ftlCharge >= 1.0f;
    bool active  = enabled && charged;
    float charge = inCombat ? p.ftlCharge : 1.0f;

    Label("FTL", 512, 14, 10, C_SHIELD);
    if (inCombat && !charged) {
        float rate = p.FtlChargeRate();
        const char* eta = rate > 0.0f ? TextFormat("%.0fs", (1.0f - charge) / rate) : "MOTEURS HS";
        LabelRight(eta, 662, 14, 10, rate > 0.0f ? C_LABEL : C_HULL_LOW);
    } else {
        LabelRight("PRET", 662, 14, 10, C_SHIELD);
    }

    Rectangle bar = { 512, 28, 150, 8 };
    DrawRectangleRec(bar, ColorAlpha(BLACK, 0.5f));
    const int SEG = 15;
    float segW = bar.width / SEG;
    for (int k = 0; k < (int)(charge * SEG + 0.001f); k++)
        DrawRectangleRec({ bar.x + k * segW + 1, bar.y, segW - 2, bar.height }, charged ? C_SHIELD : ColorAlpha(C_SHIELD, 0.6f));

    Rectangle btn = JumpButtonRect();
    bool over = active && Hover(btn);
    DrawRectangleRec(btn, over ? C_SHIELD : ColorAlpha(BLACK, active ? 0.6f : 0.35f));
    DrawRectangleLinesEx(btn, active ? 2.0f : 1.0f, active ? C_SHIELD : C_BORDER);
    LabelCentered(label, btn.x + btn.width / 2, btn.y + 8, 10, over ? C_INK : active ? C_TEXT : C_TEXT_DIM);
}

void HUD::DrawEnemy(const Ship& e, int fleeState, float fleeValue, float t) const {
    DrawRectangleGradientH(VIRTUAL_W - 520, 0, 520, 164, BLANK, ColorAlpha(BLACK, 0.6f));

    const float R = VIRTUAL_W - 18.0f;
    LabelRight(e.name.c_str(), R, 12, 20, C_TEXT);
    if (e.trait >= 0) {
        const char* label = TextFormat("ELITE - %s", EliteTraitName(e.trait));
        float w = MeasureLabel(label, 10) + 10;
        DrawRectangleRec({ R - 362, 13, w, 14 }, C_SCRAP);
        Label(label, R - 357, 16, 10, C_INK);
    } else {
        Label("HOSTILE", R - 360, 16, 10, C_ENEMY);
    }

    // Barre de coque remplie depuis la droite
    float disp = (float)e.hull;
    _DrawHullBar(e, { R - 360, 40, 360, 12 }, disp, disp, e.hitFlash);
    _DrawShieldCells(e, R - 360, 60, 0.0f, false);
    LabelRight(TextFormat("ESQ %d%%   %d/%d", e.Evasion(), e.hull, e.maxHull), R, 62, 10, C_LABEL);

    // Armes ennemies : charge et cible, pour anticiper la prochaine salve
    {
        float x = R - 360;
        for (const auto& w : e.weapons) {
            if (w.Empty()) continue;
            const WeaponDef& d = GetWeaponDef(w.id);
            bool imminent = w.powered && w.charge > 0.85f;
            Color c = !w.powered ? C_TEXT_DIM : imminent ? C_ENEMY : C_LABEL;
            Label(d.shortName, x, 84, 10, c);
            DrawRectangleRec({ x, 98, 84, 4 }, ColorAlpha(BLACK, 0.5f));
            DrawRectangleRec({ x, 98, 84 * w.charge, 4 },
                             imminent && std::fmod(t, 0.3f) < 0.15f ? WHITE : (w.powered ? C_ENEMY : C_TEXT_DIM));
            if (w.powered && w.target >= 0)
                Label(TextFormat("> %s", SystemShortName(w.target)), x, 106, 10, ColorAlpha(c, 0.8f));
            x += 92;
        }
    }

    if (fleeState == 1) {
        bool blink = std::fmod(t, 0.5f) < 0.3f;
        Rectangle b = { R - 360, 124, 360, 22 };
        DrawRectangleRec(b, blink ? C_WARNING : ColorAlpha(C_WARNING, 0.25f));
        LabelCentered(TextFormat("INTENTION DE FUITE - %.0fs", std::ceil(fleeValue)), b.x + b.width / 2, b.y + 6, 10,
                      blink ? C_INK : C_WARNING);
        TextShadow("Achevez-le ou visez ses MOTEURS", R - 360, 150, 13, C_TEXT_DIM);
    } else if (fleeState == 2) {
        Rectangle b = { R - 360, 124, 360, 22 };
        DrawRectangleRec(b, ColorAlpha(BLACK, 0.6f));
        DrawRectangleRec({ b.x, b.y, b.width * fleeValue, b.height }, ColorAlpha(C_WARNING, 0.8f));
        DrawRectangleLinesEx(b, 1.0f, C_WARNING);
        LabelCentered(TextFormat("FTL ENNEMI %d%%", (int)(fleeValue * 100)), b.x + b.width / 2, b.y + 6, 10, C_TEXT);
        TextShadow("Moteurs HS = charge bloquée", R - 360, 150, 13, C_WARNING);
    }
}

void HUD::DrawLog() const {
    float y = 596.0f;
    for (int i = (int)_log.size() - 1; i >= 0; i--) {
        const LogLine& l = _log[i];
        float a = std::clamp((10.0f - l.age) / 2.0f, 0.0f, 1.0f);
        const float size = 14.0f;
        float w = Measure(l.text.c_str(), size);
        float x = VIRTUAL_W - 18.0f - w;
        DrawRectangleRec({ x - 10, y + 3, 3, 12 }, ColorAlpha(l.color, a));
        TextShadow(l.text.c_str(), x, y, size, ColorAlpha(l.color, a));
        y -= 20.0f;
    }
}
