#include "galaxymap.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// ─── Helpers locaux ───────────────────────────────────────────────────────────

static float Dist(Vector2 a, Vector2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return sqrtf(dx*dx + dy*dy);
}

static float randf() { return (float)(rand() % 1000) / 1000.0f; }

// ─── Génération ───────────────────────────────────────────────────────────────

// Table de probabilités d'événements selon la menace
static NodeEvent RollEvent(NodeThreat threat) {
    // LOW  : 50% empty, 20% combat, 20% merchant, 10% derelict
    // MED  : 30% empty, 40% combat, 15% merchant, 15% derelict
    // HIGH : 15% empty, 60% combat,  5% merchant, 20% derelict
    int r = rand() % 100;
    switch (threat) {
        case NodeThreat::LOW:
            if (r < 50) return NodeEvent::EMPTY;
            if (r < 70) return NodeEvent::COMBAT;
            if (r < 90) return NodeEvent::MERCHANT;
            return NodeEvent::DERELICT;
        case NodeThreat::MEDIUM:
            if (r < 30) return NodeEvent::EMPTY;
            if (r < 70) return NodeEvent::COMBAT;
            if (r < 85) return NodeEvent::MERCHANT;
            return NodeEvent::DERELICT;
        case NodeThreat::HIGH:
            if (r < 15) return NodeEvent::EMPTY;
            if (r < 75) return NodeEvent::COMBAT;
            if (r < 80) return NodeEvent::MERCHANT;
            return NodeEvent::DERELICT;
    }
    return NodeEvent::EMPTY;
}

void GalaxyMap::_BuildNodes() {
    _nodes.clear();
    _currentNodeIdx = 0;

    const float MARGIN_X = 110.0f;
    const float MARGIN_Y = 80.0f;
    const float usableW  = _screenW - MARGIN_X * 2;
    const float usableH  = _screenH - MARGIN_Y * 2 - 60;

    // 4 colonnes : 0=départ, 1-2=zone, 3=sortie
    const int COLS = 4;
    const int nodesPerCol[] = { 1, 2 + rand()%2, 2 + rand()%2, 1 };

    int idx = 0;
    for (int col = 0; col < COLS; col++) {
        int  cnt  = nodesPerCol[col];
        bool exit = (col == COLS - 1);
        float cx  = MARGIN_X + (float)col / (COLS - 1) * usableW;

        for (int row = 0; row < cnt; row++) {
            float cy = MARGIN_Y + (row + 0.5f) / cnt * usableH;
            // Légère variation aléatoire pour l'aspect organique
            float jitter = 30.0f;
            if (col > 0 && col < COLS - 1) {
                cx = MARGIN_X + (float)col / (COLS - 1) * usableW
                     + (randf() - 0.5f) * jitter;
                cy += (randf() - 0.5f) * jitter;
            }

            MapNode node{};
            node.pos          = { cx, cy };
            node.sectorIdx    = rand() % _sectorCount;
            node.visited      = (idx == 0);
            node.isCurrentPos = (idx == 0);
            node.isExit       = exit;
            node.column       = col;
            node.threat       = exit ? NodeThreat::HIGH
                                     : (NodeThreat)(rand() % 3);
            node.event        = exit ? NodeEvent::EMPTY  // géré dans main
                                     : (col == 0 ? NodeEvent::EMPTY
                                                 : RollEvent(node.threat));
            _nodes.push_back(node);
            idx++;
        }
    }

    // Connexions gauche → droite seulement
    for (int i = 0; i < (int)_nodes.size(); i++) {
        for (int j = 0; j < (int)_nodes.size(); j++) {
            if (_nodes[j].column != _nodes[i].column + 1) continue;
            // Lier chaque nœud col N à chaque nœud col N+1 (pour les petites zones)
            auto& li = _nodes[i].links;
            auto& lj = _nodes[j].links;
            if (std::find(li.begin(), li.end(), j) == li.end()) li.push_back(j);
            if (std::find(lj.begin(), lj.end(), i) == lj.end()) lj.push_back(i);
        }
    }
}

void GalaxyMap::Generate(int screenW, int screenH,
                          const SectorDef* sectors, int sectorCount) {
    _sectors     = sectors;
    _sectorCount = sectorCount;
    _screenW     = screenW;
    _screenH     = screenH;
    _zoneLevel   = 0;
    _BuildNodes();
}

void GalaxyMap::Regenerate(int screenW, int screenH) {
    _screenW = screenW;
    _screenH = screenH;
    _zoneLevel++;
    _lastJumpEvent  = NodeEvent::EMPTY;
    _lastJumpIsExit = false;
    _BuildNodes();
    _openTimer = 0.0f;
}

void GalaxyMap::MarkCurrentVisited(int nodeIdx) {
    if (nodeIdx < 0 || nodeIdx >= (int)_nodes.size()) return;
    for (auto& n : _nodes) n.isCurrentPos = false;
    _nodes[nodeIdx].visited     = true;
    _nodes[nodeIdx].isCurrentPos = true;
    _currentNodeIdx = nodeIdx;
}

// ─── Update ───────────────────────────────────────────────────────────────────

void GalaxyMap::Update(Vector2 mousePos, float dt) {
    if (!_active) return;

    _scanTimer  += dt * 0.4f;
    _pulseTimer += dt * 2.2f;
    _openTimer   = std::min(_openTimer + dt * 4.0f, 1.0f);

    _hoveredIdx = -1;
    for (int i = 0; i < (int)_nodes.size(); i++) {
        if (CheckCollisionPointCircle(mousePos, _nodes[i].pos, 22)) {
            _hoveredIdx = i;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && _IsReachable(i)) {
                _targetSectorIdx = _nodes[i].sectorIdx;
                _lastJumpEvent   = _nodes[i].event;
                _lastJumpIsExit  = _nodes[i].isExit;
                _jumpRequested   = true;
                _active          = false;
                MarkCurrentVisited(i);
            }
            break;
        }
    }
}

bool GalaxyMap::_IsReachable(int idx) const {
    if (idx == _currentNodeIdx) return false;
    // Un nœud est accessible s'il est lié directement au nœud courant
    const auto& links = _nodes[_currentNodeIdx].links;
    return std::find(links.begin(), links.end(), idx) != links.end();
}

// ─── Couleurs ─────────────────────────────────────────────────────────────────

Color GalaxyMap::_ThreatColor(NodeThreat t) const {
    switch (t) {
        case NodeThreat::LOW:    return {  60, 200, 100, 255 };
        case NodeThreat::MEDIUM: return { 220, 170,  40, 255 };
        case NodeThreat::HIGH:   return { 220,  60,  60, 255 };
    }
    return WHITE;
}

// ─── Draw helpers ─────────────────────────────────────────────────────────────

void GalaxyMap::_DrawConnection(const MapNode& a, const MapNode& b,
                                 bool reachable) const {
    Color c = reachable
              ? Color{ 80, 160, 255, 160 }
              : Color{ 50,  60,  80, 100 };
    float thick = reachable ? 1.5f : 0.8f;
    DrawLineEx(a.pos, b.pos, thick, c);

    // Tirets lumineux sur les connexions accessibles
    if (reachable) {
        float dx = b.pos.x - a.pos.x, dy = b.pos.y - a.pos.y;
        float len = sqrtf(dx*dx + dy*dy);
        float ux = dx / len, uy = dy / len;
        // Petit triangle directionnel au milieu
        float mx = (a.pos.x + b.pos.x) * 0.5f;
        float my = (a.pos.y + b.pos.y) * 0.5f;
        Vector2 tip  = { mx + ux * 6, my + uy * 6 };
        Vector2 left = { mx - ux * 4 - uy * 3, my - uy * 4 + ux * 3 };
        Vector2 rigt = { mx - ux * 4 + uy * 3, my - uy * 4 - ux * 3 };
        DrawTriangle(tip, left, rigt, { 80, 160, 255, 140 });
    }
}

void GalaxyMap::_DrawNode(const MapNode& n, int idx, Font font) const {
    bool reachable = _IsReachable(idx);
    bool hovered   = (idx == _hoveredIdx);
    bool current   = n.isCurrentPos;

    float pulse = 0.7f + 0.3f * sinf(_pulseTimer + idx * 1.3f);

    // Exit node plus grand
    float radius = n.isExit  ? 18.0f
                 : current   ? 16.0f
                 : reachable ? 13.0f
                             : 10.0f;

    // Couleur : exit = or, sinon menace
    Color tColor  = n.isExit ? Color{ 255, 200, 60, 255 } : _ThreatColor(n.threat);

    // Halo externe
    if (reachable || current || (n.isExit && !n.visited)) {
        DrawCircleV(n.pos, radius + 12,
                    ColorAlpha(tColor, 0.12f * pulse));
        DrawCircleV(n.pos, radius + 6,
                    ColorAlpha(tColor, 0.22f * pulse));
    }

    // Fond
    Color bgCol = n.visited ? Color{ 20, 30, 50, 220 } : Color{ 10, 15, 25, 200 };
    DrawCircleV(n.pos, radius, bgCol);

    // Anneau principal
    Color ringCol = n.isExit  ? Color{ 255, 200, 60, 255 }
                  : current   ? Color{  80, 200, 255, 255 }
                  : hovered   ? Color{ 255, 220,  80, 255 }
                  : reachable ? tColor
                  : n.visited ? Color{  60,  80, 120, 180 }
                              : Color{  40,  50,  70, 120 };

    DrawCircleLinesV(n.pos, radius, ringCol);
    if (current || hovered || n.isExit)
        DrawCircleLinesV(n.pos, radius + 4,
                         ColorAlpha(ringCol, 0.4f * pulse));

    // ── Icône intérieure ──────────────────────────────────────────────────────
    if (n.isExit && !n.visited) {
        // Flèche → (sortie de zone)
        DrawLineEx({ n.pos.x - 6, n.pos.y }, { n.pos.x + 6, n.pos.y },
                   2.0f, { 255, 200, 60, 255 });
        DrawLineEx({ n.pos.x + 2, n.pos.y - 4 }, { n.pos.x + 6, n.pos.y },
                   2.0f, { 255, 200, 60, 255 });
        DrawLineEx({ n.pos.x + 2, n.pos.y + 4 }, { n.pos.x + 6, n.pos.y },
                   2.0f, { 255, 200, 60, 255 });
    } else if (current) {
        DrawLineEx({ n.pos.x - 5, n.pos.y }, { n.pos.x + 5, n.pos.y },
                   1.5f, { 80, 200, 255, 255 });
        DrawLineEx({ n.pos.x, n.pos.y - 5 }, { n.pos.x, n.pos.y + 5 },
                   1.5f, { 80, 200, 255, 255 });
        DrawCircleLinesV(n.pos, 3.5f, { 80, 200, 255, 200 });
    } else if (n.visited) {
        DrawCircleV(n.pos, 3, { 60, 100, 160, 200 });
    } else if (reachable) {
        DrawCircleV(n.pos, 3.5f * pulse, ColorAlpha(tColor, 0.9f));
    } else {
        DrawCircleV(n.pos, 2.5f, { 40, 50, 70, 150 });
    }

    // ── Label / événement ─────────────────────────────────────────────────────
    if (n.isExit) {
        const char* exitLbl = "[ EXIT ZONE ]";
        float ew = MeasureTextEx(font, exitLbl, 13, 1).x;
        DrawTextEx(font, exitLbl,
                   { n.pos.x - ew / 2, n.pos.y + radius + 6 },
                   13, 1, ColorAlpha({ 255, 200, 60, 255 }, 0.9f));
    } else {
        // Événement sous le nœud
        const char* evLabel = nullptr;
        Color       evColor = {};
        if (!n.visited) {
            switch (n.event) {
                case NodeEvent::COMBAT:   evLabel = "! COMBAT";   evColor = { 220, 70, 70, 200 }; break;
                case NodeEvent::MERCHANT: evLabel = "$ MERCHANT"; evColor = {  80, 220, 80, 200 }; break;
                case NodeEvent::DERELICT: evLabel = "? DERELICT"; evColor = { 200, 160, 60, 200 }; break;
                default: break;
            }
        }

        float ly2 = n.pos.y + radius + 6.0f;
        if (_sectors) {
            int si = n.sectorIdx % _sectorCount;
            const char* name = _sectors[si].name;
            float fontSize = (hovered || current) ? 16.0f : 11.0f;
            float tw = MeasureTextEx(font, name, fontSize, 1).x;
            float lx = n.pos.x - tw / 2.0f;
            float ly = (hovered || current) ? n.pos.y - radius - 26.0f : ly2;

            if (hovered || current) {
                DrawRectangleRounded({ lx - 6, ly - 4, tw + 12, fontSize + 8 },
                                      0.3f, 4, { 5, 10, 20, 210 });
                DrawRectangleRoundedLinesEx({ lx - 6, ly - 4, tw + 12, fontSize + 8 },
                                            0.3f, 4, 0.8f, ColorAlpha(ringCol, 0.7f));
                const char* threats[] = { "LOW RISK","MEDIUM RISK","HIGH RISK" };
                const char* tStr = threats[(int)n.threat];
                float tw2 = MeasureTextEx(font, tStr, 12, 1).x;
                DrawTextEx(font, tStr, { n.pos.x - tw2 / 2, ly - 17 },
                           12, 1, ColorAlpha(tColor, 0.85f));
            }
            DrawTextEx(font, name, { lx, ly }, fontSize, 1,
                       (hovered || current) ? Color{ 160, 200, 255, 220 }
                       : n.visited ? ColorAlpha(Color{ 140, 170, 220, 255 }, 0.75f)
                                   : ColorAlpha(tColor, 0.55f));
            ly2 += (hovered || current) ? 0 : fontSize + 4;
        }

        if (evLabel) {
            float ew = MeasureTextEx(font, evLabel, 11, 1).x;
            DrawTextEx(font, evLabel, { n.pos.x - ew / 2, ly2 }, 11, 1, evColor);
        }
    }

    if (hovered && reachable) {
        const char* lbl = n.isExit ? "[ NEXT ZONE ]" : "[ JUMP ]";
        float lw = MeasureTextEx(font, lbl, 12, 1).x;
        DrawTextEx(font, lbl,
                   { n.pos.x - lw / 2, n.pos.y + radius + (n.isExit ? 22 : 28) },
                   12, 1, { 80, 200, 255, 200 });
    }
}

void GalaxyMap::_DrawScanLine() const {
    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    float y = fmodf(_scanTimer * screenH * 0.5f, (float)screenH);

    // Ligne principale
    DrawLineEx({ 0, y }, { (float)screenW, y }, 1.0f,
               { 60, 140, 255, 30 });
    // Traîne
    DrawLineEx({ 0, y - 2 }, { (float)screenW, y - 2 }, 1.5f,
               { 60, 140, 255, 15 });
}

void GalaxyMap::_DrawHeader(Font font) const {
    int screenW = GetScreenWidth();

    // Barre de titre
    DrawRectangle(0, 0, screenW, 56, { 5, 8, 18, 230 });
    DrawLineEx({ 0, 56 }, { (float)screenW, 56 }, 0.5f,
               { 60, 120, 220, 120 });

    // Titre centré
    const char* title = "NAVIGATION CHART";
    float tw = MeasureTextEx(font, title, 24.0f, 2).x;
    DrawTextEx(font, title,
               { (screenW - tw) / 2.0f, 16.0f },
               24.0f, 2, { 80, 180, 255, 240 });

    // Légende des menaces (droite)
    struct { const char* label; Color col; } legend[] = {
        { "LOW",    {  60, 200, 100, 200 } },
        { "MEDIUM", { 220, 170,  40, 200 } },
        { "HIGH",   { 220,  60,  60, 200 } },
    };
    float lx = (float)screenW - 160.0f;
    float ly = 18.0f;
    DrawTextEx(font, "THREAT:", { lx - 52, ly }, 13.0f, 1,
               { 100, 120, 160, 180 });
    for (int i = 0; i < 3; i++) {
        DrawCircleV({ lx + i * 48.0f, ly + 6 }, 4, legend[i].col);
        DrawTextEx(font, legend[i].label,
                   { lx + i * 48.0f + 8, ly },
                   12.0f, 1, legend[i].col);
    }

    // Instruction + barre de statut bas d'écran
    int sh = GetScreenHeight();
    DrawRectangle(0, sh - 44, screenW, 44, { 5, 8, 18, 220 });
    DrawLineEx({ 0, (float)sh - 44 }, { (float)screenW, (float)sh - 44 },
               0.8f, { 60, 120, 220, 100 });

    // Contrôle (centré)
    const char* hint = "TAB / M : FERMER   |   CLIQUER UN NœUD ACCESSIBLE POUR SAUTER";
    float hw = MeasureTextEx(font, hint, 12.0f, 1).x;
    DrawTextEx(font, hint, { (screenW - hw) / 2.0f, (float)sh - 33.0f },
               12.0f, 1, { 80, 110, 160, 150 });

    // Carburant (gauche)
    char fuelTxt[32];
    snprintf(fuelTxt, sizeof(fuelTxt), "FUEL : %d / %d", _playerFuel, _playerMaxFuel);
    Color fuelCol = (_playerFuel <= 3) ? Color{220, 60, 60, 220} : Color{220, 160, 50, 200};
    DrawTextEx(font, fuelTxt, { 16.0f, (float)sh - 33.0f }, 12.0f, 1, fuelCol);

    // Secteur courant (droite)
    if (_sectors && _currentNodeIdx < (int)_nodes.size()) {
        const char* csName = _sectors[_nodes[_currentNodeIdx].sectorIdx % _sectorCount].name;
        char curTxt[48];
        snprintf(curTxt, sizeof(curTxt), "SECTEUR : %s", csName);
        float cw = MeasureTextEx(font, curTxt, 12.0f, 1).x;
        DrawTextEx(font, curTxt, { (float)screenW - cw - 16.0f, (float)sh - 33.0f },
                   12.0f, 1, { 140, 190, 255, 200 });
    }
}

void GalaxyMap::_DrawTooltip(Font font) const {
    // Rien ici pour l'instant — le tooltip est géré dans _DrawNode
    (void)font;
}

// ─── Draw principal ───────────────────────────────────────────────────────────

void GalaxyMap::Draw(Font font) const {
    if (!_active) return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    // Fondu d'ouverture
    float fadeAlpha = std::min(_openTimer, 1.0f);

    // Fond
    DrawRectangle(0, 0, screenW, screenH,
                  ColorAlpha({ 2, 5, 15, 255 }, fadeAlpha * 0.92f));

    // Grille subtile
    for (int x = 0; x < screenW; x += 60)
        DrawLineEx({ (float)x, 0 }, { (float)x, (float)screenH },
                   0.3f, { 30, 50, 80, 20 });
    for (int y = 0; y < screenH; y += 60)
        DrawLineEx({ 0, (float)y }, { (float)screenW, (float)y },
                   0.3f, { 30, 50, 80, 20 });

    // Scan line
    _DrawScanLine();

    // ── Connexions ────────────────────────────────────────────────────────────
    // On dessine une seule fois chaque paire (i < j)
    for (int i = 0; i < (int)_nodes.size(); i++) {
        for (int j : _nodes[i].links) {
            if (j > i) {
                bool reach = (_IsReachable(i) || _IsReachable(j)
                              || i == _currentNodeIdx || j == _currentNodeIdx);
                _DrawConnection(_nodes[i], _nodes[j], reach);
            }
        }
    }

    // ── Nœuds ─────────────────────────────────────────────────────────────────
    for (int i = 0; i < (int)_nodes.size(); i++)
        _DrawNode(_nodes[i], i, font);

    // ── Header / footer ───────────────────────────────────────────────────────
    _DrawHeader(font);
}