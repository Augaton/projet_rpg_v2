// Rendu des écrans et surcouches : menu principal, balise, saut, événements, fin de partie,
// pause, paramètres, choix du vaisseau, archives, tutoriel, aide.
// La boucle, le flux de jeu et la composition de l'image (_Render) sont dans Game.cpp.
#include "Game.hpp"
#include "../core/Config.hpp"
#include "../core/Random.hpp"
#include "../core/UI.hpp"
#include "../ship/ShipFactory.hpp"
#include "../ship/Weapons.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace ui;

// ─── Menu principal ──────────────────────────────────────────────────────────

void Game::_DrawMenu() {
    // Logo avec aberration chromatique lors d'un glitch périodique
    const float tx = 72.0f, ty = 64.0f;
    if (std::fmod(_t, 3.7f) < 0.12f) {
        float jx = rng::Float(-8.0f, 8.0f);
        BeginBlendMode(BLEND_ADDITIVE);
            DrawLogo(tx - 5 + jx, ty, 6.0f, ColorAlpha(C_ENEMY, 0.7f));
            DrawLogo(tx + 5 - jx, ty, 6.0f, ColorAlpha(C_SHIELD, 0.7f));
        EndBlendMode();
    }
    Vector2 logo = DrawLogo(tx, ty, 6.0f);

    Hazard({ tx + 4, ty + logo.y + 12, logo.x - 8, 8 }, C_ACCENT, _t * 30.0f);
    Label("SURVIVEZ. SEMEZ LA FLOTTE. ABATTEZ L'AMIRAL.", tx + 4, ty + logo.y + 34, 20, C_ACCENT);

    // Partie en cours, lue dans la sauvegarde
    if (_hasSave) {
        Rectangle card = { VIRTUAL_W - 72.0f - 300.0f, 64, 300, 124 };
        Console(card, C_SHIELD, "PARTIE EN COURS", C_SHIELD);
        const SaveSummary& s = _saveInfo;
        float pct = (float)s.hull / (float)std::max(1, s.maxHull);
        struct Cell { const char* label; const char* value; Color color; };
        const Cell cells[4] = {
            { "coque",     TextFormat("%d/%d", s.hull, s.maxHull), pct > 0.5f ? C_GOOD : pct > 0.25f ? C_WARNING : C_HULL_LOW },
            { "ferraille", TextFormat("%d", s.scrap),               C_SCRAP },
            { "carburant", TextFormat("%d", s.fuel),                C_TEXT },
            { "secteur",   TextFormat("%d / %d", s.sector + 1, SECTOR_COUNT), C_TEXT },
        };
        for (int i = 0; i < 4; i++) {
            float cx = card.x + 20 + (i % 2) * 140.0f, cy = card.y + 22 + (i / 2) * 50.0f;
            Text(cells[i].label, cx, cy, 13, C_TEXT_DIM);
            Label(cells[i].value, cx, cy + 18, 20, cells[i].color);
        }
    }

    // Menu
    const float bx = tx, bw = 400, bh = 46;
    const float step = 54;
    float by = ty + logo.y + 78;
    if (_hasSave) {
        if (Button({ bx, by, bw, bh }, "CONTINUER", true, C_GOOD, 20)) _Defer([this] { _ContinueRun(); });
        by += step;
    }
    if (Button({ bx, by, bw, bh }, "NOUVELLE PARTIE", true, _hasSave ? C_ACCENT : C_GOOD, 20))
        _Defer([this] { _setupOpen = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "ARCHIVES", true, C_SCRAP, 20))        _Defer([this] { _archivesOpen = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "PARAMÈTRES", true, C_ACCENT, 20))     _Defer([this] { _settingsOpen = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "COMMENT JOUER", true, C_ACCENT, 20))  _Defer([this] { _help = true; });
    by += step;
    if (Button({ bx, by, bw, bh }, "QUITTER", true, C_ENEMY, 20))         _Defer([this] { _quit = true; });

    if (!_saveError.empty())
        Text(("Sauvegarde ignorée : " + _saveError).c_str(), bx, by + 60, 14, C_HULL_LOW);

    // Pied de page
    DrawRectangleRec({ 0, VIRTUAL_H - 30.0f, (float)VIRTUAL_W, 30 }, C_ACCENT);
    Label(TextFormat("%s  //  FLÈCHES + ENTRÉE  //  F1 AIDE  //  F11 PLEIN ÉCRAN  //  F12 CAPTURE", VERSION_TAG),
          16, VIRTUAL_H - 20.0f, 10, C_INK);
    bool blink = std::fmod(_t, 1.0f) < 0.5f;
    LabelRight(blink ? "PRÊT_" : "PRÊT", VIRTUAL_W - 16.0f, VIRTUAL_H - 20.0f, 10, C_INK);
}

// ─── Balise ──────────────────────────────────────────────────────────────────

void Game::_DrawBeaconUI() {
    const Beacon& b = _map.Current();
    Rectangle info = { 686, 12, 270, 62 };
    Console(info, C_SHIELD, TextFormat("SECTEUR %d // BALISE", _map.Sector() + 1), C_SHIELD);
    Label(SectorMap::KindName(b.kind), info.x + 16, info.y + 14, 20, C_TEXT);
    int toExit = _map.JumpsToExit(_map.CurrentIdx());
    if (toExit > 0)
        Label(TextFormat("SORTIE A %d SAUT%s", toExit, toExit > 1 ? "S" : ""), info.x + 16, info.y + 40, 10, C_TEXT_DIM);

    Rectangle r = { info.x, 86, info.width, 38 };
    if (Button(r, "VAISSEAU [U]", true, C_SHIELD, 20))
        _Defer([this] { _shipMenu.Open(); });
    r.y += 48;
    if (b.kind == BeaconKind::STORE) {
        if (Button(r, "MAGASIN", true, C_GOOD, 20))
            _Defer([this] {
                _store.Generate(_map.Sector(), _map.Sector() * 100 + _map.CurrentIdx());
                _store.Open();
                _state = GameState::STORE;
            });
        r.y += 48;
    }
    if (b.kind == BeaconKind::EXIT) {
        if (Button(r, "SECTEUR SUIVANT", true, C_WARNING, 20))
            _Defer([this] { _BeginSectorJump(); });
        r.y += 48;
    }
    if (b.kind == BeaconKind::BOSS) {
        if (Button(r, "ATTAQUER L'AMIRAL", true, C_ENEMY, 20))
            _Defer([this] { _StartCombat(EnemyKind::BOSS); });
    }
}

// ─── Saut ────────────────────────────────────────────────────────────────────

void Game::_DrawJump() {
    float k = std::clamp(_jumpT / JUMP_DURATION, 0.0f, 1.0f);
    float a = std::pow(std::sin(k * PI), 3.0f) * 0.9f;
    DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(BLACK, a));

    Rectangle box = { VIRTUAL_W / 2.0f - 220, 440, 440, 74 };
    Bevel(box, 12, ColorAlpha(C_INK, 0.88f));
    BevelLines(box, 12, C_SHIELD, 2.0f);
    Hazard({ box.x + 14, box.y + 4, box.width - 28, 4 }, ColorAlpha(C_SHIELD, 0.5f), _t * 80.0f);
    LabelCentered("SAUT FTL", box.x + box.width / 2, box.y + 16, 20, C_SHIELD);
    SegBar({ box.x + 24, box.y + 44, box.width - 48, 14 }, k, 20, C_SHIELD, ColorAlpha(C_SHIELD, 0.15f));
}

// ─── Événement ───────────────────────────────────────────────────────────────

void Game::_DrawEvent() {
    const float W  = 720.0f;
    const float x  = (VIRTUAL_W - W) / 2.0f;
    const float tw = W - 60.0f;
    const float bodySize = 18.0f;

    int  len      = _EventTextLength();
    bool revealed = _typed >= (float)len;
    int  shown    = revealed ? -1 : (int)_typed;

    // Hauteur calculée sur le texte complet : le panneau ne bouge pas pendant la frappe
    float h = 86.0f;
    if (!_eventBody.empty()) h += TextWrapped(_eventBody.c_str(), { 0, 0, tw, 0 }, bodySize, WHITE, false, false) + 14;
    if (_eventResolved) {
        if (!_resultText.empty()) h += TextWrapped(_resultText.c_str(), { 0, 0, tw, 0 }, bodySize, WHITE, false, false) + 12;
        h += (float)_resultLines.size() * 26.0f + 70.0f;
    } else if (_event) {
        h += (float)_event->choices.size() * 52.0f + 8.0f;
    }

    float ease = 1.0f - std::pow(1.0f - std::clamp(_eventAnim, 0.0f, 1.0f), 3.0f);
    float y = std::max(40.0f, 330.0f - h / 2.0f) + (1.0f - ease) * 24.0f;
    Dim(0.4f * ease);
    Panel({ x, y, W, h }, C_ACCENT, 0.3f, C_PANEL_BG, "TRANSMISSION ENTRANTE");

    Label(_eventTitle.c_str(), x + 30, y + 26, 30, C_TEXT);
    DrawRectangleRec({ x + 30, y + 62, tw, 2 }, C_BORDER);
    float cy = y + 76;

    if (!_eventBody.empty()) {
        Color bodyCol = _eventResolved ? C_TEXT_DIM : C_LABEL;
        cy += TextWrapped(_eventBody.c_str(), { x + 30, cy, tw, 0 }, bodySize, bodyCol,
                          false, true, _eventResolved ? -1 : shown) + 14;
    }

    bool cursor = std::fmod(_t, 0.6f) < 0.3f;

    if (!_eventResolved && _event) {
        if (!revealed) {
            if (cursor) Label("_", x + W - 44, y + h - 34, 20, C_ACCENT);
            return;
        }
        for (int i = 0; i < (int)_event->choices.size(); i++) {
            const Choice& c = _event->choices[i];
            bool ok = ChoiceAvailable(c, _player);
            std::string label = std::to_string(i + 1) + ". " + c.label;
            std::string req = ChoiceRequirement(c);
            if (!req.empty()) label += "  " + req;

            Color accent = c.success.combat ? C_ENEMY : (c.chance < 1.0f ? C_WARNING : C_ACCENT);
            if (ButtonText({ x + 30, cy, tw, 42 }, label.c_str(), ok, accent, 17))
                _Defer([this, i] { _ChooseOption(i); });
            cy += 52;
        }
    } else {
        if (!_resultText.empty())
            cy += TextWrapped(_resultText.c_str(), { x + 30, cy, tw, 0 }, bodySize, C_TEXT, false, true, shown) + 12;
        if (!revealed) {
            if (cursor) Label("_", x + W - 44, y + h - 34, 20, C_ACCENT);
            return;
        }

        for (const auto& line : _resultLines) {
            if (line.empty()) continue;
            Color c = line[0] == '+' ? C_GOOD : line[0] == '-' ? C_HULL_LOW : C_SCRAP;
            DrawRectangleRec({ x + 30, cy + 2, 8, 16 }, c);
            Text(line.c_str(), x + 48, cy, 18, c);
            cy += 26;
        }
        cy += 14;
        if (_OutcomeIsPlain()) {
            if (Button({ x + W / 2 - 250, cy, 240, 42 }, "CARTE [J]", true, C_SHIELD, 20))
                _Defer([this] { _ContinueToMap(); });
            if (Button({ x + W / 2 + 10, cy, 240, 42 }, "RESTER [ESPACE]", true, C_ACCENT, 20))
                _Defer([this] { _ContinueAfterEvent(); });
        } else {
            const char* label = _pendingOutcome.combat ? "COMBAT ! [ESPACE]"
                              : _pendingOutcome.store  ? "COMMERCER [ESPACE]"
                                                       : "SAUTER [ESPACE]";
            if (Button({ x + W / 2 - 130, cy, 260, 42 }, label, true, _pendingOutcome.combat ? C_ENEMY : C_ACCENT, 20))
                _Defer([this] { _ContinueAfterEvent(); });
        }
    }
}

// ─── Fin de partie ───────────────────────────────────────────────────────────

void Game::_DrawEndScreen(bool victory) {
    Dim(victory ? 0.4f : 0.6f);

    Rectangle r = { 370, 100, 540, 520 };
    Color accent = victory ? C_GOOD : C_HULL_LOW;
    Panel(r, accent, 0.4f, C_PANEL_BG, victory ? "MISSION ACCOMPLIE" : "SIGNAL PERDU");

    Hazard({ r.x + 16, r.y + 3, r.width - 19, 14 }, ColorAlpha(accent, 0.5f), _t * 20.0f);
    LabelCentered(victory ? "VICTOIRE" : "VAISSEAU DÉTRUIT", r.x + r.width / 2, r.y + 36, 40, accent);

    float y = r.y + 86;
    LabelCentered(TextFormat("%s  //  %s", GetPlayerShipInfo(_player.shipType).name, GetDifficulty(_difficulty).name),
                  r.x + r.width / 2, y, 10, C_TEXT_DIM);
    y += 20;
    y += TextWrapped(_endReason.c_str(), { r.x + 36, y, r.width - 72, 0 }, 16, C_LABEL, true) + 14;

    struct Line { const char* label; int value; };
    Line lines[] = {
        { "SECTEUR ATTEINT",     _map.Sector() + 1 },
        { "SAUTS",               _stats.jumps },
        { "ENNEMIS DÉTRUITS",    _stats.kills },
        { "ÉVÉNEMENTS",          _stats.events },
        { "FERRAILLE RÉCOLTÉE",  _stats.scrap },
    };
    for (const auto& l : lines) {
        Label(l.label, r.x + 60, y, 20, C_TEXT_DIM);
        LabelRight(TextFormat("%d", l.value), r.x + r.width - 60, y, 20, C_TEXT);
        y += 30;
    }

    for (int id : _newUnlocks) {
        bool blink = std::fmod(_t, 0.8f) < 0.5f;
        const char* msg = TextFormat("NOUVEAU VAISSEAU : %s", GetPlayerShipInfo(id).name);
        float w = MeasureLabel(msg, 20) + 24;
        DrawRectangleRec({ r.x + (r.width - w) / 2, y + 4, w, 28 }, blink ? C_SCRAP : ColorAlpha(C_SCRAP, 0.3f));
        LabelCentered(msg, r.x + r.width / 2, y + 8, 20, blink ? C_INK : C_SCRAP);
        y += 34;
    }

    float by = r.y + r.height - 70;
    if (Button({ r.x + 40, by, 210, 46 }, "REJOUER", true, C_GOOD, 20))
        _Defer([this] { _NewGame(); });
    if (Button({ r.x + r.width - 250, by, 210, 46 }, "MENU", true, C_ACCENT, 20))
        _Defer([this] { _ReturnToMenu(); });
}

// ─── Surcouches ──────────────────────────────────────────────────────────────

void Game::_DrawPauseMenu() {
    Dim(0.65f);
    Rectangle r = { 470, 170, 340, 380 };
    Panel(r, C_ACCENT, 0.3f, C_PANEL_BG, "SYSTÈME EN ATTENTE");
    LabelCentered("PAUSE", r.x + r.width / 2, r.y + 28, 40, C_ACCENT);

    float bx = r.x + 30, bw = r.width - 60, by = r.y + 96;
    if (Button({ bx, by, bw, 44 }, "REPRENDRE", true, C_GOOD, 20))          _Defer([this] { _pause = false; });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "PARAMÈTRES", true, C_ACCENT, 20))       _Defer([this] { _settingsOpen = true; });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "COMMANDES", true, C_ACCENT, 20))        _Defer([this] { _help = true; });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "MENU PRINCIPAL", true, C_WARNING, 20))  _Defer([this] { _ReturnToMenu(); });
    by += 54;
    if (Button({ bx, by, bw, 44 }, "QUITTER", true, C_ENEMY, 20))           _Defer([this] { _quit = true; });

    Text("Progression sauvegardée à chaque balise.", r.x + 30, r.y + r.height - 26, 13, C_TEXT_DIM);
}

void Game::_DrawSettings() {
    Dim(0.7f);
    Rectangle r = { 250, 40, 780, 640 };
    Panel(r, C_ACCENT, 0.3f, C_PANEL_BG, "CONFIGURATION");
    Label("PARAMÈTRES", r.x + 30, r.y + 26, 30, C_TEXT);

    auto pct   = [](float v) { return TextFormat("%d%%", (int)std::lround(v * 100.0f)); };
    auto yesno = [](bool v) { return v ? "OUI" : "NON"; };
    auto step  = [](float v, int d) { return std::clamp(std::round((v + 0.1f * d) * 10.0f) / 10.0f, 0.0f, 1.0f); };

    bool changed = false;
    float x = r.x + 30, w = r.width - 60, y = r.y + 76;
    const float H = 32, GAP = 3;
    int d;

    if ((d = Stepper({ x, y, w, H }, "VOLUME GÉNÉRAL", pct(_settings.masterVolume)))) { _settings.masterVolume = step(_settings.masterVolume, d); changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "MUSIQUE", pct(_settings.musicVolume))))          { _settings.musicVolume = step(_settings.musicVolume, d); changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "EFFETS SONORES", pct(_settings.sfxVolume))))     { _settings.sfxVolume = step(_settings.sfxVolume, d); changed = true; }
    y += H + GAP + 8;

    if ((d = Stepper({ x, y, w, H }, "PLEIN ÉCRAN", yesno(_settings.fullscreen))))     { _settings.fullscreen = !_settings.fullscreen; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "SYNCHRO VERTICALE", yesno(_settings.vsync))))    { _settings.vsync = !_settings.vsync; changed = true; }
    y += H + GAP;
    {
        static const int CAPS[] = { 30, 60, 120, 144, 0 };
        int idx = 1;
        for (int i = 0; i < 5; i++) if (CAPS[i] == _settings.fpsCap) idx = i;
        const char* label = _settings.fpsCap != 0 ? TextFormat("%d", _settings.fpsCap)
                          : _settings.vsync       ? "ÉCRAN" : "ILLIMITÉ";
        if ((d = Stepper({ x, y, w, H }, "LIMITE D'IMAGES/S", label))) {
            _settings.fpsCap = CAPS[(idx + d + 5) % 5];
            changed = true;
        }
    }
    y += H + GAP + 8;

    static const char* CRT[] = { "AUCUN", "LÉGER", "FORT" };
    if ((d = Stepper({ x, y, w, H }, "EFFET CATHODIQUE", CRT[std::clamp(_settings.crt, 0, 2)]))) {
        _settings.crt = (_settings.crt + d + 3) % 3; changed = true;
    }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "TREMBLEMENTS", pct(_settings.shake)))) {
        _settings.shake = std::clamp(_settings.shake + 0.5f * d, 0.0f, 1.0f); changed = true;
    }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "RÉDUIRE LES FLASHS", yesno(_settings.reduceFlashes)))) { _settings.reduceFlashes = !_settings.reduceFlashes; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "PAUSE HORS FOCUS", yesno(_settings.pauseOnFocus))))    { _settings.pauseOnFocus = !_settings.pauseOnFocus; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "PAUSE AUTO EN COMBAT", yesno(_settings.autoPause))))   { _settings.autoPause = !_settings.autoPause; changed = true; }
    y += H + GAP;
    if ((d = Stepper({ x, y, w, H }, "TUTORIEL", yesno(_settings.tutorial)))) {
        _settings.tutorial = !_settings.tutorial;
        if (_settings.tutorial) _tutStep = 0;
        changed = true;
    }
    y += H + GAP;
    static const char* SPEED[] = { "INSTANTANÉ", "NORMAL", "RAPIDE" };
    if ((d = Stepper({ x, y, w, H }, "VITESSE DU TEXTE", SPEED[std::clamp(_settings.textSpeed, 0, 2)]))) {
        _settings.textSpeed = (_settings.textSpeed + d + 3) % 3; changed = true;
    }

    if (changed) _Defer([this] { _ApplySettings(); });

    Text("Flèches haut/bas : choisir   gauche/droite : modifier", r.x + 30, r.y + r.height - 44, 14, C_TEXT_DIM);
    if (Button({ r.x + r.width - 210, r.y + r.height - 60, 180, 42 }, "RETOUR", true, C_ACCENT, 20))
        _Defer([this] { _settingsOpen = false; _settings.Save(); });
}

void Game::_LaunchFromSetup() {
    if (_hasSave) _confirmNew = true;
    else          { _setupOpen = false; _NewGame(); }
}

void Game::_DrawSetup() {
    Dim(0.75f);
    Rectangle p = { 120, 40, 1040, 640 };
    Panel(p, C_GOOD, 0.3f, C_PANEL_BG, "NOUVELLE PARTIE");
    Label("CHOISISSEZ VOTRE VAISSEAU", p.x + 24, p.y + 22, 20, C_TEXT);

    static Ship previews[PLAYER_SHIP_COUNT];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < PLAYER_SHIP_COUNT; i++) previews[i] = MakePlayerShip(i);
        built = true;
    }

    const float cw = 326, ch = 400, gap = 11;
    for (int i = 0; i < PLAYER_SHIP_COUNT; i++) {
        Rectangle c = { p.x + 20 + i * (cw + gap), p.y + 56, cw, ch };
        const Ship& ship = previews[i];
        const PlayerShipInfo& info = GetPlayerShipInfo(i);
        bool unlocked = _profile.unlocked[i];
        bool selected = _selectedShip == i;
        Color accent  = !unlocked ? C_BORDER : selected ? C_GOOD : C_ACCENT;

        if (unlocked && Hover(c) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) _Defer([this, i] { _selectedShip = i; });

        Bevel(c, 14, ColorAlpha(selected ? Color{ 10, 30, 16, 255 } : C_INK, unlocked ? 0.7f : 0.4f));
        BevelLines(c, 14, accent, selected ? 2.0f : 1.0f);
        if (selected) DrawRectangleRec({ c.x + 14, c.y, c.width - 14, 4 }, C_GOOD);

        Label(info.name, c.x + 16, c.y + 16, 30, unlocked ? C_TEXT : C_TEXT_DIM);
        Text(info.role, c.x + 18, c.y + 52, 14, unlocked ? accent : C_TEXT_DIM);

        // Aperçu du vaisseau sur un halo
        Vector2 center = { c.x + c.width / 2, c.y + 138 };
        if (unlocked)
            DrawCircleGradient((int)center.x, (int)center.y, 82.0f, ColorAlpha(accent, selected ? 0.22f : 0.10f), BLANK);
        ShipView view;
        view.pos       = center;
        view.rotation  = 90.0f + std::sin(_t * 0.8f + i) * 3.0f;
        view.scaleMult = 140.0f / _renderer.Size(ship);
        view.alpha     = unlocked ? 1.0f : 0.25f;
        _renderer.DrawShip(ship, view, _t);

        if (!unlocked) {
            LabelCentered("VERROUILLÉ", c.x + c.width / 2, c.y + 130, 20, C_WARNING);
            TextWrapped(info.unlockHint, { c.x + 20, c.y + 220, c.width - 40, 0 }, 15, C_LABEL, true);
            continue;
        }

        // Caractéristiques
        float y = c.y + 206;
        Text(TextFormat("Coque %d   ·   Réacteur %d   ·   Missiles %d", ship.maxHull, ship.reactor, ship.missiles),
             c.x + 18, y, 13, C_TEXT);
        y += 20;
        // Lignes composées dans des tampons fixes (écran redessiné à chaque image)
        char mods[128] = "", arms[160] = "";
        size_t mu = 0, au = 0;
        for (int sidx = 0; sidx < SYS_COUNT && mu < sizeof mods; sidx++) {
            if (!ship.sys[sidx].Installed()) continue;
            int n = std::snprintf(mods + mu, sizeof mods - mu, "%s%d ", SystemShortName(sidx), ship.sys[sidx].level);
            if (n < 0) break;
            mu = std::min(sizeof mods - 1, mu + (size_t)n);
        }
        Text(mods, c.x + 18, y, 13, C_LABEL);
        y += 20;
        for (const auto& w : ship.weapons) {
            if (w.Empty() || au >= sizeof arms) continue;
            int n = std::snprintf(arms + au, sizeof arms - au, "%s%s", au == 0 ? "" : " + ", GetWeaponDef(w.id).name);
            if (n < 0) break;
            au = std::min(sizeof arms - 1, au + (size_t)n);
        }
        Text(arms, c.x + 18, y, 13, C_ENEMY);
        y += 20;
        for (int a : ship.augments)
            if (a >= 0) { Text(GetAugDef(a).name, c.x + 18, y, 13, C_SCRAP); y += 20; }
        TextWrapped(info.description, { c.x + 18, y + 6, c.width - 36, 0 }, 13, C_TEXT_DIM);

        if (Button({ c.x + 20, c.y + ch - 50, c.width - 40, 36 }, selected ? "SÉLECTIONNÉ" : "CHOISIR", !selected, C_GOOD, 20))
            _Defer([this, i] { _selectedShip = i; });
    }

    // Difficulté
    float y = p.y + 56 + ch + 20;
    int d = Stepper({ p.x + 20, y, 520, 40 }, "DIFFICULTÉ", GetDifficulty(_difficulty).name, C_ACCENT);
    if (d != 0) _difficulty = (_difficulty + d + DIFFICULTY_COUNT) % DIFFICULTY_COUNT;
    static const char* DIFF_DESC[DIFFICULTY_COUNT] = {
        "Ennemis moins résistants et plus lents, +25 % de ferraille, flotte ralentie.",
        "L'expérience prévue.",
        "Ennemis blindés et agressifs, moins de ferraille, flotte plus rapide.",
    };
    Text(DIFF_DESC[_difficulty], p.x + 24, y + 50, 14, C_TEXT_DIM);

    float by = p.y + p.height - 62;
    if (Button({ p.x + p.width - 440, by, 200, 44 }, "RETOUR", true, C_ACCENT, 20))
        _Defer([this] { _setupOpen = false; });
    if (Button({ p.x + p.width - 220, by, 200, 44 }, "LANCER", _profile.unlocked[_selectedShip], C_GOOD, 20))
        _Defer([this] { _LaunchFromSetup(); });
}

void Game::_DrawArchives() {
    Dim(0.75f);
    Rectangle p = { 170, 40, 940, 640 };
    Panel(p, C_SCRAP, 0.3f, C_PANEL_BG, "ARCHIVES");
    Label("ARCHIVES DE VOL", p.x + 24, p.y + 22, 30, C_TEXT);

    struct Stat { const char* label; int value; };
    Stat stats[] = {
        { "PARTIES",           _profile.runs },
        { "VICTOIRES",         _profile.wins },
        { "MEILLEUR SECTEUR",  _profile.bestSector },
        { "ENNEMIS DÉTRUITS",  _profile.totalKills },
        { "FERRAILLE TOTALE",  _profile.totalScrap },
    };
    float x = p.x + 24, y = p.y + 76;
    for (const auto& st : stats) {
        Rectangle r = { x, y, 170, 64 };
        DrawRectangleRec(r, ColorAlpha(C_INK, 0.5f));
        DrawRectangleRec({ r.x, r.y, 3, r.height }, C_SCRAP);
        Label(st.label, r.x + 12, r.y + 10, 10, C_TEXT_DIM);
        Label(TextFormat("%d", st.value), r.x + 12, r.y + 28, 30, C_TEXT);
        x += 180;
    }

    // Vaisseaux
    y += 90;
    Label("VAISSEAUX", p.x + 24, y, 10, C_ACCENT);
    y += 18;
    for (int i = 0; i < PLAYER_SHIP_COUNT; i++) {
        const PlayerShipInfo& info = GetPlayerShipInfo(i);
        bool ok = _profile.unlocked[i];
        DrawRectangleRec({ p.x + 24, y, p.width - 48, 30 }, ColorAlpha(C_INK, 0.4f));
        Label(info.name, p.x + 36, y + 8, 20, ok ? C_TEXT : C_TEXT_DIM);
        Text(info.role, p.x + 200, y + 7, 14, C_LABEL);
        TextRight(ok ? "Débloqué" : info.unlockHint, p.x + p.width - 36, y + 7, 14, ok ? C_GOOD : C_WARNING);
        y += 36;
    }

    // Historique
    y += 16;
    Label("DERNIÈRES PARTIES", p.x + 24, y, 10, C_ACCENT);
    y += 20;
    const float cols[] = { 36, 200, 360, 480, 620, 740 };
    const char* heads[] = { "VAISSEAU", "DIFFICULTÉ", "SECTEUR", "RÉSULTAT", "ENNEMIS", "FERRAILLE" };
    for (int k = 0; k < 6; k++) Label(heads[k], p.x + cols[k], y, 10, C_TEXT_DIM);
    y += 18;
    if (_profile.history.empty())
        Text("Aucune partie terminée pour l'instant.", p.x + 36, y + 4, 14, C_TEXT_DIM);
    for (const auto& h : _profile.history) {
        Color c = h.won ? C_GOOD : C_LABEL;
        Text(GetPlayerShipInfo(h.ship).name,            p.x + cols[0], y, 14, c);
        Text(GetDifficulty(h.difficulty).name,          p.x + cols[1], y, 14, c);
        Text(TextFormat("%d / %d", h.sector, SECTOR_COUNT), p.x + cols[2], y, 14, c);
        Text(h.won ? "VICTOIRE" : "DÉTRUIT",            p.x + cols[3], y, 14, h.won ? C_GOOD : C_HULL_LOW);
        Text(TextFormat("%d", h.kills),                 p.x + cols[4], y, 14, c);
        Text(TextFormat("%d", h.scrap),                 p.x + cols[5], y, 14, c);
        y += 22;
    }

    if (Button({ p.x + p.width - 220, p.y + p.height - 62, 196, 44 }, "FERMER", true, C_SCRAP, 20))
        _Defer([this] { _archivesOpen = false; });
}

// ─── Tutoriel ────────────────────────────────────────────────────────────────

namespace {
struct TutorialStep {
    const char* title;
    const char* text;
    Vector2     pointer;   // élément désigné (coordonnées virtuelles)
};
const TutorialStep TUTORIAL[] = {
    { "ÉNERGIE",
      "Vos armes (en bas) se chargent quand elles sont alimentées. Les colonnes en bas à gauche répartissent "
      "l'énergie du réacteur : clic gauche pour ajouter, clic droit pour retirer.",
      { 140, 648 } },
    { "VISER",
      "Cliquez sur une salle du vaisseau ennemi pour y envoyer toutes vos armes. [T] choisit les cibles "
      "automatiquement : lasers sur les boucliers, missiles sur les armes.",
      { 940, 350 } },
    { "PAUSE ET VITESSE",
      "[ESPACE] fige le combat pour donner vos ordres calmement. [TAB] accélère le temps quand tout est sous contrôle.",
      { 760, 30 } },
    { "DÉFENSE",
      "Les couches de bouclier bloquent les lasers, pas les missiles. Les salles encadrées en rouge sur votre "
      "vaisseau sont visées par l'ennemi : anticipez !",
      { 330, 360 } },
};
constexpr int TUTORIAL_STEPS = 4;
}

void Game::_UpdateTutorial(float dt) {
    if (!_settings.tutorial || _tutStep >= TUTORIAL_STEPS || _state != GameState::COMBAT) return;
    _tutTimer += dt;
    if (_combat.Paused()) _tutPausedOnce = true;

    bool done = false;
    switch (_tutStep) {
    case 0: done = _tutTimer > 8.0f; break;
    case 1:
        for (const auto& w : _player.weapons) if (!w.Empty() && w.target >= 0) done = true;
        break;
    case 2: done = _tutPausedOnce && !_combat.Paused(); break;
    case 3: done = _tutTimer > 9.0f; break;
    }
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) done = true;

    if (done) {
        _tutStep++;
        _tutTimer = 0.0f;
        _audio.PlaySfx("sx_ready", 1.3f, 0.4f);
        if (_tutStep >= TUTORIAL_STEPS) {
            _settings.tutorial = false;
            _settings.Save();
            _toast  = "Tutoriel terminé. [J] ouvre la carte, [U] le menu VAISSEAU. Réactivable dans les paramètres.";
            _toastT = 5.0f;
        }
    }
}

void Game::_DrawTutorial() {
    if (!_settings.tutorial || _tutStep >= TUTORIAL_STEPS || _combat.Dying()) return;
    const TutorialStep& st = TUTORIAL[_tutStep];

    const float W = 620.0f;
    float h = TextWrapped(st.text, { 0, 0, W - 40, 0 }, 15, WHITE, false, false) + 70;
    Rectangle r = { (VIRTUAL_W - W) / 2.0f, 440, W, h };

    // Flèche vers l'élément concerné
    float pulse = 0.5f + 0.5f * std::sin(_t * 6.0f);
    Vector2 from = { r.x + r.width / 2, st.pointer.y < r.y ? r.y : r.y + r.height };
    DrawLineEx(from, st.pointer, 2.0f, ColorAlpha(C_GOOD, 0.4f + 0.4f * pulse));
    float s = 16.0f + 6.0f * pulse;
    CornerBrackets({ st.pointer.x - s, st.pointer.y - s, s * 2, s * 2 }, 8.0f, C_GOOD);
    DrawRectangleRec({ st.pointer.x - 2, st.pointer.y - 2, 4, 4 }, C_GOOD);

    Bevel({ r.x + 4, r.y + 4, r.width, r.height }, 12, ColorAlpha(BLACK, 0.4f));
    Bevel(r, 12, ColorAlpha(C_INK, 0.92f));
    Bevel({ r.x, r.y, r.width, 22 }, 12, C_GOOD);
    BevelLines(r, 12, C_GOOD, 2.0f);
    Label(TextFormat("TUTORIEL %d/%d - %s", _tutStep + 1, TUTORIAL_STEPS, st.title), r.x + 16, r.y + 6, 10, C_INK);
    TextWrapped(st.text, { r.x + 20, r.y + 32, W - 40, 0 }, 15, C_TEXT);
    const char* hint = _tutStep == 1 ? "Assignez une cible pour continuer"
                     : _tutStep == 2 ? "Appuyez sur ESPACE deux fois pour continuer"
                                     : "Continue automatiquement";
    Text(hint, r.x + 20, r.y + h - 26, 13, C_GOOD);
    TextRight("[ENTRÉE] étape suivante", r.x + W - 20, r.y + h - 26, 13, C_TEXT_DIM);
}

void Game::_DrawConfirm() {
    Dim(0.7f);
    Rectangle r = { 390, 250, 500, 220 };
    Panel(r, C_WARNING, 0.4f, C_PANEL_BG, "CONFIRMATION");
    LabelCentered("NOUVELLE PARTIE ?", r.x + r.width / 2, r.y + 30, 30, C_WARNING);
    TextWrapped("La partie en cours sera définitivement effacée.", { r.x + 40, r.y + 84, r.width - 80, 0 },
                17, C_LABEL, true);
    if (Button({ r.x + 40, r.y + 150, 200, 44 }, "ANNULER", true, C_ACCENT, 20))
        _Defer([this] { _confirmNew = false; });
    if (Button({ r.x + r.width - 240, r.y + 150, 200, 44 }, "EFFACER", true, C_ENEMY, 20))
        _Defer([this] { _confirmNew = false; _setupOpen = false; _NewGame(); });
}

void Game::_DrawHelp() {
    Dim(0.75f);
    Rectangle r = { 180, 20, 920, 680 };
    Panel(r, C_ACCENT, 0.3f, C_PANEL_BG, "MANUEL DE BORD");
    Label("COMMENT JOUER", r.x + 30, r.y + 22, 30, C_TEXT);

    struct Entry { const char* key; const char* desc; };
    const Entry controls[] = {
        { "CLIC G / MOLETTE+", "Ajouter de l'énergie à un système" },
        { "CLIC D / MOLETTE-", "Retirer de l'énergie" },
        { "CLIC G SUR ARME",   "Allumer l'arme, puis la sélectionner" },
        { "CLIC SALLE ENNEMIE","Assigner la cible de l'arme sélectionnée" },
        { "CLIC D SUR ARME",   "Éteindre l'arme" },
        { "GLISSER UNE ARME",  "Changer d'emplacement / viser une salle ennemie" },
        { "MAJ + CLIC G / D",  "Système au maximum / coupé, toutes les armes" },
        { "1 - 4",             "Sélectionner / allumer une arme" },
        { "ESPACE",            "Pause tactique en combat / continuer" },
        { "TAB",               "Vitesse du combat x1 / x2 / x4" },
        { "T / CLIC SALLE",    "Ciblage auto / toutes les armes sur la salle" },
        { "C / H",             "Invisibilité / piratage (modules achetés)" },
        { "U",                 "Menu VAISSEAU : modules et augmentations" },
        { "J",                 "Carte du secteur, saut FTL d'urgence" },
        { "1 - 9 (CARTE)",     "Choisir une balise, ENTRÉE ou même chiffre : sauter" },
        { "MAJ + CLIC",        "Acheter par 5 au magasin" },
        { "FLÈCHES + ENTRÉE",  "Naviguer dans les menus" },
        { "ÉCHAP / F1",        "Annuler, menu pause / aide" },
        { "F11 / F12",         "Plein écran / capture d'écran" },
    };
    float y = r.y + 74;
    for (const auto& c : controls) {
        DrawRectangleRec({ r.x + 30, y - 2, 250, 21 }, ColorAlpha(C_ACCENT, 0.15f));
        Label(c.key, r.x + 38, y + 3, 10, C_ACCENT);
        Text(c.desc, r.x + 300, y, 16, C_LABEL);
        y += 23;
    }

    y += 10;
    Label("CONSEILS", r.x + 30, y, 20, C_TEXT);
    y += 28;
    TextWrapped("Les boucliers bloquent les lasers : percez-les avec des missiles ou des rafales. "
                "Toucher les ARMES ennemies les empêche de tirer, toucher les MOTEURS réduit leur esquive. "
                "Chaque saut fait avancer la flotte rebelle. Les systèmes sont réparés après le combat, "
                "pas la coque : utilisez les magasins.",
                { r.x + 30, y, r.width - 290, 0 }, 15, C_LABEL);   // laisse la place au bouton

    if (Button({ r.x + r.width - 210, r.y + r.height - 60, 180, 42 }, "FERMER", true, C_ACCENT, 20))
        _Defer([this] { _help = false; });
}

void Game::_DrawToast() {
    if (_toastT <= 0.0f || _toast.empty()) return;
    float a = std::min(1.0f, _toastT);
    float w = Measure(_toast.c_str(), 15) + 30;
    Rectangle r = { (VIRTUAL_W - w) / 2.0f, 150, w, 30 };
    Bevel(r, 8, ColorAlpha(C_INK, 0.9f * a));
    BevelLines(r, 8, ColorAlpha(C_ACCENT, a), 2.0f);
    Text(_toast.c_str(), r.x + 15, r.y + 7, 15, ColorAlpha(C_TEXT, a));
}
