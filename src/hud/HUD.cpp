#include "HUD.hpp"
#include <cmath>
#include <cstdio>
#include <algorithm>

// ─── Palette couleurs ─────────────────────────────────────────────────────────

static const Color C_HULL_FULL   = { 80,  210, 120, 255 };
static const Color C_HULL_MID    = { 220, 180,  40, 255 };
static const Color C_HULL_LOW    = { 220,  60,  60, 255 };
static const Color C_SHIELD      = {  80, 180, 255, 255 };
static const Color C_SHIELD_REGEN= {  60, 140, 220, 255 };
static const Color C_FUEL        = { 220, 160,  50, 255 };
static const Color C_BAR_BG      = {  20,  20,  30, 200 };
static const Color C_PANEL_BG    = {  10,  12,  20, 180 };
static const Color C_BORDER      = {  50,  60,  80, 255 };
static const Color C_LABEL       = { 160, 170, 190, 255 };

// ─── Init / Unload ────────────────────────────────────────────────────────────

HUD::HUD() {
    _font = GetFontDefault();
}

void HUD::LoadFont(const char* path, int size) {
    _font       = ::LoadFont(path);  // :: pour éviter ambiguité
    _fontLoaded = true;
    SetTextureFilter(_font.texture, TEXTURE_FILTER_BILINEAR);
}

void HUD::Unload() {
    if (_fontLoaded) UnloadFont(_font);
}

// ─── Update ───────────────────────────────────────────────────────────────────

void HUD::Update(float dt, const ShipStats& stats) {
    // Détection des impacts pour les flash
    if (stats.shield < _shieldPrev - 0.5f) _shieldFlash = 1.0f;
    if (stats.hull   < _hullPrev   - 0.5f) _hullFlash   = 1.0f;
    _shieldPrev = stats.shield;
    _hullPrev   = stats.hull;

    _shieldFlash = std::max(0.0f, _shieldFlash - dt * 3.0f);
    _hullFlash   = std::max(0.0f, _hullFlash   - dt * 3.0f);

    // Notifications
    for (auto& n : _notifications) {
        n.pos.y -= 28.0f * dt;
        n.life  -= dt * 0.7f;
    }
    _notifications.erase(
        std::remove_if(_notifications.begin(), _notifications.end(),
            [](const HUDNotification& n){ return n.life <= 0; }),
        _notifications.end()
    );
}

void HUD::PushNotification(const std::string& text, Color color) {
    _notifications.push_back({ text, { 0, 0 }, 1.0f, color });
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

void HUD::_DrawText(const char* text, Vector2 pos, float size, Color color) const {
    DrawTextEx(_font, text, pos, size, 1.0f, color);
}

float HUD::_MeasureText(const char* text, float size) const {
    return MeasureTextEx(_font, text, size, 1.0f).x;
}

// Barre générique avec label flottant au-dessus
void HUD::_DrawBar(Vector2 pos, float w, float h,
                   float pct, Color fill, Color bg,
                   const char* label, float labelSize) const {
    pct = std::clamp(pct, 0.0f, 1.0f);

    // Fond
    DrawRectangleRounded({ pos.x, pos.y, w, h }, 0.3f, 4, bg);
    // Bordure
    DrawRectangleRoundedLinesEx({ pos.x, pos.y, w, h }, 0.3f, 4, 1.0f, C_BORDER);

    // Remplissage
    if (pct > 0.0f) {
        float fillW = (w - 4) * pct;
        DrawRectangleRounded({ pos.x + 2, pos.y + 2, fillW, h - 4 },
                              0.25f, 4, fill);
    }

    // Label au-dessus à gauche
    _DrawText(label, { pos.x, pos.y - labelSize - 3 }, labelSize, C_LABEL);

    // Valeur en % à droite
    char buf[16];
    snprintf(buf, sizeof(buf), "%d%%", (int)(pct * 100));
    float tw = _MeasureText(buf, labelSize);
    _DrawText(buf, { pos.x + w - tw, pos.y - labelSize - 3 }, labelSize, fill);
}

// Fuel en pips discrets (style FTL exact)
void HUD::_DrawFuelPips(Vector2 pos, int current, int max) const {
    const float PIP_W    = 14.0f;
    const float PIP_H    = 18.0f;
    const float PIP_GAP  =  4.0f;

    for (int i = 0; i < max; i++) {
        float x = pos.x + i * (PIP_W + PIP_GAP);
        bool  on = i < current;

        Color fill   = on ? C_FUEL        : C_BAR_BG;
        Color border = on ? C_FUEL        : C_BORDER;

        DrawRectangleRounded({ x, pos.y, PIP_W, PIP_H }, 0.2f, 4, fill);
        DrawRectangleRoundedLinesEx({ x, pos.y, PIP_W, PIP_H },
                                    0.2f, 4, 1.0f, border);
    }
}

// ─── Panneau principal (bas-gauche) ──────────────────────────────────────────

void HUD::_DrawShipStatus(Vector2 origin, const ShipStats& stats) const {
    const float BAR_W    = 200.0f;
    const float BAR_H    =  14.0f;
    const float LABEL_SZ =  14.0f;
    const float ROW_GAP  =  36.0f;
    const float PAD      =  18.0f;

    // Hauteur totale du panneau : 3 lignes + fuel pips
    float panelH = PAD + LABEL_SZ + 3 + BAR_H       // Hull
                 + ROW_GAP + LABEL_SZ + 3 + BAR_H    // Shield
                 + ROW_GAP + LABEL_SZ + 3 + 18.0f    // Fuel (pips)
                 + PAD;
    float panelW = BAR_W + PAD * 2;

    // Fond du panneau
    DrawRectangleRounded({ origin.x, origin.y, panelW, panelH },
                          0.08f, 6, C_PANEL_BG);
    DrawRectangleRoundedLinesEx({ origin.x, origin.y, panelW, panelH },
                                 0.08f, 6, 1.0f, C_BORDER);

    float bx = origin.x + PAD;
    float by = origin.y + PAD + LABEL_SZ + 3;

    // ── HULL ──────────────────────────────────────────────────────────────────
    float hullPct = stats.HullPct();
    Color hullColor = (hullPct > 0.5f) ? C_HULL_FULL
                    : (hullPct > 0.25f) ? C_HULL_MID
                                        : C_HULL_LOW;

    // Flash rouge sur dégâts
    if (_hullFlash > 0.0f)
        hullColor = ColorAlpha({ 255, 80, 80, 255 }, _hullFlash);

    _DrawBar({ bx, by }, BAR_W, BAR_H,
             hullPct, hullColor, C_BAR_BG, "HULL", LABEL_SZ);

    // ── SHIELD ────────────────────────────────────────────────────────────────
    by += ROW_GAP;
    bool isRegen = (stats.shieldRegenTimer <= 0 && stats.shield < stats.maxShield);
    Color shieldColor = isRegen ? C_SHIELD_REGEN : C_SHIELD;

    if (_shieldFlash > 0.0f)
        shieldColor = ColorAlpha({ 255, 255, 100, 255 }, _shieldFlash);

    _DrawBar({ bx, by }, BAR_W, BAR_H,
             stats.ShieldPct(), shieldColor, C_BAR_BG, "SHIELD", LABEL_SZ);

    // Indicateur regen discret
    if (isRegen) {
        _DrawText("regen", { bx + BAR_W / 2 - 16, by + BAR_H + 3 },
                  11.0f, ColorAlpha(C_SHIELD_REGEN, 0.7f));
    } else if (stats.shieldRegenTimer > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.1fs", stats.shieldRegenTimer);
        _DrawText(buf, { bx + BAR_W / 2 - 10, by + BAR_H + 3 },
                  11.0f, ColorAlpha(C_LABEL, 0.6f));
    }

    // ── FUEL ──────────────────────────────────────────────────────────────────
    by += ROW_GAP;
    int fuelCur = (int)stats.fuel;
    int fuelMax = (int)stats.maxFuel;

    // Label + valeur numérique
    _DrawText("FUEL", { bx, by - LABEL_SZ - 3 }, LABEL_SZ, C_LABEL);
    char fuelBuf[16];
    snprintf(fuelBuf, sizeof(fuelBuf), "%d / %d", fuelCur, fuelMax);
    float fw = _MeasureText(fuelBuf, LABEL_SZ);
    _DrawText(fuelBuf, { bx + BAR_W - fw, by - LABEL_SZ - 3 },
              LABEL_SZ, (fuelCur <= 3) ? C_HULL_LOW : C_FUEL);

    _DrawFuelPips({ bx, by }, fuelCur, fuelMax);
}

// ─── Draw principal ───────────────────────────────────────────────────────────

void HUD::Draw(const ShipStats& stats, int screenW, int screenH) const {
    // ── Panneau bas-gauche ────────────────────────────────────────────────────
    _DrawShipStatus({ 16.0f, (float)screenH - 175.0f }, stats);

    // ── Alerte critique (hull < 25%) ──────────────────────────────────────────
    if (stats.HullPct() < 0.25f) {
        float pulse = 0.4f + 0.35f * sinf(GetTime() * 5.0f);
        const char* warn = "!! HULL CRITICAL !!";
        float tw = _MeasureText(warn, 18.0f);
        _DrawText(warn,
                  { (screenW - tw) / 2.0f, (float)screenH - 40.0f },
                  18.0f,
                  ColorAlpha(C_HULL_LOW, pulse));
    }

    // ── Alerte carburant ──────────────────────────────────────────────────────
    if (stats.fuel <= 3 && stats.fuel > 0) {
        float pulse = 0.4f + 0.35f * sinf(GetTime() * 3.5f);
        const char* warn = "LOW FUEL";
        float tw = _MeasureText(warn, 16.0f);
        _DrawText(warn,
                  { (screenW - tw) / 2.0f, (float)screenH - 64.0f },
                  16.0f,
                  ColorAlpha(C_FUEL, pulse));
    }

    // ── Notifications flottantes ──────────────────────────────────────────────
    // Ancrées sur le panneau HUD
    float notifX = 16.0f + 18.0f;
    float notifBaseY = (float)screenH - 185.0f;

    for (int i = 0; i < (int)_notifications.size(); i++) {
        const auto& n = _notifications[i];
        float alpha = std::min(n.life * 2.0f, 1.0f);
        _DrawText(n.text.c_str(),
                  { notifX, notifBaseY - 24.0f * i + n.pos.y },
                  15.0f,
                  ColorAlpha(n.color, alpha));
    }
}