#include "galaxymap.hpp"
#include <cmath>
#include <cstdlib>
#include <algorithm>

// ─── Helpers locaux ───────────────────────────────────────────────────────────

static float randf() { return (float)(rand() % 1000) / 1000.0f; }

static void DrawTextCentered(Font font, const char* text, float cx, float y,
                              float size, float spacing, Color col) {
    float w = MeasureTextEx(font, text, size, spacing).x;
    DrawTextEx(font, text, { cx - w / 2.0f, y }, size, spacing, col);
}

// ─── Palette FTL/terminal ─────────────────────────────────────────────────────
static constexpr Color COL_BG      = {  8,  13,  20, 255 };
static constexpr Color COL_GRID    = { 13,  26,  42,  60 };
static constexpr Color COL_CYAN    = {  0, 200, 255, 255 };
static constexpr Color COL_GOLD    = { 255, 200,  32, 255 };
static constexpr Color COL_RED     = { 255,  68,  68, 255 };
static constexpr Color COL_GREEN   = {  48, 210, 110, 255 };
static constexpr Color COL_ORANGE  = { 230, 145,  30, 255 };
static constexpr Color COL_TEXT    = { 138, 180, 204, 255 };
static constexpr Color COL_TEXTDIM = {  70, 100, 130, 255 };

// ─── Génération ───────────────────────────────────────────────────────────────

static NodeEvent RollEvent(NodeThreat threat) {
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

    const float MARGIN_X = 130.0f;
    const float MARGIN_Y = 100.0f;
    const float usableW  = _screenW - MARGIN_X * 2;
    const float usableH  = _screenH - MARGIN_Y * 2 - 70;

    const int COLS = 4;
    const int nodesPerCol[] = { 1, 2 + rand()%2, 2 + rand()%2, 1 };

    int idx = 0;
    for (int col = 0; col < COLS; col++) {
        int   cnt  = nodesPerCol[col];
        bool  exit = (col == COLS - 1);
        float cx   = MARGIN_X + (float)col / (COLS - 1) * usableW;

        for (int row = 0; row < cnt; row++) {
            float cy = MARGIN_Y + (row + 0.5f) / cnt * usableH;
            if (col > 0 && col < COLS - 1) {
                cx  = MARGIN_X + (float)col / (COLS - 1) * usableW + (randf() - 0.5f) * 28.0f;
                cy += (randf() - 0.5f) * 28.0f;
            }
            MapNode node{};
            node.pos          = { cx, cy };
            node.sectorIdx    = rand() % _sectorCount;
            node.visited      = (idx == 0);
            node.isCurrentPos = (idx == 0);
            node.isExit       = exit;
            node.column       = col;
            node.threat       = exit ? NodeThreat::HIGH : (NodeThreat)(rand() % 3);
            node.event        = exit ? NodeEvent::EMPTY
                                     : (col == 0 ? NodeEvent::EMPTY : RollEvent(node.threat));
            _nodes.push_back(node);
            idx++;
        }
    }

    for (int i = 0; i < (int)_nodes.size(); i++) {
        for (int j = 0; j < (int)_nodes.size(); j++) {
            if (_nodes[j].column != _nodes[i].column + 1) continue;
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
    _screenW        = screenW;
    _screenH        = screenH;
    _zoneLevel++;
    _lastJumpEvent  = NodeEvent::EMPTY;
    _lastJumpIsExit = false;
    _BuildNodes();
    _openTimer = 0.0f;
}

void GalaxyMap::MarkCurrentVisited(int nodeIdx) {
    if (nodeIdx < 0 || nodeIdx >= (int)_nodes.size()) return;
    for (auto& n : _nodes) n.isCurrentPos = false;
    _nodes[nodeIdx].visited      = true;
    _nodes[nodeIdx].isCurrentPos = true;
    _currentNodeIdx = nodeIdx;
}

// ─── Update ───────────────────────────────────────────────────────────────────

void GalaxyMap::Update(Vector2 mousePos, float dt) {
    if (!_active) return;

    _scanTimer  += dt * 0.30f;
    _pulseTimer += dt * 1.8f;
    _openTimer   = std::min(_openTimer + dt * 4.0f, 1.0f);

    _hoveredIdx = -1;
    for (int i = 0; i < (int)_nodes.size(); i++) {
        if (CheckCollisionPointCircle(mousePos, _nodes[i].pos, 26)) {
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
    const auto& links = _nodes[_currentNodeIdx].links;
    return std::find(links.begin(), links.end(), idx) != links.end();
}

Color GalaxyMap::_ThreatColor(NodeThreat t) const {
    switch (t) {
        case NodeThreat::LOW:    return COL_GREEN;
        case NodeThreat::MEDIUM: return COL_ORANGE;
        case NodeThreat::HIGH:   return COL_RED;
    }
    return WHITE;
}

// ─── Connexions ───────────────────────────────────────────────────────────────

void GalaxyMap::_DrawConnection(const MapNode& a, const MapNode& b,
                                 bool reachable) const {
    if (reachable) {
        DrawLineEx(a.pos, b.pos, 1.5f, ColorAlpha(COL_CYAN, 0.35f));

        // Flèche centrale
        float dx = b.pos.x - a.pos.x, dy = b.pos.y - a.pos.y;
        float len = sqrtf(dx*dx + dy*dy);
        float ux = dx/len, uy = dy/len;
        float mx = (a.pos.x + b.pos.x)*0.5f, my = (a.pos.y + b.pos.y)*0.5f;
        Vector2 tip  = { mx + ux*7,             my + uy*7             };
        Vector2 left = { mx - ux*5 - uy*3.5f,   my - uy*5 + ux*3.5f  };
        Vector2 rigt = { mx - ux*5 + uy*3.5f,   my - uy*5 - ux*3.5f  };
        DrawTriangle(tip, left, rigt, ColorAlpha(COL_CYAN, 0.55f));
    } else {
        // Pointillés discrets
        float dx = b.pos.x - a.pos.x, dy = b.pos.y - a.pos.y;
        float len = sqrtf(dx*dx + dy*dy);
        float ux = dx/len, uy = dy/len;
        for (float t = 0; t < len; t += 12.0f) {
            float t2 = std::min(t + 5.0f, len);
            DrawLineEx({ a.pos.x + ux*t,  a.pos.y + uy*t  },
                       { a.pos.x + ux*t2, a.pos.y + uy*t2 },
                       0.8f, ColorAlpha(COL_TEXTDIM, 0.28f));
        }
    }
}

// ─── Nœud ─────────────────────────────────────────────────────────────────────

void GalaxyMap::_DrawNode(const MapNode& n, int idx, Font font) const {
    bool reachable = _IsReachable(idx);
    bool hovered   = (idx == _hoveredIdx);
    bool current   = n.isCurrentPos;

    float pulse = 0.70f + 0.30f * sinf(_pulseTimer + idx * 1.4f);

    float radius = n.isExit  ? 22.0f
                 : current   ? 20.0f
                 : reachable ? 18.0f
                             : 14.0f;

    Color nodeCol = n.isExit  ? COL_GOLD
                  : current   ? COL_CYAN
                  : reachable ? _ThreatColor(n.threat)
                  : n.visited ? COL_TEXTDIM
                              : ColorAlpha(_ThreatColor(n.threat), 0.32f);

    // ── Halo pulsant ──────────────────────────────────────────────────────────
    if (current) {
        DrawCircleV(n.pos, radius + 14, ColorAlpha(COL_CYAN, 0.07f * pulse));
        DrawCircleV(n.pos, radius +  7, ColorAlpha(COL_CYAN, 0.18f * pulse));
    } else if (n.isExit && !n.visited) {
        DrawCircleV(n.pos, radius + 14, ColorAlpha(COL_GOLD, 0.07f * pulse));
        DrawCircleV(n.pos, radius +  7, ColorAlpha(COL_GOLD, 0.18f * pulse));
    } else if (reachable) {
        DrawCircleV(n.pos, radius + 8, ColorAlpha(nodeCol, 0.12f * pulse));
    }

    // ── Corps ─────────────────────────────────────────────────────────────────
    DrawCircleV(n.pos, radius, ColorAlpha(COL_BG, 0.93f));
    DrawCircleLinesV(n.pos, radius, nodeCol);

    // Second anneau fin sur hovered/current/exit — PAS de rectangle
    if (hovered || current || (n.isExit && !n.visited))
        DrawCircleLinesV(n.pos, radius + 3, ColorAlpha(nodeCol, 0.38f * pulse));

    // ── Icône intérieure ──────────────────────────────────────────────────────
    float s = radius * 0.38f;

    if (n.isExit && !n.visited) {
        // Flèche →
        DrawLineEx({ n.pos.x - s, n.pos.y }, { n.pos.x + s, n.pos.y }, 2.0f, COL_GOLD);
        DrawLineEx({ n.pos.x + s - 4, n.pos.y - 3 }, { n.pos.x + s, n.pos.y }, 2.0f, COL_GOLD);
        DrawLineEx({ n.pos.x + s - 4, n.pos.y + 3 }, { n.pos.x + s, n.pos.y }, 2.0f, COL_GOLD);
    } else if (current) {
        // Croix + point central
        DrawLineEx({ n.pos.x - s, n.pos.y }, { n.pos.x + s, n.pos.y }, 1.6f, COL_CYAN);
        DrawLineEx({ n.pos.x, n.pos.y - s }, { n.pos.x, n.pos.y + s }, 1.6f, COL_CYAN);
        DrawCircleV(n.pos, 2.5f, COL_CYAN);
    } else if (n.visited) {
        DrawCircleV(n.pos, 3.0f, COL_TEXTDIM);
    } else {
        Color ic = ColorAlpha(WHITE, reachable ? 0.80f : 0.28f);
        switch (n.event) {
            case NodeEvent::COMBAT:
                DrawLineEx({ n.pos.x - s, n.pos.y }, { n.pos.x + s, n.pos.y }, 1.5f, ic);
                DrawLineEx({ n.pos.x, n.pos.y - s }, { n.pos.x, n.pos.y + s }, 1.5f, ic);
                DrawCircleLinesV(n.pos, s * 0.65f, ic);
                break;
            case NodeEvent::MERCHANT:
                DrawLineEx({ n.pos.x, n.pos.y - s }, { n.pos.x, n.pos.y + s }, 1.5f, ic);
                DrawCircleLinesV({ n.pos.x, n.pos.y - s*0.4f }, s*0.55f, ic);
                DrawCircleLinesV({ n.pos.x, n.pos.y + s*0.4f }, s*0.55f, ic);
                break;
            case NodeEvent::DERELICT:
                DrawLineEx({ n.pos.x,     n.pos.y - s }, { n.pos.x + s, n.pos.y      }, 1.5f, ic);
                DrawLineEx({ n.pos.x + s, n.pos.y      }, { n.pos.x,     n.pos.y + s  }, 1.5f, ic);
                DrawLineEx({ n.pos.x,     n.pos.y + s  }, { n.pos.x - s, n.pos.y      }, 1.5f, ic);
                DrawLineEx({ n.pos.x - s, n.pos.y      }, { n.pos.x,     n.pos.y - s  }, 1.5f, ic);
                break;
            default:
                DrawCircleV(n.pos, 2.0f, ColorAlpha(ic, 0.4f));
                break;
        }
    }

    // ── Labels ────────────────────────────────────────────────────────────────
    // AUCUN rectangle autour des labels. Texte direct, propre.

    float labelY = n.pos.y + radius + 7.0f;

    if (n.isExit) {
        DrawTextCentered(font, "EXIT ZONE", n.pos.x, labelY,
                         12.0f, 1, ColorAlpha(COL_GOLD, 0.88f));
        if (hovered && reachable)
            DrawTextCentered(font, "> NEXT ZONE <", n.pos.x, labelY + 16.0f,
                             12.0f, 1, ColorAlpha(COL_GOLD, 0.90f));
    } else {
        // Nom du secteur
        if (_sectors) {
            const char* name = _sectors[n.sectorIdx % _sectorCount].name;
            Color nameCol = current   ? COL_CYAN
                          : n.visited ? COL_TEXTDIM
                          : reachable ? COL_TEXT
                                      : ColorAlpha(COL_TEXT, 0.35f);
            DrawTextCentered(font, name, n.pos.x, labelY, 12.0f, 1, nameCol);
            labelY += 16.0f;
        }

        // Type d'événement (non visité seulement)
        if (!n.visited && !current) {
            const char* evLabel = nullptr;
            Color        evCol  = {};
            switch (n.event) {
                case NodeEvent::COMBAT:   evLabel = "COMBAT";   evCol = COL_RED;    break;
                case NodeEvent::MERCHANT: evLabel = "MERCHANT"; evCol = COL_GREEN;  break;
                case NodeEvent::DERELICT: evLabel = "DERELICT"; evCol = COL_ORANGE; break;
                default: break;
            }
            if (evLabel) {
                DrawTextCentered(font, evLabel, n.pos.x, labelY, 11.0f, 1,
                                 ColorAlpha(evCol, reachable ? 0.90f : 0.30f));
                labelY += 15.0f;
            }
        }

        // Prompt de saut ou hors portée
        if (hovered && reachable)
            DrawTextCentered(font, "> JUMP <", n.pos.x, labelY,
                             12.0f, 1, ColorAlpha(COL_CYAN, 0.90f));
        else if (hovered && !reachable && !current)
            DrawTextCentered(font, "OUT OF RANGE", n.pos.x, labelY,
                             10.0f, 1, ColorAlpha(COL_RED, 0.65f));
    }
}

// ─── Scanline ────────────────────────────────────────────────────────────────

void GalaxyMap::_DrawScanLine() const {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    float y = fmodf(_scanTimer * sh * 0.50f, (float)sh);
    DrawLineEx({ 0, y }, { (float)sw, y }, 1.0f, ColorAlpha(COL_CYAN, 0.06f));
    DrawRectangleGradientV(0, (int)(y - 10), sw, 10,
                           {0,0,0,0}, ColorAlpha(COL_CYAN, 0.04f));
}

// ─── Header + Footer ─────────────────────────────────────────────────────────

void GalaxyMap::_DrawHeader(Font font) const {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    // ── HEADER ────────────────────────────────────────────────────────────────
    DrawRectangle(0, 0, sw, 68, ColorAlpha(COL_BG, 0.97f));
    DrawLineEx({ 0, 68 }, { (float)sw, 68 }, 1.2f, ColorAlpha(COL_CYAN, 0.28f));

    // Titre (gauche)
    DrawTextEx(font, "NAVIGATION CHART", { 20.0f, 12.0f }, 24.0f, 2, COL_CYAN);
    char zoneTxt[32];
    snprintf(zoneTxt, sizeof(zoneTxt), "ZONE %d", _zoneLevel);
    DrawTextEx(font, zoneTxt, { 22.0f, 42.0f }, 12.0f, 1, COL_TEXTDIM);

    // ── Légende centrée : EVENTS | THREAT ────────────────────────────────────
    // Tout en texte + petits cercles. Aucun rectangle.
    float cx = sw * 0.50f;

    // EVENTS (gauche du centre)
    float ex = cx - 250.0f;
    DrawTextEx(font, "EVENTS :", { ex, 10.0f }, 11.0f, 1, COL_TEXTDIM);
    struct { const char* name; Color col; } evs[] = {
        { "COMBAT",   COL_RED    },
        { "MERCHANT", COL_GREEN  },
        { "DERELICT", COL_ORANGE },
    };
    float ey = 30.0f;
    for (int i = 0; i < 3; i++) {
        float x = ex + i * 88.0f;
        DrawCircleV({ x + 5, ey + 6 }, 4.5f, ColorAlpha(evs[i].col, 0.22f));
        DrawCircleLinesV({ x + 5, ey + 6 }, 4.5f, evs[i].col);
        DrawTextEx(font, evs[i].name, { x + 14, ey }, 11.0f, 1, evs[i].col);
    }

    // Séparateur vertical discret
    DrawLineEx({ cx + 20, 8 }, { cx + 20, 62 }, 1.0f,
               ColorAlpha(COL_TEXTDIM, 0.28f));

    // THREAT (droite du centre)
    float tx = cx + 34.0f;
    DrawTextEx(font, "THREAT :", { tx, 10.0f }, 11.0f, 1, COL_TEXTDIM);
    struct { const char* name; Color col; } threats[] = {
        { "LOW",    COL_GREEN  },
        { "MEDIUM", COL_ORANGE },
        { "HIGH",   COL_RED    },
    };
    for (int i = 0; i < 3; i++) {
        float x = tx + i * 74.0f;
        DrawCircleV({ x + 5, ey + 6 }, 4.5f, ColorAlpha(threats[i].col, 0.22f));
        DrawCircleLinesV({ x + 5, ey + 6 }, 4.5f, threats[i].col);
        DrawTextEx(font, threats[i].name, { x + 14, ey }, 11.0f, 1, threats[i].col);
    }

    // ── FOOTER ────────────────────────────────────────────────────────────────
    float fy = (float)sh - 58.0f;
    DrawRectangle(0, (int)fy, sw, 58, ColorAlpha(COL_BG, 0.97f));
    DrawLineEx({ 0, fy }, { (float)sw, fy }, 1.2f, ColorAlpha(COL_CYAN, 0.28f));

    // Carburant (gauche)
    {
        Color fuelCol = (_playerFuel <= 3) ? COL_RED : COL_GOLD;
        DrawTextEx(font, "FUEL", { 20.0f, fy + 8.0f }, 11.0f, 1, COL_TEXTDIM);
        float bx = 60.0f, by = fy + 10.0f, bw = 130.0f, bh = 11.0f;
        float ratio = (_playerMaxFuel > 0) ? (float)_playerFuel / _playerMaxFuel : 0.0f;
        DrawRectangle((int)bx, (int)by, (int)bw, (int)bh, { 20,30,45,230 });
        if (ratio > 0.f)
            DrawRectangle((int)bx, (int)by, (int)(bw*ratio), (int)bh, fuelCol);
        DrawRectangleLines((int)bx, (int)by, (int)bw, (int)bh,
                           ColorAlpha(fuelCol, 0.38f));
        char fuelNum[16];
        snprintf(fuelNum, sizeof(fuelNum), "%d / %d", _playerFuel, _playerMaxFuel);
        float fnw = MeasureTextEx(font, fuelNum, 11.0f, 1).x;
        DrawTextEx(font, fuelNum, { bx + (bw - fnw)/2, by + 14.0f },
                   11.0f, 1, ColorAlpha(fuelCol, 0.82f));
    }

    // Secteur courant (droite)
    if (_sectors && _currentNodeIdx < (int)_nodes.size()) {
        const char* csName =
            _sectors[_nodes[_currentNodeIdx].sectorIdx % _sectorCount].name;
        char curTxt[64];
        snprintf(curTxt, sizeof(curTxt), "CURRENT SECTOR : %s", csName);
        float cw = MeasureTextEx(font, curTxt, 12.0f, 1).x;
        DrawTextEx(font, curTxt, { (float)sw - cw - 20.0f, fy + 10.0f },
                   12.0f, 1, COL_TEXT);
    }

    // Contrôles (centré en bas du footer)
    const char* hint = "TAB / M  :  CLOSE     |     CLICK A REACHABLE NODE TO JUMP";
    float hw = MeasureTextEx(font, hint, 11.0f, 1).x;
    DrawTextEx(font, hint, { ((float)sw - hw)/2.0f, fy + 36.0f },
               11.0f, 1, COL_TEXTDIM);
}

void GalaxyMap::_DrawTooltip(Font font) const { (void)font; }

// ─── Draw principal ───────────────────────────────────────────────────────────

void GalaxyMap::Draw(Font font) const {
    if (!_active) return;

    int   sw   = GetScreenWidth();
    int   sh   = GetScreenHeight();
    float fade = std::min(_openTimer, 1.0f);

    // Fond uniforme très sombre
    DrawRectangle(0, 0, sw, sh, ColorAlpha(COL_BG, fade * 0.97f));

    // Grille fine style terminal
    for (int x = 0; x < sw; x += 48)
        DrawLineEx({ (float)x, 70 }, { (float)x, (float)sh - 60 }, 0.5f,
                   ColorAlpha(COL_GRID, 0.9f));
    for (int y = 70; y < sh - 60; y += 48)
        DrawLineEx({ 0, (float)y }, { (float)sw, (float)y }, 0.5f,
                   ColorAlpha(COL_GRID, 0.9f));

    _DrawScanLine();

    // Labels de colonnes sous le header
    const float MARGIN_X = 130.0f;
    const float usableW  = (float)sw - MARGIN_X * 2;
    const char* colLabels[] = { "DEPARTURE", "SECTOR A", "SECTOR B", "EXIT ZONE" };
    for (int col = 0; col < 4; col++) {
        float cx = MARGIN_X + (float)col / 3.0f * usableW;
        DrawLineEx({ cx, 72 }, { cx, (float)sh - 62 }, 0.5f,
                   ColorAlpha(COL_CYAN, 0.05f));
        float lw = MeasureTextEx(font, colLabels[col], 10.0f, 1).x;
        DrawTextEx(font, colLabels[col], { cx - lw/2, 74.0f },
                   10.0f, 1, ColorAlpha(COL_TEXTDIM, 0.50f));
    }

    // Connexions
    for (int i = 0; i < (int)_nodes.size(); i++)
        for (int j : _nodes[i].links)
            if (j > i) {
                bool reach = (_IsReachable(i) || _IsReachable(j)
                              || i == _currentNodeIdx || j == _currentNodeIdx);
                _DrawConnection(_nodes[i], _nodes[j], reach);
            }

    // Nœuds
    for (int i = 0; i < (int)_nodes.size(); i++)
        _DrawNode(_nodes[i], i, font);

    // Header / footer par-dessus tout
    _DrawHeader(font);
}