#include "SectorMap.hpp"
#include "../core/Config.hpp"
#include "../core/Random.hpp"
#include "../core/UI.hpp"
#include <algorithm>
#include <cmath>
#include <deque>

using namespace ui;

static constexpr Rectangle PANEL = { 100, 40, 1080, 640 };
static constexpr Rectangle AREA  = { 190, 170, 900, 380 };

// ═════════════════════════════════════════════════════════════════════════════
// Génération
// ═════════════════════════════════════════════════════════════════════════════

float SectorMap::_ColumnX(float column) const {
    return AREA.x + AREA.width * (column / (float)(COLUMNS - 1));
}

void SectorMap::_Link(int a, int b) {
    if (a == b || _IsLinked(a, b)) return;
    _beacons[a].links.push_back(b);
    _beacons[b].links.push_back(a);
}

bool SectorMap::_IsLinked(int a, int b) const {
    const auto& l = _beacons[a].links;
    return std::find(l.begin(), l.end(), b) != l.end();
}

void SectorMap::Generate(int sectorIdx, bool finalSector) {
    _sector = std::clamp(sectorIdx, 0, SECTOR_COUNT - 1);
    _beacons.clear();
    _fleet = -1.5f;

    std::vector<std::vector<int>> cols(COLUMNS);

    for (int c = 0; c < COLUMNS; c++) {
        bool edge = (c == 0 || c == COLUMNS - 1);
        int rows  = edge ? 1 : rng::Int(2, 4);
        float band = AREA.height / (float)rows;

        for (int r = 0; r < rows; r++) {
            Beacon b;
            b.column = c;
            b.pos.x  = _ColumnX((float)c) + (edge ? 0.0f : rng::Float(-24.0f, 24.0f));
            b.pos.y  = AREA.y + band * (r + 0.5f) + (rows > 1 ? rng::Float(-band * 0.22f, band * 0.22f) : 0.0f);
            cols[c].push_back((int)_beacons.size());
            _beacons.push_back(b);
        }
    }

    // Liens vers la colonne suivante
    for (int c = 0; c < COLUMNS - 1; c++) {
        auto& next = cols[c + 1];
        for (int a : cols[c]) {
            std::vector<int> sorted = next;
            std::sort(sorted.begin(), sorted.end(), [&](int x, int y) {
                return fabsf(_beacons[x].pos.y - _beacons[a].pos.y) < fabsf(_beacons[y].pos.y - _beacons[a].pos.y);
            });
            _Link(a, sorted[0]);
            if (sorted.size() > 1 && rng::Chance(0.5f)) _Link(a, sorted[1]);
        }
        // Chaque balise doit être atteignable
        for (int b : next) {
            bool incoming = false;
            for (int a : cols[c]) incoming |= _IsLinked(a, b);
            if (incoming) continue;
            int best = cols[c][0];
            for (int a : cols[c])
                if (fabsf(_beacons[a].pos.y - _beacons[b].pos.y) < fabsf(_beacons[best].pos.y - _beacons[b].pos.y)) best = a;
            _Link(best, b);
        }
    }
    for (int c = 1; c < COLUMNS - 1; c++)
        for (size_t i = 1; i < cols[c].size(); i++)
            if (rng::Chance(0.3f)) _Link(cols[c][i - 1], cols[c][i]);

    // Types de balises
    std::vector<int> middle;
    for (int c = 1; c < COLUMNS - 1; c++)
        for (int b : cols[c]) {
            int roll = rng::Int(0, 99);
            _beacons[b].kind = roll < 54 ? BeaconKind::COMBAT
                             : roll < 70 ? BeaconKind::DISTRESS
                             : roll < 84 ? BeaconKind::DERELICT
                             : roll < 95 ? BeaconKind::NEBULA
                                         : BeaconKind::EMPTY;
            if (c >= 2 && c <= COLUMNS - 2) middle.push_back(b);
        }

    for (int n = 0; n < 2 && !middle.empty(); n++) {
        int pick = rng::Int(0, (int)middle.size() - 1);
        _beacons[middle[pick]].kind = BeaconKind::STORE;
        middle.erase(middle.begin() + pick);
    }

    // Dangers : plus fréquents dans les secteurs avancés, jamais sur les balises spéciales
    for (auto& b : _beacons) {
        if (b.kind == BeaconKind::STORE || b.column == 0 || b.column == COLUMNS - 1) continue;
        if (!rng::Chance(0.14f + 0.05f * (float)_sector)) continue;
        if (b.kind == BeaconKind::NEBULA) b.hazard = BeaconHazard::ION_STORM;
        else b.hazard = (BeaconHazard)rng::Int(1, HAZARD_COUNT - 1);
    }

    _beacons[cols[0][0]].kind = BeaconKind::START;
    _beacons[cols[COLUMNS - 1][0]].kind = finalSector ? BeaconKind::BOSS : BeaconKind::EXIT;

    _current = cols[0][0];
    _beacons[_current].visited = true;
    _hovered = -1;
    _ComputeDistances();
}

void SectorMap::_ComputeDistances() {
    int n = (int)_beacons.size();
    _distExit.assign(n, -1);
    std::deque<int> queue;
    for (int i = 0; i < n; i++) {
        if (_beacons[i].kind == BeaconKind::EXIT || _beacons[i].kind == BeaconKind::BOSS) {
            _distExit[i] = 0;
            queue.push_back(i);
        }
    }
    while (!queue.empty()) {
        int a = queue.front();
        queue.pop_front();
        for (int b : _beacons[a].links) {
            if (_distExit[b] < 0) {
                _distExit[b] = _distExit[a] + 1;
                queue.push_back(b);
            }
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Sérialisation
// ═════════════════════════════════════════════════════════════════════════════

MapState SectorMap::Export() const {
    MapState s;
    s.sector  = _sector;
    s.current = _current;
    s.fleet   = _fleet;
    s.beacons = _beacons;
    return s;
}

bool SectorMap::Import(const MapState& st) {
    int n = (int)st.beacons.size();
    if (st.sector < 0 || st.sector >= SECTOR_COUNT)   return false;
    if (n < 2 || n > MAX_BEACONS)                     return false;
    if (st.current < 0 || st.current >= n)            return false;
    if (!std::isfinite(st.fleet) || st.fleet < -5.0f || st.fleet > 50.0f) return false;

    for (int i = 0; i < n; i++) {
        const Beacon& b = st.beacons[i];
        if (!std::isfinite(b.pos.x) || !std::isfinite(b.pos.y))            return false;
        if (b.pos.x < 0 || b.pos.x > VIRTUAL_W || b.pos.y < 0 || b.pos.y > VIRTUAL_H) return false;
        if (b.column < 0 || b.column >= COLUMNS)                            return false;
        if ((int)b.kind < 0 || (int)b.kind >= BEACON_KIND_COUNT)            return false;
        if ((int)b.hazard < 0 || (int)b.hazard >= HAZARD_COUNT)             return false;
        if (b.links.size() > (size_t)MAX_BEACONS)                           return false;
        for (int l : b.links) {
            if (l < 0 || l >= n || l == i) return false;
            const auto& back = st.beacons[l].links;
            if (std::find(back.begin(), back.end(), i) == back.end()) return false;   // liens symétriques
        }
    }

    std::vector<Beacon> backup = std::move(_beacons);
    _beacons = st.beacons;
    _ComputeDistances();
    if (_distExit[st.current] < 0) {        // la sortie doit rester atteignable
        _beacons = std::move(backup);
        _ComputeDistances();
        return false;
    }

    _sector  = st.sector;
    _current = st.current;
    _fleet   = st.fleet;
    _hovered = -1;
    _beacons[_current].visited = true;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// État
// ═════════════════════════════════════════════════════════════════════════════

void SectorMap::Open(bool closable) {
    _open     = true;
    _closable = closable;
    _openAnim = 0.0f;
    ClearRequests();
}

bool SectorMap::JumpTo(int idx) {
    if (idx < 0 || idx >= (int)_beacons.size()) return true;
    bool was = _beacons[idx].visited;
    _current = idx;
    _beacons[idx].visited = true;
    return was;
}

bool SectorMap::IsOvertaken(int idx) const {
    return (float)_beacons[idx].column + 0.5f <= _fleet;
}

int SectorMap::FleetEta(int idx) const {
    float gap = (float)_beacons[idx].column + 0.5f - _fleet;
    return gap <= 0.0f ? 0 : (int)std::ceil(gap / FLEET_PER_JUMP);
}

int SectorMap::JumpsToExit(int idx) const {
    return (idx >= 0 && idx < (int)_distExit.size()) ? _distExit[idx] : -1;
}

// Connu si visité, balise spéciale, ou à portée des capteurs :
// voisines (de base), 2 sauts (capteurs niv. 1), tout le secteur (niv. 2)
bool SectorMap::_KindKnown(int idx) const {
    const Beacon& b = _beacons[idx];
    if (b.visited || b.kind == BeaconKind::STORE || b.kind == BeaconKind::EXIT ||
        b.kind == BeaconKind::BOSS || _IsLinked(_current, idx) || _sensors >= 2)
        return true;
    if (_sensors == 1)
        for (int l : _beacons[_current].links)
            if (_IsLinked(l, idx)) return true;
    return false;
}

const std::vector<int>& SectorMap::_Reachable() const {
    _reachBuf.assign(_beacons[_current].links.begin(), _beacons[_current].links.end());
    std::sort(_reachBuf.begin(), _reachBuf.end(), [&](int a, int b) { return _beacons[a].pos.y < _beacons[b].pos.y; });
    return _reachBuf;
}

const char* HazardName(BeaconHazard h) {
    switch (h) {
    case BeaconHazard::ASTEROIDS:   return "Champ d'astéroïdes";
    case BeaconHazard::SOLAR_FLARE: return "Éruptions solaires";
    case BeaconHazard::ION_STORM:   return "Tempête ionique";
    default:                  return "";
    }
}

const char* SectorMap::KindName(BeaconKind k) {
    switch (k) {
    case BeaconKind::START:    return "Point d'entrée";
    case BeaconKind::EMPTY:    return "Balise calme";
    case BeaconKind::COMBAT:   return "Contact hostile";
    case BeaconKind::STORE:    return "Magasin";
    case BeaconKind::DISTRESS: return "Signal de détresse";
    case BeaconKind::DERELICT: return "Épave";
    case BeaconKind::NEBULA:   return "Nébuleuse";
    case BeaconKind::EXIT:     return "Sortie du secteur";
    case BeaconKind::BOSS:     return "Vaisseau amiral";
    }
    return "";
}

// ═════════════════════════════════════════════════════════════════════════════
// Update
// ═════════════════════════════════════════════════════════════════════════════

int SectorMap::Update(float dt, int fuel) {
    if (!_open) return -1;
    _openAnim = std::min(1.0f, _openAnim + dt * 5.0f);

    Vector2 m = GetMousePosition();
    _hovered = -1;
    float best = 20.0f * 20.0f;
    for (int i = 0; i < (int)_beacons.size(); i++) {
        float dx = m.x - _beacons[i].pos.x, dy = m.y - _beacons[i].pos.y;
        if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; _hovered = i; }
    }

    if (fuel <= 0) return -1;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && _hovered >= 0 &&
        _hovered != _current && _IsLinked(_current, _hovered)) {
        return _hovered;
    }

    // Raccourcis clavier : 1..N = destinations, de haut en bas
    const auto& reach = _Reachable();
    for (int i = 0; i < (int)reach.size() && i < 9; i++)
        if (IsKeyPressed(KEY_ONE + i) || IsKeyPressed(KEY_KP_1 + i)) return reach[i];

    return -1;
}

// ═════════════════════════════════════════════════════════════════════════════
// Draw
// ═════════════════════════════════════════════════════════════════════════════

static void DashedLine(Vector2 a, Vector2 b, float thick, float on, float off, Color c) {
    float dx = b.x - a.x, dy = b.y - a.y;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    dx /= len; dy /= len;
    for (float d = 0.0f; d < len; d += on + off) {
        float e = std::min(d + on, len);
        DrawLineEx({ a.x + dx * d, a.y + dy * d }, { a.x + dx * e, a.y + dy * e }, thick, c);
    }
}

void SectorMap::Draw(int fuel, float t) {
    if (!_open) return;

    Dim(0.65f * _openAnim);
    float dy = (1.0f - _openAnim) * 30.0f;
    Rectangle p = { PANEL.x, PANEL.y + dy, PANEL.width, PANEL.height };
    Panel(p, C_ACCENT, 0.3f, C_PANEL_BG, "NAV.SYS // CARTE DU SECTEUR");

    // En-tête
    const SectorDef& sd = SECTORS[_sector];
    Label(TextFormat("SECTEUR %d/%d", _sector + 1, SECTOR_COUNT), p.x + 30, p.y + 28, 20, C_ACCENT);
    Label(sd.name, p.x + 30, p.y + 56, 40, C_TEXT);

    int toExit = JumpsToExit(_current);
    LabelRight(TextFormat("CARBURANT %d", fuel), p.x + p.width - 30, p.y + 28, 20, fuel > 3 ? C_FUEL : C_HULL_LOW);
    TextRight(TextFormat("Sortie : %d saut%s minimum", toExit, toExit > 1 ? "s" : ""),
              p.x + p.width - 30, p.y + 58, 16, C_LABEL);
    TextRight("Chaque saut : -1 carburant", p.x + p.width - 30, p.y + 80, 15, C_TEXT_DIM);

    DrawRectangleRec({ p.x + 30, p.y + 110, p.width - 60, 2 }, C_BORDER);

    Rectangle area = { AREA.x - 60, AREA.y - 40 + dy, AREA.width + 120, AREA.height + 80 };
    for (float gx = area.x; gx <= area.x + area.width; gx += 30)
        for (float gy = area.y; gy <= area.y + area.height; gy += 30)
            DrawRectangleRec({ gx, gy, 2, 2 }, ColorAlpha(C_BORDER, 0.5f));

    // Flotte rebelle
    if (_fleet > -0.5f) {
        float fx = std::min(_ColumnX(_fleet), area.x + area.width);
        Rectangle zone = { area.x, area.y, fx - area.x, area.height };
        if (zone.width > 0) {
            DrawRectangleRec(zone, ColorAlpha(C_HULL_LOW, 0.08f));
            Hazard(zone, ColorAlpha(C_HULL_LOW, 0.16f), t * 20.0f);
            DrawRectangleRec({ fx - 2, zone.y, 4, zone.height }, C_HULL_LOW);
            if (zone.width > 150) {
                float w = MeasureLabel("FLOTTE REBELLE", 10) + 12;
                DrawRectangleRec({ fx - w - 2, zone.y, w, 16 }, C_HULL_LOW);
                Label("FLOTTE REBELLE", fx - w + 4, zone.y + 3, 10, C_INK);
            }
        }
    }

    auto pos = [&](int i) { return Vector2{ _beacons[i].pos.x, _beacons[i].pos.y + dy }; };
    bool canJump = fuel > 0;

    // Liens
    for (int a = 0; a < (int)_beacons.size(); a++) {
        for (int b : _beacons[a].links) {
            if (b < a) continue;
            bool fromCurrent = (a == _current || b == _current);
            if (fromCurrent && canJump) {
                DrawLineEx(pos(a), pos(b), 3.0f, C_ACCENT);
                Vector2 from = pos(_current), to = pos(a == _current ? b : a);
                float k = std::fmod(t * 0.9f, 1.0f);
                Vector2 q = { from.x + (to.x - from.x) * k, from.y + (to.y - from.y) * k };
                DrawRectangleRec({ q.x - 3, q.y - 3, 6, 6 }, C_TEXT);
            } else {
                DashedLine(pos(a), pos(b), 2.0f, 6.0f, 6.0f, ColorAlpha(C_LABEL, fromCurrent ? 0.6f : 0.35f));
            }
        }
    }

    // Balises
    const auto& reach = _Reachable();
    for (int i = 0; i < (int)_beacons.size(); i++) {
        const Beacon& b = _beacons[i];
        Vector2 c = pos(i);
        bool known     = _KindKnown(i);
        bool overtaken = IsOvertaken(i);
        bool reachable = canJump && _IsLinked(_current, i);

        Color col = C_TEXT;
        if (known) {
            switch (b.kind) {
            case BeaconKind::STORE:  col = C_GOOD;     break;
            case BeaconKind::EXIT:   col = C_ACCENT;   break;
            case BeaconKind::BOSS:   col = C_ENEMY;    break;
            case BeaconKind::COMBAT: col = C_HULL_LOW; break;
            case BeaconKind::NEBULA: col = C_ION;      break;
            default:                 col = C_LABEL;    break;
            }
        }
        if (overtaken) col = C_HULL_LOW;

        float s = (i == _hovered) ? 22.0f : 16.0f;
        Rectangle r = { c.x - s / 2, c.y - s / 2, s, s };
        DrawRectangleRec({ r.x + 3, r.y + 3, s, s }, ColorAlpha(BLACK, 0.7f));
        DrawRectangleRec(r, b.visited ? ColorAlpha(col, 0.45f) : C_INK);
        DrawRectangleLinesEx(r, 2.0f, col);

        const char* glyph = !known                          ? "?"
                          : b.kind == BeaconKind::COMBAT    ? "!"
                          : b.kind == BeaconKind::NEBULA    ? "~"
                          : b.kind == BeaconKind::DISTRESS || b.kind == BeaconKind::DERELICT ? "*"
                                                            : nullptr;
        if (glyph && !b.visited)           LabelCentered(glyph, c.x + 1, c.y - 5, 10, known ? col : C_TEXT);
        else if (b.kind == BeaconKind::STORE) LabelCentered("$", c.x + 1, c.y - 5, 10, col);

        const char* tag = b.kind == BeaconKind::STORE ? "MAGASIN"
                        : b.kind == BeaconKind::EXIT  ? "SORTIE"
                        : b.kind == BeaconKind::BOSS  ? "AMIRAL" : nullptr;
        // Balises révélées par les capteurs
        if (!tag && known && !b.visited) {
            switch (b.kind) {
            case BeaconKind::COMBAT:   tag = "HOSTILE";   break;
            case BeaconKind::DISTRESS: tag = "DETRESSE";  break;
            case BeaconKind::DERELICT: tag = "EPAVE";     break;
            case BeaconKind::NEBULA:   tag = "NEBULEUSE"; break;
            case BeaconKind::EMPTY:    tag = "CALME";     break;
            default:                                      break;
            }
        }
        if (tag) LabelCentered(tag, c.x, c.y + 16, 10, col);
        // Danger connu : petit losange + libellé
        if (known && b.hazard != BeaconHazard::NONE) {
            DrawPoly({ c.x - 14, c.y - 14 }, 4, 6.0f, 0.0f, C_WARNING);
            const char* h = b.hazard == BeaconHazard::ASTEROIDS ? "ASTEROIDES"
                          : b.hazard == BeaconHazard::SOLAR_FLARE ? "ERUPTIONS" : "ION";
            LabelCentered(h, c.x, c.y + (tag ? 28 : 16), 10, C_WARNING);
        }

        if (reachable) {
            auto it = std::find(reach.begin(), reach.end(), i);
            int n = (int)(it - reach.begin()) + 1;
            if (n <= 9) {
                const char* key = TextFormat("%d", n);
                DrawRectangleRec({ c.x + 12, c.y - 22, 14, 14 }, C_ACCENT);
                Label(key, c.x + 15, c.y - 20, 10, C_INK);
            }
        }

        if (i == _current) {
            bool blink = std::fmod(t, 0.8f) < 0.5f;
            if (blink) CornerBrackets({ c.x - 18, c.y - 18, 36, 36 }, 8.0f, C_SHIELD);
            LabelCentered("VOUS", c.x, c.y - 36, 10, C_SHIELD);
        }
    }

    // Info-bulle
    if (_hovered >= 0) {
        const Beacon& b = _beacons[_hovered];
        const char* title = _KindKnown(_hovered) ? KindName(b.kind) : "Balise inexplorée";

        std::vector<std::pair<std::string, Color>> lines;
        if (_hovered == _current)                 lines.push_back({ "Position actuelle", C_TEXT_DIM });
        else if (!_IsLinked(_current, _hovered))  lines.push_back({ "Hors de portée", C_TEXT_DIM });
        else if (!canJump)                        lines.push_back({ "Carburant épuisé", C_HULL_LOW });
        else                                      lines.push_back({ "Clic : sauter (-1 carburant)", C_GOOD });

        if (_KindKnown(_hovered) && b.hazard != BeaconHazard::NONE)
            lines.push_back({ std::string("Danger : ") + HazardName(b.hazard), C_WARNING });

        int d = JumpsToExit(_hovered);
        if (d > 0) lines.push_back({ TextFormat("Sortie à %d saut%s", d, d > 1 ? "s" : ""), C_LABEL });

        if (IsOvertaken(_hovered))
            lines.push_back({ "RATTRAPÉE PAR LA FLOTTE", C_HULL_LOW });
        else if (_fleet > -0.5f || FleetEta(_hovered) <= 6)
            lines.push_back({ TextFormat("Flotte : dans ~%d saut%s", FleetEta(_hovered), FleetEta(_hovered) > 1 ? "s" : ""),
                              FleetEta(_hovered) <= 2 ? C_WARNING : C_LABEL });

        float w = MeasureLabel(title, 20) + 28;
        for (const auto& l : lines) w = std::max(w, Measure(l.first.c_str(), 15) + 28);
        float h = 38 + 20.0f * lines.size();

        Vector2 c = pos(_hovered);
        Rectangle tr = { std::min(c.x + 24, p.x + p.width - w - 16), std::max(p.y + 120, c.y - h - 10), w, h };
        DrawRectangleRec({ tr.x + 5, tr.y + 5, w, h }, ColorAlpha(BLACK, 0.7f));
        DrawRectangleRec(tr, C_INK);
        DrawRectangleLinesEx(tr, 2.0f, C_ACCENT);
        Label(title, tr.x + 14, tr.y + 10, 20, C_TEXT);
        float ly = tr.y + 36;
        for (const auto& l : lines) {
            Text(l.first.c_str(), tr.x + 14, ly, 15, l.second);
            ly += 20;
        }
    }

    // Pied de page
    float by = p.y + p.height - 62;
    DrawRectangleRec({ p.x + 30, by - 14, p.width - 60, 2 }, C_BORDER);
    Text("Touches 1-9 : sauter vers la balise numérotée", p.x + 30, by + 4, 15, C_TEXT_DIM);
    Text("Atteignez la SORTIE avant la flotte rebelle.", p.x + 30, by + 24, 15, C_TEXT_DIM);

    if (fuel <= 0) {
        if (Button({ p.x + p.width - 500, by, 260, 42 }, "SIGNAL DE DÉTRESSE", true, C_WARNING, 20))
            _distressRequested = true;
    }
    if (_closable) {
        if (Button({ p.x + p.width - 220, by, 190, 42 }, "FERMER", true, C_ACCENT, 20))
            _closeRequested = true;
    } else {
        LabelRight("FUITE : SAUTEZ !", p.x + p.width - 30, by + 12, 20, C_WARNING);
    }
}
