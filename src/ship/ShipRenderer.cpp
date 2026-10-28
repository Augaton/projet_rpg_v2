#include "ShipRenderer.hpp"
#include "../core/UI.hpp"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

// Position des salles dans le repère du sprite (64 px, nez vers le haut)
// Grille 3x3 : avant (armes), milieu (boucliers / réparation), arrière (moteurs)
static constexpr Vector2 ROOM_LOCAL[SYS_COUNT] = {
    { -11.0f, -1.0f },   // SHIELDS
    {   0.0f, 12.0f },   // ENGINES
    {   0.0f,-14.0f },   // WEAPONS
    {  11.0f, -1.0f },   // REPAIR
    { -11.0f, 12.0f },   // CLOAK
    {  11.0f, 12.0f },   // HACKING
    { -11.0f,-14.0f },   // DEFENSE
    {  11.0f,-14.0f },   // SENSORS (pas une salle)
};
static constexpr float ROOM_SIZE = 26.0f;

static int SafeHull(const Ship& s) { return (s.hullType >= 0 && s.hullType < HULL_COUNT) ? s.hullType : 0; }

// ─── Chargement ──────────────────────────────────────────────────────────────

void ShipRenderer::Load() {
    auto load = [](const char* path) {
        Sheet s;
        if (!path || !FileExists(path)) return s;
        s.tex = LoadTexture(path);
        if (s.tex.id > 0 && s.tex.height > 0) {
            s.frames = std::max(1, s.tex.width / s.tex.height);
            SetTextureFilter(s.tex, TEXTURE_FILTER_POINT);
        }
        return s;
    };

    for (int i = 0; i < HULL_COUNT; i++) {
        const HullDef& d = HULL_DEFS[i];
        _visuals[i].base        = load(d.base);
        _visuals[i].engine      = load(d.engine);
        _visuals[i].shield      = load(d.shield);
        _visuals[i].weapons     = load(d.weapons);
        _visuals[i].destruction = load(d.destruction);
        if (!_visuals[i].base.Valid())
            TraceLog(LOG_WARNING, "ShipRenderer: sprite manquant pour la coque %d", i);
    }
}

void ShipRenderer::Unload() {
    for (auto& v : _visuals) {
        for (Sheet* s : { &v.base, &v.engine, &v.shield, &v.weapons, &v.destruction })
            if (s->tex.id > 0) UnloadTexture(s->tex);
    }
}

// ─── Géométrie ───────────────────────────────────────────────────────────────

float ShipRenderer::Size(const Ship& ship) const {
    const Visual& v = _visuals[SafeHull(ship)];
    float frame = v.base.Valid() ? v.base.FrameSize() : 64.0f;
    return frame * HULL_DEFS[SafeHull(ship)].scale;
}

static Vector2 Rotate(Vector2 p, float deg) {
    float r = deg * DEG2RAD;
    float c = cosf(r), s = sinf(r);
    return { p.x * c - p.y * s, p.x * s + p.y * c };
}

Vector2 ShipRenderer::RoomPos(const Ship& ship, const ShipView& view, int sys) const {
    if (sys < 0 || sys >= SYS_COUNT) return view.pos;
    float k = Size(ship) / 64.0f;
    Vector2 local = { ROOM_LOCAL[sys].x * k, ROOM_LOCAL[sys].y * k };
    Vector2 r = Rotate(local, view.rotation);
    return { view.pos.x + r.x, view.pos.y + r.y };
}

Vector2 ShipRenderer::MuzzlePos(const Ship& ship, const ShipView& view) const {
    Vector2 r = Rotate({ 0.0f, -Size(ship) * 0.38f }, view.rotation);
    return { view.pos.x + r.x, view.pos.y + r.y };
}

Vector2 ShipRenderer::ShieldRadii(const Ship& ship) const {
    float s = Size(ship);
    return { s * 0.58f, s * 0.50f };
}

int ShipRenderer::RoomAt(const Ship& ship, const ShipView& view, Vector2 point) const {
    for (int s = 0; s < SYS_COUNT; s++) {
        if (!ship.sys[s].Installed() || !SystemIsRoom(s)) continue;
        Vector2 c = RoomPos(ship, view, s);
        if (fabsf(point.x - c.x) <= ROOM_SIZE * 0.7f && fabsf(point.y - c.y) <= ROOM_SIZE * 0.7f)
            return s;
    }
    return -1;
}

float ShipRenderer::DestructionDuration(const Ship& ship) const {
    const Sheet& d = _visuals[SafeHull(ship)].destruction;
    return d.Valid() ? (float)d.frames / 12.0f : 0.8f;
}

// ─── Dessin ──────────────────────────────────────────────────────────────────

void ShipRenderer::_DrawFrame(const Sheet& s, int frame, const ShipView& view,
                              float scale, Color tint) const {
    if (!s.Valid()) return;
    float fs = s.FrameSize();
    frame = std::clamp(frame, 0, s.frames - 1);
    float size = fs * scale;
    DrawTexturePro(s.tex,
        { frame * fs, 0, fs, fs },
        { view.pos.x, view.pos.y, size, size },
        { size / 2.0f, size / 2.0f },
        view.rotation, tint);
}

void ShipRenderer::DrawShip(const Ship& ship, const ShipView& baseView, float t) const {
    ShipView view = baseView;
    view.pos.x += view.kick.x;
    view.pos.y += view.kick.y;
    const Visual& v = _visuals[SafeHull(ship)];
    float scale = HULL_DEFS[SafeHull(ship)].scale * view.scaleMult;

    if (view.destroyT >= 0.0f) {
        int frame = (int)(view.destroyT * 12.0f);
        if (v.destruction.Valid() && frame < v.destruction.frames)
            _DrawFrame(v.destruction, frame, view, scale, WHITE);
        return;
    }

    // Réacteurs
    if (v.engine.Valid()) {
        int frame = (int)(t * (12.0f + 12.0f * view.thrust)) % v.engine.frames;
        _DrawFrame(v.engine, frame, view, scale, ColorAlpha(WHITE, view.alpha));
        if (view.thrust > 0.0f) {
            // Flamme renforcée, légèrement vacillante
            float flicker = 0.7f + 0.3f * std::sin(t * 40.0f);
            BeginBlendMode(BLEND_ADDITIVE);
                _DrawFrame(v.engine, frame, view, scale * (1.0f + 0.12f * view.thrust),
                           ColorAlpha({ 140, 220, 255, 255 }, view.thrust * flicker));
            EndBlendMode();
        }
    }

    // Coque (rougit à l'impact, translucide si invisible)
    Color tint = ui::LerpColor(WHITE, { 255, 110, 110, 255 }, ship.hitFlash);
    if (view.alpha < 1.0f) {
        float shimmer = 0.85f + 0.15f * std::sin(t * 25.0f);
        tint = ColorAlpha(ui::LerpColor(tint, { 150, 220, 255, 255 }, 0.5f), view.alpha * shimmer);
    }
    if (v.base.Valid()) {
        _DrawFrame(v.base, 0, view, scale, tint);
    } else {
        DrawPoly(view.pos, 3, 40.0f, view.rotation - 90.0f, { 120, 140, 180, 255 });
    }

    // Tourelles : animation de tir
    if (v.weapons.Valid()) {
        float anim = 0.0f;
        for (const auto& w : ship.weapons) anim = std::max(anim, w.fireAnim);
        int frame = anim > 0.0f ? (int)((1.0f - anim / 0.5f) * (float)v.weapons.frames) : 0;
        _DrawFrame(v.weapons, frame, view, scale, tint);
    }
}

void ShipRenderer::DrawShield(const Ship& ship, const ShipView& baseView, float t) const {
    ShipView view = baseView;
    view.pos.x += view.kick.x;
    view.pos.y += view.kick.y;
    if (view.destroyT >= 0.0f || ship.shieldLayers <= 0) return;

    const Visual& v = _visuals[SafeHull(ship)];
    float layers = (float)ship.shieldLayers;
    float scale  = HULL_DEFS[SafeHull(ship)].scale;

    if (v.shield.Valid()) {
        int frame = (int)(t * 12.0f) % v.shield.frames;
        unsigned char a = (unsigned char)std::min(255.0f, 90.0f + 35.0f * layers);
        BeginBlendMode(BLEND_ADDITIVE);
            _DrawFrame(v.shield, frame, view, scale * 1.08f, { 110, 200, 255, a });
        EndBlendMode();
    }

    // Bulle elliptique : cercle étiré verticalement (voile plus dense au bord, liseré, balayage)
    Vector2 r = ShieldRadii(ship);
    float pulse = 0.5f + 0.5f * sinf(t * 2.0f);
    rlPushMatrix();
    rlTranslatef(view.pos.x, view.pos.y, 0.0f);
    rlScalef(1.0f, r.y / r.x, 1.0f);
    BeginBlendMode(BLEND_ADDITIVE);
        DrawCircleGradient(0, 0, r.x, ColorAlpha(ui::C_SHIELD, 0.0f), ColorAlpha(ui::C_SHIELD, 0.10f + 0.04f * layers));
        for (int i = 0; i < ship.shieldLayers; i++) {
            float rad = r.x + (float)i * 6.0f;
            float a   = 0.38f + 0.18f * pulse - 0.08f * (float)i;
            DrawRing({ 0, 0 }, rad - 1.5f, rad, 0.0f, 360.0f, 72, ColorAlpha(ui::C_SHIELD, a));
            float sweep = t * (70.0f + 25.0f * i) + i * 120.0f;
            DrawRing({ 0, 0 }, rad - 3.0f, rad + 1.0f, sweep, sweep + 50.0f, 16, ColorAlpha(WHITE, 0.25f));
        }
    EndBlendMode();
    rlPopMatrix();
}

void ShipRenderer::DrawRooms(const Ship& ship, const ShipView& view, bool hostile,
                             int hoveredSys, const int* targetMask, float t) const {
    if (view.destroyT >= 0.0f) return;

    for (int s = 0; s < SYS_COUNT; s++) {
        const ShipSystem& x = ship.sys[s];
        if (!x.Installed() || !SystemIsRoom(s)) continue;

        Vector2 c = RoomPos(ship, view, s);
        Rectangle r = { c.x - ROOM_SIZE / 2, c.y - ROOM_SIZE / 2, ROOM_SIZE, ROOM_SIZE };

        Color state = (x.hackTimer > 0.0f)  ? ui::C_ION
                    : (x.damage >= x.level) ? ui::C_HULL_LOW
                    : (x.damage > 0)        ? ui::C_WARNING
                    : (x.ionTimer > 0.0f)   ? ui::C_ION
                    : hostile               ? Color{ 230, 200, 200, 255 }
                                            : ui::C_GOOD;

        bool hovered = (s == hoveredSys);
        float grow   = hovered ? 3.0f : 0.0f;
        Rectangle rr = { r.x - grow, r.y - grow, r.width + grow * 2, r.height + grow * 2 };

        ui::Bevel({ rr.x + 3, rr.y + 3, rr.width, rr.height }, 6, ColorAlpha(BLACK, 0.6f));
        ui::Bevel(rr, 6, hovered ? state : ColorAlpha(ui::C_INK, 0.82f));
        ui::BevelLines(rr, 6, state, 2.0f);
        ui::DrawIcon(s, c, 1.0f, hovered ? ui::C_INK : state);

        // Salle piratée : parasites violets
        if (x.hackTimer > 0.0f) {
            for (int k = 0; k < 4; k++) {
                float gy = rr.y + std::fmod(t * 60.0f + k * 7.0f, rr.height);
                DrawRectangleRec({ rr.x, gy, rr.width, 2 }, ColorAlpha(ui::C_ION, 0.7f));
            }
            ui::Label("HACK", rr.x - 2, rr.y - 12, 10, ui::C_ION);
        }

        // Barres endommagées sous la salle
        if (x.damage > 0) {
            float pw = 4.0f, gap = 2.0f;
            float total = x.level * pw + (x.level - 1) * gap;
            for (int i = 0; i < x.level; i++) {
                bool broken = i >= x.level - x.damage;
                DrawRectangle((int)(c.x - total / 2 + i * (pw + gap)), (int)(r.y + ROOM_SIZE + 3),
                              (int)pw, 3, broken ? ui::C_HULL_LOW : ui::C_GOOD);
            }
        }

        // Réticules des armes du joueur
        if (targetMask && targetMask[s]) {
            float pulse = 0.6f + 0.4f * sinf(t * 6.0f);
            Rectangle tr = { rr.x - 5, rr.y - 5, rr.width + 10, rr.height + 10 };
            ui::CornerBrackets(tr, 7.0f, ColorAlpha(ui::C_ENEMY, pulse));

            float lx = tr.x + tr.width + 3;
            for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
                if (!(targetMask[s] & (1 << i))) continue;
                const char* n = TextFormat("%d", i + 1);
                DrawRectangleRec({ lx, tr.y, 12, 12 }, ui::C_ENEMY);
                ui::Label(n, lx + 2, tr.y + 1, 10, ui::C_INK);
                lx += 14;
            }
        }
    }
}
