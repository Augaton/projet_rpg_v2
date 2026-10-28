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
// Quatre consoles biseautées translucides : statut, FTL, énergie, armes.

static constexpr Rectangle STATUS_AREA   = {   0,   0, 480, 116 };
static constexpr Rectangle STATUS_PANEL  = {  10,  12, 462,  98 };
static constexpr Rectangle FTL_AREA      = { 496,   8, 182,  70 };
static constexpr Rectangle FTL_PANEL     = { 500,  12, 172,  62 };
static constexpr Rectangle SYSTEMS_AREA  = {   0, 530, 356, 190 };
static constexpr Rectangle SYSTEMS_PANEL = {  10, 546, 340, 166 };
static constexpr Rectangle WEAPONS_AREA  = { 356, 606, 616, 114 };
static constexpr Rectangle WEAPONS_PANEL = { 360, 612, 608, 100 };

static constexpr float PIP_BASELINE = 670.0f;   // bas des barres d'énergie
static constexpr float PIP_AREA_H   = 104.0f;
static constexpr float RES_Y        = 84.0f;    // centre de la ligne des ressources

static_assert(ICON_SHIELD == SYS_SHIELDS && ICON_SENSORS == SYS_SENSORS, "icônes de systèmes = indices SYS_*");

Rectangle HUD::JumpButtonRect() { return { 512, 42, 150, 26 }; }
Vector2   HUD::ScrapAnchor()    { return { 172, RES_Y }; }
Rectangle HUD::AugmentRect(int slot) { return { 262.0f + slot * 68.0f, RES_Y - 10, 64, 20 }; }

Rectangle HUD::SystemColumnRect(int slot) {
    return { 60.0f + slot * 42.0f, PIP_BASELINE - PIP_AREA_H - 4, 36.0f, PIP_AREA_H + 30 };
}

Rectangle HUD::WeaponCardRect(int slot) {
    return { WEAPONS_PANEL.x + 10 + slot * 148.0f, WEAPONS_PANEL.y + 12, 140, 78 };
}

// Icône de la famille d'une arme (missile, ion, laser, canon)
static int WeaponIcon(const WeaponDef& d) {
    if (d.missile) return ICON_MISSILE;
    if (d.ion > 0) return ICON_ION;
    return d.proj == ProjType::LASER ? ICON_LASER : ICON_CANNON;
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

    DrawRectangleRec({ a.x - 3 + jx, a.y - 3, a.width + 6, a.height + 6 }, ColorAlpha(BLACK, 0.55f));
    DrawRectangleLinesEx({ a.x - 3 + jx, a.y - 3, a.width + 6, a.height + 6 }, 1.0f, ColorAlpha(col, 0.35f));
    for (int i = 0; i < n; i++) {
        Rectangle r = { std::floor(a.x + jx + i * (pw + gap)), a.y, std::max(1.0f, std::floor(pw)), a.height };
        if ((float)i < displayed) {
            DrawRectangleRec(r, col);
            DrawRectangleRec({ r.x, r.y, r.width, 3 }, ColorAlpha(WHITE, 0.35f));
            DrawRectangleRec({ r.x, r.y + r.height - 3, r.width, 3 }, ColorAlpha(BLACK, 0.25f));
        } else if ((float)i < lag) {
            DrawRectangleRec(r, C_HULL_TRAIL);
        } else {
            DrawRectangleRec(r, ColorAlpha(C_BAR_BG, 0.9f));
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
            Bevel(r, 4, LerpColor(C_SHIELD, WHITE, flash * 0.7f));
            DrawRectangleRec({ r.x + 4, r.y + 2, 6, 2 }, ColorAlpha(WHITE, 0.7f));
        } else if (i < maxLayers) {
            float k = (i == s.shieldLayers) ? s.shieldCharge : 0.0f;
            Bevel(r, 4, ColorAlpha(BLACK, 0.6f));
            DrawRectangleRec({ r.x + 1, r.y + S * (1.0f - k), S - 2, S * k }, ColorAlpha(C_SHIELD, 0.5f));
            BevelLines(r, 4, C_SHIELD);
        } else {
            BevelLines(r, 4, ColorAlpha(C_BORDER, 0.8f));
        }
        if (s.sys[SYS_SHIELDS].ionTimer > 0.0f)
            BevelLines({ r.x - 2, r.y - 2, S + 4, S + 4 }, 5, C_ION);
    }
    return width;
}

// ─── Statut (coque, boucliers, ressources) ───────────────────────────────────

void HUD::_DrawStatus(const Ship& p, bool /*inCombat*/, float t) const {
    const Rectangle P = STATUS_PANEL;
    Console(P, C_ACCENT, p.name.c_str(), C_ACCENT);

    float hullPct = (float)p.hull / (float)std::max(1, p.maxHull);
    DrawIcon(ICON_HULL, { P.x + 18, P.y + 19 }, 1.0f, HullColor(hullPct));
    _DrawHullBar(p, { P.x + 34, P.y + 12, 318, 14 }, _hullDisplayed, _hullLag, _hullFlash);
    Label(TextFormat("%d/%d", p.hull, p.maxHull), P.x + 362, P.y + 9, 20, HullColor(hullPct));

    DrawIcon(ICON_SHIELD, { P.x + 18, P.y + 43 }, 1.0f, p.MaxShieldLayers() > 0 ? C_SHIELD : C_TEXT_DIM);
    float w = _DrawShieldCells(p, P.x + 34, P.y + 36, _shieldFlash, false);
    float ex = P.x + 34 + w + (w > 0 ? 22 : 8);
    Color evCol = p.Evasion() > 0 ? C_LABEL : C_HULL_LOW;
    DrawIcon(ICON_ENGINES, { ex, P.y + 43 }, 1.0f, evCol);
    Label(TextFormat("ESQUIVE %d%%", p.Evasion()), ex + 12, P.y + 39, 10, evCol);

    // Séparateur
    DrawRectangleRec({ P.x + 10, P.y + 58, P.width - 20, 1 }, ColorAlpha(C_BORDER, 0.6f));

    struct Res { int icon; int value; Color color; bool low; float bump; };
    Res res[3] = {
        { ICON_FUEL,    p.fuel,     C_FUEL,    p.fuel <= 3,     0.0f },
        { ICON_MISSILE, p.missiles, C_MISSILE, p.missiles == 0, 0.0f },
        { ICON_SCRAP,   p.scrap,    C_SCRAP,   false,           _scrapBump },
    };
    float x = P.x + 18;
    for (const auto& r : res) {
        bool blink = r.low && std::fmod(t, 0.8f) < 0.4f;
        DrawIcon(r.icon, { x, RES_Y }, 1.0f, blink ? C_HULL_LOW : r.color);
        float size = r.bump > 0.4f ? 30.0f : 20.0f;
        Label(TextFormat("%d", r.value), x + 13, RES_Y - size * 0.5f, size,
              r.bump > 0.0f ? LerpColor(r.color, WHITE, r.bump) : (r.low ? C_HULL_LOW : C_TEXT));
        x += 72;
    }

    // Augmentations
    for (int i = 0; i < MAX_AUGMENTS; i++) {
        Rectangle r = AugmentRect(i);
        if (p.augments[i] < 0) {
            BevelLines(r, 5, ColorAlpha(C_BORDER, 0.5f));
            continue;
        }
        bool hover = _hoverAug == p.augments[i];
        Bevel(r, 5, hover ? C_SCRAP : ColorAlpha(C_SCRAP, 0.18f));
        BevelLines(r, 5, C_SCRAP);
        LabelCentered(GetAugDef(p.augments[i]).shortName, r.x + r.width / 2, r.y + 6, 10, hover ? C_INK : C_SCRAP);
    }
}

// ─── Réacteur & systèmes ─────────────────────────────────────────────────────

void HUD::_DrawSystems(const Ship& p) const {
    Console(SYSTEMS_PANEL, C_POWER, "ÉNERGIE", C_POWER);
    TextRight("G+  D-  molette", SYSTEMS_PANEL.x + SYSTEMS_PANEL.width - 12, SYSTEMS_PANEL.y + 6, 12, C_TEXT_DIM);

    // Réacteur : énergie libre en vert
    {
        int   n    = std::clamp(p.reactor, 1, 40);
        float gap  = n > 12 ? 1.0f : 2.0f;
        float ph   = std::max(1.0f, std::min(10.0f, std::floor((PIP_AREA_H - gap * (n - 1)) / n)));
        int   free = p.FreePower();
        float x    = 22;
        DrawRectangleRec({ x - 3, PIP_BASELINE - n * (ph + gap) - 1, 30, n * (ph + gap) + 4 }, ColorAlpha(BLACK, 0.5f));
        for (int i = 0; i < n; i++) {
            Rectangle pr = { x, PIP_BASELINE - (i + 1) * ph - i * gap, 24, ph };
            if (i < free) {
                DrawRectangleRec(pr, C_POWER);
                DrawRectangleRec({ pr.x, pr.y, pr.width, 2 }, ColorAlpha(WHITE, 0.4f));
            } else {
                DrawRectangleRec(pr, ColorAlpha(C_BAR_BG, 0.9f));
            }
        }
        DrawIcon(ICON_POWER, { x + 12, PIP_BASELINE + 14 }, 1.0f, free > 0 ? C_POWER : C_TEXT_DIM);
        LabelCentered(TextFormat("%d", free), x + 12, PIP_BASELINE + 26, 10, free > 0 ? C_POWER : C_TEXT_DIM);
    }
    DrawRectangleRec({ 52, PIP_BASELINE - PIP_AREA_H, 1, PIP_AREA_H + 36 }, ColorAlpha(C_BORDER, 0.6f));

    int vis[SYS_COUNT];
    int nv = VisibleSystems(p, vis);
    for (int slot = 0; slot < nv; slot++) {
        int s = vis[slot];
        const ShipSystem& x = p.sys[s];
        Rectangle col = SystemColumnRect(slot);
        bool hover = (s == _hoverSys);

        if (hover) {
            Bevel(col, 6, ColorAlpha(C_ACCENT, 0.10f));
            BevelLines(col, 6, ColorAlpha(C_ACCENT, 0.5f));
        }

        const float ph = 10.0f, gap = 3.0f;
        int ion = (x.ionTimer > 0.0f && x.damage < x.level) ? 1 : 0;
        for (int i = 0; i < x.level; i++) {
            Rectangle pr = { col.x + 6, PIP_BASELINE - (i + 1) * ph - i * gap, col.width - 12, ph };
            bool isBroken = i >= x.level - x.damage;
            bool isIon    = !isBroken && ion && i == x.level - x.damage - 1;

            if (x.hackTimer > 0.0f)  DrawRectangleLinesEx(pr, 2.0f, C_ION);
            else if (isBroken) {
                DrawRectangleRec(pr, ColorAlpha(C_HULL_LOW, 0.85f));
                DrawLineEx({ pr.x + 2, pr.y + pr.height - 2 }, { pr.x + pr.width - 2, pr.y + 2 }, 2.0f, ColorAlpha(BLACK, 0.6f));
            }
            else if (isIon)          DrawRectangleLinesEx(pr, 2.0f, C_ION);
            else if (i < x.power) {
                DrawRectangleRec(pr, C_POWER);
                DrawRectangleRec({ pr.x, pr.y, pr.width, 2 }, ColorAlpha(WHITE, 0.4f));
            }
            else {
                DrawRectangleRec(pr, ColorAlpha(BLACK, 0.45f));
                DrawRectangleLinesEx(pr, 1.0f, ColorAlpha(C_LABEL, 0.45f));
            }
        }
        // Réparation passive des modules en combat : barre de progression bien visible
        if (x.damage > 0 && x.repairProgress > 0.0f) {
            float top = PIP_BASELINE - x.level * (ph + gap) - 8;
            DrawRectangleRec({ col.x + 4, top, col.width - 8, 5 }, ColorAlpha(BLACK, 0.6f));
            DrawRectangleRec({ col.x + 4, top, (col.width - 8) * x.repairProgress, 5 }, C_WARNING);
        }

        Color iconCol = x.damage >= x.level ? C_HULL_LOW
                      : x.damage > 0        ? C_WARNING
                      : x.power > 0         ? (hover ? C_TEXT : C_POWER)
                                            : (hover ? C_TEXT : C_LABEL);
        DrawIcon(s, { col.x + col.width / 2, PIP_BASELINE + 14 }, 1.0f, iconCol);
        LabelCentered(TextFormat("%d/%d", x.power, x.level), col.x + col.width / 2, PIP_BASELINE + 26, 10, C_TEXT_DIM);
    }
}

// ─── Armes ───────────────────────────────────────────────────────────────────

void HUD::_DrawWeapons(const Ship& p, int selected, bool inCombat, float t) const {
    Console(WEAPONS_PANEL, C_ENEMY, "ARMES", C_LABEL);

    for (int i = 0; i < p.weaponSlots && i < MAX_WEAPON_SLOTS; i++) {
        Rectangle r = WeaponCardRect(i);
        const WeaponSlot& w = p.weapons[i];

        // Emplacement d'origine de la carte saisie : cadre seul
        if (_dragging && i == _dragSlot) {
            BevelLines(r, 8, ColorAlpha(C_BORDER, 0.8f));
            continue;
        }
        if (w.Empty()) {
            Bevel(r, 8, ColorAlpha(BLACK, 0.25f));
            for (float x = r.x + 8; x < r.x + r.width - 8; x += 12) {
                DrawRectangleRec({ x, r.y, 6, 1 }, ColorAlpha(C_BORDER, 0.7f));
                DrawRectangleRec({ x, r.y + r.height - 1, 6, 1 }, ColorAlpha(C_BORDER, 0.7f));
            }
            LabelCentered("VIDE", r.x + r.width / 2, r.y + 34, 10, ColorAlpha(C_TEXT_DIM, 0.7f));
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
            const WeaponDef& d = GetWeaponDef(p.weapons[_dragSlot].id);
            Color c = inCombat ? C_ENEMY : C_TEXT_DIM;
            float w = MeasureLabel(d.shortName, 10) + 50;
            Rectangle chip = { _dragMouse.x - w - 16, _dragMouse.y - 34, w, 22 };   // à gauche : l'info-bulle de salle est à droite
            Bevel(chip, 5, ColorAlpha(C_INK, 0.88f));
            BevelLines(chip, 5, c);
            DrawRectangleRec({ chip.x + 4, chip.y + 4, 14, 14 }, c);
            Label(TextFormat("%d", _dragSlot + 1), chip.x + 8, chip.y + 6, 10, C_INK);
            DrawIcon(WeaponIcon(d), { chip.x + 30, chip.y + 11 }, 1.0f, c);
            Label(d.shortName, chip.x + 42, chip.y + 6, 10, C_TEXT);
            float rr = 10.0f;
            DrawRectangleLinesEx({ _dragMouse.x - rr, _dragMouse.y - rr, rr * 2, rr * 2 }, 2.0f, c);
        } else {
            Rectangle r = WeaponCardRect(_dragSlot);
            r.x += _dragMouse.x - _dragStart.x;
            r.y += _dragMouse.y - _dragStart.y;
            Bevel({ r.x + 5, r.y + 5, r.width, r.height }, 8, ColorAlpha(BLACK, 0.6f));
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
    const float cut = 8.0f;

    Color accent = isSel ? C_ENEMY : ready ? C_GOOD : w.powered ? C_POWER : C_BORDER;

    // Halo « prêt » + flash au moment où la charge se termine
    if (ready && inCombat)
        BevelLines({ r.x - 3, r.y - 3, r.width + 6, r.height + 6 }, cut + 2, ColorAlpha(accent, 0.25f + 0.3f * pulse), 2.0f);
    Bevel(r, cut, isSel ? Color{ 52, 10, 10, 235 } : ColorAlpha(BLACK, w.powered ? 0.62f : 0.42f));
    if (w.powered)
        DrawRectangleGradientV((int)r.x, (int)(r.y + cut), (int)r.width, 30, ColorAlpha(accent, 0.14f), BLANK);
    BevelLines(r, cut, ColorAlpha(accent, w.powered ? 1.0f : 0.6f), (isSel || hover || ready) ? 2.0f : 1.0f);
    if (_readyFlash[i] > 0.0f)
        Bevel(r, cut, ColorAlpha(WHITE, _readyFlash[i] * 0.5f));

    // Numéro d'emplacement et nom
    Rectangle num = { r.x + 4, r.y + 4, 16, 16 };
    Bevel(num, 4, accent);
    LabelCentered(TextFormat("%d", i + 1), num.x + 8, num.y + 3, 10, C_INK);
    Label(def.shortName, r.x + 26, r.y + 5, 20, w.powered ? C_TEXT : C_LABEL);

    for (int k = 0; k < def.power; k++) {
        Rectangle pr = { r.x + r.width - 12 - k * 9.0f, r.y + 32, 6, 12 };
        if (w.powered) DrawRectangleRec(pr, C_POWER);
        else           DrawRectangleLinesEx(pr, 1.0f, C_LABEL);
    }

    const char* status;
    Color       statusCol;
    if (!w.powered)          { status = "HORS TENSION"; statusCol = C_TEXT_DIM; }
    else if (noAmmo)         { status = "PAS DE MISSILE"; statusCol = C_HULL_LOW; }
    else if (!inCombat) {
        status    = def.ion > 0 ? TextFormat("ION x%d", def.shots)
                                : TextFormat("%d DÉG x%d", def.damage, def.shots);
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
    DrawIcon(WeaponIcon(def), { r.x + 14, r.y + 38 }, 1.0f, w.powered ? accent : C_TEXT_DIM);
    Label(status, r.x + 26, r.y + 34, 10, statusCol);

    // Jauge de charge segmentée
    Rectangle bar = { r.x + 8, r.y + r.height - 18, r.width - 18, 7 };
    DrawRectangleRec({ bar.x - 2, bar.y - 2, bar.width + 3, bar.height + 4 }, ColorAlpha(BLACK, 0.5f));
    SegBar(bar, w.charge, 12, ready ? LerpColor(C_GOOD, WHITE, 0.3f * pulse) : C_POWER, ColorAlpha(C_BAR_BG, 0.9f));
}

// ─── Info-bulle ──────────────────────────────────────────────────────────────

void HUD::_DrawTooltip(const Ship& p) const {
    if (_dragging) return;
    std::string title, body;
    int icon = -1;

    if (_hoverSys >= 0) {
        const ShipSystem& x = p.sys[_hoverSys];
        title = TextFormat("%s  NIV.%d", SystemName(_hoverSys), x.level);
        body  = SystemDescription(_hoverSys);
        icon  = _hoverSys;
        if (_hoverSys == SYS_ENGINES) body += TextFormat(" Esquive actuelle : %d%%.", p.Evasion());
        if (x.damage > 0)      body += TextFormat(" Endommagé : %d barre(s).", x.damage);
        if (x.ionTimer > 0.0f) body += TextFormat(" Ionisé (%.0fs).", x.ionTimer);
    } else if (_hoverAug >= 0) {
        title = GetAugDef(_hoverAug).name;
        body  = GetAugDef(_hoverAug).description;
    } else if (_hoverWeapon >= 0 && !p.weapons[_hoverWeapon].Empty()) {
        const WeaponDef& d = GetWeaponDef(p.weapons[_hoverWeapon].id);
        title = d.name;
        icon  = WeaponIcon(d);
        body  = TextFormat("Énergie %d · Charge %.0fs · %d projectile(s)", d.power, d.charge, d.shots);
        if (d.damage > 0) body += TextFormat(" · %d dégât(s)", d.damage);
        if (d.ion > 0)    body += ". Ionique : retire une couche de bouclier ou bloque un système.";
        if (d.missile)    body += ". Traverse les boucliers, consomme 1 missile par salve.";
        if (d.pierce > 0) body += TextFormat(". Perce jusqu'à %d couche(s) de bouclier.", d.pierce);
        body += "\nG : allumer / viser    D : éteindre    Maj : toutes les armes\nGlisser : changer d'emplacement, ou sur une salle ennemie pour la viser";
    } else {
        return;
    }

    const float W = 360.0f;
    float h = TextWrapped(body.c_str(), { 0, 0, W - 28, 0 }, 14, WHITE, false, false);
    Rectangle r = { 12, SYSTEMS_PANEL.y - h - 52, W, h + 42 };
    if (_hoverWeapon >= 0) r = { WeaponCardRect(_hoverWeapon).x, WEAPONS_PANEL.y - h - 52, W, h + 42 };
    if (_hoverAug >= 0)    r = { 18, STATUS_PANEL.y + STATUS_PANEL.height + 10, W, h + 42 };
    r.x = std::min(r.x, (float)VIRTUAL_W - W - 12);

    Bevel({ r.x + 3, r.y + 3, r.width, r.height }, 8, ColorAlpha(BLACK, 0.4f));
    Bevel(r, 8, ColorAlpha(C_INK, 0.92f));
    BevelLines(r, 8, ColorAlpha(C_ACCENT, 0.8f));
    DrawRectangleRec({ r.x + 8, r.y, r.width - 8, 2 }, C_ACCENT);
    float tx = r.x + 14;
    if (icon >= 0) {
        DrawIcon(icon, { r.x + 22, r.y + 16 }, 1.0f, C_ACCENT);
        tx += 18;
    }
    Label(title.c_str(), tx, r.y + 12, 10, C_ACCENT);
    TextWrapped(body.c_str(), { r.x + 14, r.y + 30, W - 28, h }, 14, C_LABEL);
}

// ─── Draw public ─────────────────────────────────────────────────────────────

void HUD::DrawPlayer(const Ship& p, int selected, bool inCombat, float t) const {
    _DrawStatus(p, inCombat, t);
    _DrawSystems(p);
    _DrawWeapons(p, selected, inCombat, t);
    _DrawTooltip(p);

    if ((float)p.hull / (float)std::max(1, p.maxHull) < 0.25f && std::fmod(t, 0.6f) < 0.4f) {
        const char* msg = "COQUE CRITIQUE";
        float w = MeasureLabel(msg, 20) + 60;
        Rectangle b = { (VIRTUAL_W - w) / 2.0f, 186, w, 28 };
        Bevel(b, 6, ColorAlpha(C_INK, 0.85f));
        BevelLines(b, 6, C_HULL_LOW, 2.0f);
        DrawIcon(ICON_WARNING, { b.x + 18, b.y + 14 }, 1.0f, C_HULL_LOW);
        DrawIcon(ICON_WARNING, { b.x + b.width - 18, b.y + 14 }, 1.0f, C_HULL_LOW);
        LabelCentered(msg, VIRTUAL_W / 2.0f, b.y + 4, 20, C_HULL_LOW);
    }
    if (inCombat && p.sys[SYS_SHIELDS].Installed() && p.MaxShieldLayers() == 0)
        LabelCentered("BOUCLIERS HORS LIGNE", VIRTUAL_W / 2.0f, 220, 10, C_SHIELD);
}

void HUD::DrawFtlPanel(const Ship& p, bool inCombat, const char* label, bool enabled) const {
    bool charged = !inCombat || p.ftlCharge >= 1.0f;
    bool active  = enabled && charged;
    float charge = inCombat ? p.ftlCharge : 1.0f;

    Console(FTL_PANEL, C_SHIELD);
    Label("FTL", 512, 20, 10, C_SHIELD);
    if (inCombat && !charged) {
        float rate = p.FtlChargeRate();
        const char* eta = rate > 0.0f ? TextFormat("%.0fs", (1.0f - charge) / rate) : "MOTEURS HS";
        LabelRight(eta, 662, 20, 10, rate > 0.0f ? C_LABEL : C_HULL_LOW);
    } else {
        LabelRight("PRÊT", 662, 20, 10, C_SHIELD);
    }
    SegBar({ 512, 31, 151, 6 }, charge, 15, charged ? C_SHIELD : ColorAlpha(C_SHIELD, 0.6f), ColorAlpha(BLACK, 0.5f));

    Rectangle btn = JumpButtonRect();
    bool over = active && Hover(btn);
    Bevel(btn, 6, over ? C_SHIELD : ColorAlpha(BLACK, active ? 0.6f : 0.35f));
    if (active && !over) Bevel(btn, 6, ColorAlpha(C_SHIELD, 0.12f + 0.08f * std::sin((float)GetTime() * 4.0f)));
    BevelLines(btn, 6, active ? C_SHIELD : C_BORDER, active ? 2.0f : 1.0f);
    LabelCentered(label, btn.x + btn.width / 2, btn.y + 8, 10, over ? C_INK : active ? C_TEXT : C_TEXT_DIM);
}

void HUD::DrawEnemy(const Ship& e, int fleeState, float fleeValue, float t) const {
    const float R = VIRTUAL_W - 18.0f;
    const Rectangle P = { VIRTUAL_W - 392.0f, 12, 382, 104 };
    if (e.trait >= 0) Console(P, C_SCRAP, TextFormat("ÉLITE - %s", EliteTraitName(e.trait)), C_SCRAP);
    else              Console(P, C_ENEMY, "HOSTILE", C_ENEMY);

    LabelRight(e.name.c_str(), R, 18, 20, C_TEXT);

    // Barre de coque (les seuils de phase du boss s'alignent dessus, voir Combat::DrawOverlay)
    float disp = (float)e.hull;
    _DrawHullBar(e, { R - 360, 42, 360, 12 }, disp, disp, e.hitFlash);
    _DrawShieldCells(e, R - 360, 62, 0.0f, false);
    LabelRight(TextFormat("ESQ %d%%   %d/%d", e.Evasion(), e.hull, e.maxHull), R, 64, 10, C_LABEL);

    // Armes ennemies : charge et cible, pour anticiper la prochaine salve
    {
        float x = R - 360;
        for (const auto& w : e.weapons) {
            if (w.Empty()) continue;
            const WeaponDef& d = GetWeaponDef(w.id);
            bool imminent = w.powered && w.charge > 0.85f;
            Color c = !w.powered ? C_TEXT_DIM : imminent ? C_ENEMY : C_LABEL;
            DrawIcon(WeaponIcon(d), { x + 6, 88 }, 1.0f, c);
            Label(d.shortName, x + 18, 84, 10, c);
            DrawRectangleRec({ x, 98, 84, 4 }, ColorAlpha(BLACK, 0.6f));
            DrawRectangleRec({ x, 98, 84 * w.charge, 4 },
                             imminent && std::fmod(t, 0.3f) < 0.15f ? WHITE : (w.powered ? C_ENEMY : C_TEXT_DIM));
            if (w.powered && w.target >= 0)
                Label(TextFormat("> %s", SystemShortName(w.target)), x, 104, 10, ColorAlpha(c, 0.8f));
            x += 92;
        }
    }

    if (fleeState == 1) {
        bool blink = std::fmod(t, 0.5f) < 0.3f;
        Rectangle b = { R - 360, 126, 360, 22 };
        Bevel(b, 6, blink ? C_WARNING : ColorAlpha(C_WARNING, 0.25f));
        LabelCentered(TextFormat("INTENTION DE FUITE - %.0fs", std::ceil(fleeValue)), b.x + b.width / 2, b.y + 6, 10,
                      blink ? C_INK : C_WARNING);
        TextShadow("Achevez-le ou visez ses MOTEURS", R - 360, 152, 13, C_TEXT_DIM);
    } else if (fleeState == 2) {
        Rectangle b = { R - 360, 126, 360, 22 };
        Bevel(b, 6, ColorAlpha(BLACK, 0.7f));
        DrawRectangleRec({ b.x + 6, b.y + 3, (b.width - 12) * fleeValue, b.height - 6 }, ColorAlpha(C_WARNING, 0.8f));
        BevelLines(b, 6, C_WARNING);
        LabelCentered(TextFormat("FTL ENNEMI %d%%", (int)(fleeValue * 100)), b.x + b.width / 2, b.y + 6, 10, C_TEXT);
        TextShadow("Moteurs HS = charge bloquée", R - 360, 152, 13, C_WARNING);
    }
}

void HUD::DrawLog() const {
    float y = 590.0f;
    for (int i = (int)_log.size() - 1; i >= 0; i--) {
        const LogLine& l = _log[i];
        float a = std::clamp((10.0f - l.age) / 2.0f, 0.0f, 1.0f);
        // Entrée en glissant depuis la droite
        float slide = std::max(0.0f, 1.0f - l.age * 6.0f);
        const float size = 14.0f;
        float w = Measure(l.text.c_str(), size);
        float x = VIRTUAL_W - 22.0f - w + slide * 40.0f;
        DrawRectangleGradientH((int)(x - 14), (int)y, (int)(w + 30), 19, BLANK, ColorAlpha(BLACK, 0.55f * a));
        DrawRectangleRec({ x - 10, y + 3, 3, 13 }, ColorAlpha(l.color, a));
        TextShadow(l.text.c_str(), x, y + 1, size, ColorAlpha(l.color, a));
        y -= 21.0f;
    }
}
