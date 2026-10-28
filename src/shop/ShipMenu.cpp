#include "ShipMenu.hpp"
#include "../audio/AudioManager.hpp"
#include "../core/UI.hpp"
#include "../hud/HUD.hpp"
#include <algorithm>
#include <cmath>
#include <string>

using namespace ui;

static constexpr Rectangle PANEL = { 150, 40, 980, 640 };

void ShipMenu::Draw(Ship& s, HUD& hud, AudioManager& audio, float /*t*/) {
    if (!_open) return;

    Dim(0.6f);
    Panel(PANEL, C_SHIELD, 0.3f, C_PANEL_BG, "VAISSEAU // MODULES");

    Label(s.name.c_str(), PANEL.x + 28, PANEL.y + 24, 30, C_TEXT);
    Text("Améliorez vos modules. Nouveaux modules : en magasin.",
         PANEL.x + 30, PANEL.y + 60, 14, C_TEXT_DIM);

    // Augmentations installées
    {
        float ax = PANEL.x + PANEL.width - 28;
        for (int i = MAX_AUGMENTS - 1; i >= 0; i--) {
            const char* name = s.augments[i] >= 0 ? GetAugDef(s.augments[i]).name : "-- libre --";
            float w = Measure(name, 13) + 16;
            ax -= w;
            DrawRectangleRec({ ax, PANEL.y + 58, w, 20 }, ColorAlpha(C_SCRAP, s.augments[i] >= 0 ? 0.2f : 0.05f));
            Text(name, ax + 8, PANEL.y + 61, 13, s.augments[i] >= 0 ? C_SCRAP : C_TEXT_DIM);
            ax -= 6;
        }
    }
    LabelRight(TextFormat("FERRAILLE %d", s.scrap), PANEL.x + PANEL.width - 28, PANEL.y + 24, 30, C_SCRAP);

    auto upgraded = [&](const std::string& what) {
        hud.Log("Amélioration : " + what, C_GOOD);
        audio.PlaySfx("gun_1", 1.8f, 0.4f);
    };

    // ── Réacteur ─────────────────────────────────────────────────────────────
    {
        Rectangle r = { PANEL.x + 20, PANEL.y + 90, PANEL.width - 40, 56 };
        Bevel(r, 10, ColorAlpha(C_PANEL_2, 0.95f));
        DrawRectangleRec({ r.x + 10, r.y, r.width - 10, 2 }, C_POWER);
        Label("RÉACTEUR", r.x + 16, r.y + 10, 20, C_TEXT);
        Text(TextFormat("Énergie totale %d  ·  libre %d", s.reactor, s.FreePower()), r.x + 16, r.y + 34, 13, C_LABEL);

        const int maxReactor = 25;
        for (int i = 0; i < maxReactor; i++) {
            Rectangle pip = { r.x + 230 + i * 20.0f, r.y + 18, 14, 20 };
            if (i < s.reactor) DrawRectangleRec(pip, C_POWER);
            else               DrawRectangleLinesEx(pip, 1.0f, ColorAlpha(C_BORDER, 0.8f));
        }

        int cost = ReactorUpgradeCost(s.reactor);
        bool can = s.reactor < maxReactor && s.scrap >= cost;
        const char* label = s.reactor >= maxReactor ? "MAX" : TextFormat("+1  %d", cost);
        if (Button({ r.x + r.width - 130, r.y + 12, 118, 32 }, label, can, C_POWER, 20)) {
            s.scrap -= cost;
            s.reactor++;
            s.AutoPowerNew();
            upgraded("réacteur +1");
        }
    }

    // ── Modules (2 colonnes) ─────────────────────────────────────────────────
    const float cardW = (PANEL.width - 50) / 2.0f;
    const float cardH = 96.0f;
    int hovered = -1;

    for (int sys = 0; sys < SYS_COUNT; sys++) {
        int col = sys % 2, row = sys / 2;
        Rectangle r = { PANEL.x + 20 + col * (cardW + 10), PANEL.y + 160 + row * (cardH + 6), cardW, cardH };
        const ShipSystem& x = s.sys[sys];
        int maxL      = SystemMaxLevel(sys);
        bool installed = x.Installed();
        bool maxed    = x.level >= maxL;

        if (Hover(r)) hovered = sys;
        Color accent = installed ? C_ACCENT : C_BORDER;

        Bevel(r, 10, ColorAlpha(C_PANEL_2, installed ? 0.95f : 0.5f));
        if (hovered == sys) BevelLines(r, 10, accent, 2.0f);
        DrawRectangleRec({ r.x + 10, r.y, r.width - 10, 2 }, ColorAlpha(accent, installed ? 1.0f : 0.5f));

        Label(SystemName(sys), r.x + 16, r.y + 12, 20, installed ? C_TEXT : C_TEXT_DIM);
        if (!SystemUsesPower(sys)) Label("SANS ÉNERGIE", r.x + 16 + MeasureLabel(SystemName(sys), 20) + 12, r.y + 17, 10, C_TEXT_DIM);

        // Niveaux
        for (int i = 0; i < maxL; i++) {
            Rectangle pip = { r.x + 16 + i * 18.0f, r.y + 40, 14, 10 };
            if (i < x.level) DrawRectangleRec(pip, C_ACCENT);
            else             DrawRectangleLinesEx(pip, 1.0f, ColorAlpha(C_BORDER, 0.9f));
        }

        if (installed) {
            Text(TextFormat("Actuel : %s", SystemLevelEffect(sys, x.level)), r.x + 16, r.y + 58, 13, C_LABEL);
            if (!maxed)
                Text(TextFormat("Suivant : %s", SystemLevelEffect(sys, x.level + 1)), r.x + 16, r.y + 78, 13, C_GOOD);
            else
                Text("Niveau maximal atteint", r.x + 16, r.y + 78, 13, C_TEXT_DIM);
        } else {
            Text("Non installé — disponible en magasin", r.x + 16, r.y + 58, 13, C_TEXT_DIM);
            Text(TextFormat("Installation : %d ferraille", SystemInstallCost(sys)), r.x + 16, r.y + 78, 13, C_TEXT_DIM);
        }

        if (installed) {
            int cost = SystemUpgradeCost(sys, x.level);
            bool can = !maxed && s.scrap >= cost;
            const char* label = maxed ? "MAX" : TextFormat("+1  %d", cost);
            if (Button({ r.x + r.width - 118, r.y + 36, 106, 32 }, label, can, C_ACCENT, 20)) {
                s.scrap -= cost;
                s.UpgradeSystem(sys);
                s.AutoPowerNew();
                upgraded(std::string(SystemName(sys)) + TextFormat(" niveau %d", s.sys[sys].level));
            }
        }
    }
    _hovered = hovered;

    // ── Description du module survolé ────────────────────────────────────────
    float by = PANEL.y + PANEL.height - 58;
    DrawRectangleRec({ PANEL.x + 20, by - 12, PANEL.width - 40, 1 }, C_BORDER);
    const char* info = _hovered >= 0 ? SystemDescription(_hovered)
                                     : "Survolez un module pour voir sa description. L'énergie libre est assignée automatiquement.";
    TextWrapped(info, { PANEL.x + 28, by, PANEL.width - 280, 0 }, 14, _hovered >= 0 ? C_TEXT : C_TEXT_DIM);

    if (Button({ PANEL.x + PANEL.width - 220, by, 196, 42 }, "FERMER [U]", true, C_SHIELD, 20))
        _closeRequested = true;
}
