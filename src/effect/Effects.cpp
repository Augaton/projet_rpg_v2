#include "Effects.hpp"
#include "../core/Config.hpp"
#include "../core/Random.hpp"
#include "../core/UI.hpp"
#include <algorithm>
#include <cmath>

void Effects::Spark(Vector2 pos, Color color, int count, float speed, float size, float life) {
    count = std::min(count, (int)(MAX_PARTICLES - std::min(_particles.size(), MAX_PARTICLES)));
    for (int i = 0; i < count; i++) {
        float ang = rng::Float(0.0f, 2.0f * PI);
        float spd = speed * rng::Float(0.3f, 1.0f);
        float l   = life * rng::Float(0.6f, 1.2f);
        _particles.push_back({
            pos, { std::cos(ang) * spd, std::sin(ang) * spd },
            l, l, size * rng::Float(0.6f, 1.3f), 2.5f, color
        });
    }
}

void Effects::Explosion(Vector2 pos, float scale) {
    Flare(pos, { 255, 240, 200, 255 }, 26.0f * scale);
    Spark(pos, { 255, 220, 120, 255 }, (int)(14 * scale), 280.0f * scale, 3.5f * scale, 0.45f);
    Spark(pos, { 255, 120,  40, 255 }, (int)(18 * scale), 180.0f * scale, 5.0f * scale, 0.75f);
    Debris(pos, { 120, 110, 105, 255 }, (int)(3 * scale), 140.0f * scale);
    // Fumée lente
    for (int i = 0; i < (int)(8 * scale) && _particles.size() < MAX_PARTICLES; i++) {
        float ang = rng::Float(0.0f, 2.0f * PI);
        float spd = rng::Float(10.0f, 45.0f) * scale;
        float l   = rng::Float(0.8f, 1.4f);
        _particles.push_back({
            pos, { std::cos(ang) * spd, std::sin(ang) * spd },
            l, l, rng::Float(6.0f, 11.0f) * scale, 1.2f, { 90, 80, 80, 150 }
        });
    }
    Shockwave(pos, 22.0f * scale, { 255, 170, 80, 255 });
}

void Effects::Debris(Vector2 pos, Color color, int count, float speed) {
    for (int i = 0; i < count && _chunks.size() < MAX_CHUNKS; i++) {
        float ang = rng::Float(0.0f, 2.0f * PI);
        float spd = speed * rng::Float(0.4f, 1.0f);
        float l   = rng::Float(0.9f, 1.8f);
        float sz  = rng::Float(3.0f, 8.0f);
        _chunks.push_back({
            pos, { std::cos(ang) * spd, std::sin(ang) * spd },
            rng::Float(0.0f, 360.0f), rng::Float(-540.0f, 540.0f), l, l,
            sz, sz * rng::Float(0.4f, 1.0f), color
        });
    }
}

void Effects::Smoke(Vector2 pos, float size, float life) {
    if (_smoke.size() >= MAX_SMOKE) return;
    float ang = rng::Float(0.0f, 2.0f * PI);
    float spd = rng::Float(8.0f, 24.0f);
    float l   = life * rng::Float(0.7f, 1.2f);
    _smoke.push_back({ pos, { std::cos(ang) * spd, std::sin(ang) * spd }, l, l,
                       size * rng::Float(0.7f, 1.3f), 0.6f, { 96, 90, 84, 255 } });
}

void Effects::Flare(Vector2 pos, Color color, float size) {
    if (_particles.size() >= MAX_PARTICLES) return;
    _particles.push_back({ pos, { 0, 0 }, 0.09f, 0.09f, size, 0.0f, color });
}

void Effects::Shockwave(Vector2 pos, float radius, Color color) {
    if (_rings.size() >= 32) return;
    _rings.push_back({ pos, radius, radius, 1.0f, 2.2f, color });
}

void Effects::Ripple(Vector2 center, float rx, float ry, Color color) {
    if (_rings.size() >= 32) return;
    _rings.push_back({ center, rx, ry, 1.0f, 0.25f, color });
}

void Effects::Pickup(Vector2 from, Vector2 to, Color color, int count) {
    for (int i = 0; i < count && _homing.size() < MAX_HOMING; i++) {
        float ang = rng::Float(0.0f, 2.0f * PI);
        float spd = rng::Float(120.0f, 320.0f);
        _homing.push_back({
            { from.x + rng::Float(-20.0f, 20.0f), from.y + rng::Float(-20.0f, 20.0f) },
            { std::cos(ang) * spd, std::sin(ang) * spd },
            to, rng::Float(0.25f, 0.55f), 2.5f, color
        });
    }
}

void Effects::Text(const std::string& text, Vector2 pos, Color color, float size, bool big) {
    if (_texts.size() >= 48) _texts.erase(_texts.begin());
    // Empile les textes récents au même endroit pour qu'ils restent lisibles
    int stacked = 0;
    for (const auto& t : _texts)
        if (std::fabs(t.pos.x - pos.x) < 70.0f && std::fabs(t.pos.y - pos.y) < 40.0f && t.life > t.maxLife - 0.6f) stacked++;
    pos.y -= 24.0f * (float)stacked;
    _texts.push_back({ text, { pos.x + rng::Float(-6.0f, 6.0f), pos.y }, 1.1f, 1.1f, size, big ? 1.0f : 0.5f, color });
}

void Effects::Shake(float amount) { _shake = std::max(_shake, amount * _shakeScale); }

void Effects::Flash(float amount, Color color) {
    float a = amount * _flashScale;
    if (a >= _flash) _flashColor = color;
    _flash = std::max(_flash, a);
}

// ─── Mise à jour ─────────────────────────────────────────────────────────────

void Effects::Update(float dt, float drift) {
    auto advance = [&](std::vector<Particle>& v) {
        for (auto& p : v) {
            p.pos.x += (p.vel.x - drift) * dt;
            p.pos.y += p.vel.y * dt;
            float d = std::max(0.0f, 1.0f - p.drag * dt);
            p.vel.x *= d;
            p.vel.y *= d;
            p.life  -= dt;
        }
        std::erase_if(v, [](const Particle& p) { return p.life <= 0.0f; });
    };
    advance(_particles);
    advance(_smoke);

    for (auto& c : _chunks) {
        c.pos.x += (c.vel.x - drift) * dt;
        c.pos.y += c.vel.y * dt;
        c.vel.x *= std::max(0.0f, 1.0f - 0.8f * dt);
        c.vel.y *= std::max(0.0f, 1.0f - 0.8f * dt);
        c.rot   += c.spin * dt;
        c.life  -= dt;
    }
    std::erase_if(_chunks, [](const Chunk& c) { return c.life <= 0.0f; });

    // Éclats : s'éparpillent puis filent vers leur cible
    for (auto& h : _homing) {
        h.life -= dt;
        if (h.delay > 0.0f) {
            h.delay -= dt;
            h.vel.x *= std::max(0.0f, 1.0f - 4.0f * dt);
            h.vel.y *= std::max(0.0f, 1.0f - 4.0f * dt);
        } else {
            float dx = h.target.x - h.pos.x, dy = h.target.y - h.pos.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 14.0f) { h.life = 0.0f; continue; }
            const float accel = 2600.0f;
            h.vel.x += dx / len * accel * dt;
            h.vel.y += dy / len * accel * dt;
            h.vel.x *= std::max(0.0f, 1.0f - 3.0f * dt);
            h.vel.y *= std::max(0.0f, 1.0f - 3.0f * dt);
        }
        h.pos.x += h.vel.x * dt;
        h.pos.y += h.vel.y * dt;
    }
    std::erase_if(_homing, [](const Homing& h) { return h.life <= 0.0f; });

    for (auto& t : _texts) {
        t.pos.y -= 34.0f * dt;
        t.life  -= dt;
        t.pop    = std::max(0.0f, t.pop - dt * 6.0f);
    }
    std::erase_if(_texts, [](const FloatingText& t) { return t.life <= 0.0f; });

    for (auto& r : _rings) r.life -= dt * 2.4f;
    std::erase_if(_rings, [](const Ring& r) { return r.life <= 0.0f; });

    _shake = std::max(0.0f, _shake - dt * (8.0f + _shake * 4.0f));
    _flash = std::max(0.0f, _flash - dt * 2.2f);

    if (_shake > 0.1f) _offset = { rng::Float(-1.0f, 1.0f) * _shake, rng::Float(-1.0f, 1.0f) * _shake };
    else               _offset = { 0, 0 };
}

// ─── Dessin ──────────────────────────────────────────────────────────────────

void Effects::DrawWorld() const {
    // Fumée d'abord, en mélange normal : elle passe derrière les étincelles
    for (const auto& p : _smoke) {
        float t  = p.life / p.maxLife;
        float sz = std::max(2.0f, std::round(p.size * (1.6f - 0.8f * t) / 2.0f) * 2.0f);
        float x  = std::round(p.pos.x / 2.0f) * 2.0f - sz * 0.5f;
        float y  = std::round(p.pos.y / 2.0f) * 2.0f - sz * 0.5f;
        DrawRectangleRec({ x, y, sz, sz }, ColorAlpha(p.color, 0.7f * std::min(1.0f, 2.0f * t)));
    }

    for (const auto& r : _rings) {
        float grow = 1.0f + (1.0f - r.life) * r.growth;
        float a    = r.life * r.life;
        DrawEllipseLines((int)r.center.x, (int)r.center.y, r.rx * grow, r.ry * grow, ColorAlpha(r.color, a));
        DrawEllipseLines((int)r.center.x, (int)r.center.y, r.rx * grow + 2, r.ry * grow + 2, ColorAlpha(r.color, a * 0.6f));
        if (r.growth > 1.0f)
            DrawEllipseLines((int)r.center.x, (int)r.center.y, r.rx * grow + 4, r.ry * grow + 4, ColorAlpha(r.color, a * 0.3f));
    }

    for (const auto& c : _chunks) {
        float a = std::clamp(c.life / c.maxLife * 2.0f, 0.0f, 1.0f);
        DrawRectanglePro({ c.pos.x, c.pos.y, c.w, c.h }, { c.w / 2, c.h / 2 }, c.rot, ColorAlpha(c.color, a));
    }

    // Particules carrées alignées sur une grille de 2 px : rendu pixel
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& p : _particles) {
        float t  = p.life / p.maxLife;
        float sz = std::max(2.0f, std::round(p.size * (0.5f + 0.7f * t) / 2.0f) * 2.0f);
        float x  = std::round(p.pos.x / 2.0f) * 2.0f - sz * 0.5f;
        float y  = std::round(p.pos.y / 2.0f) * 2.0f - sz * 0.5f;
        DrawRectangleRec({ x, y, sz, sz }, ColorAlpha(p.color, t));
    }
    for (const auto& h : _homing) {
        DrawRectangleRec({ std::round(h.pos.x) - 3, std::round(h.pos.y) - 3, 6, 6 }, h.color);
        DrawRectangleRec({ std::round(h.pos.x) - 1, std::round(h.pos.y) - 1, 2, 2 }, WHITE);
    }
    EndBlendMode();

    for (const auto& t : _texts) {
        float a    = std::clamp(t.life / t.maxLife * 2.0f, 0.0f, 1.0f);
        float size = t.size * (1.0f + t.pop);
        const char* s = t.text.c_str();
        float w = ui::MeasureLabel(s, size);
        ui::Label(s, t.pos.x - w * 0.5f + 2, t.pos.y + 2, size, ColorAlpha(BLACK, a * 0.8f));
        ui::Label(s, t.pos.x - w * 0.5f,     t.pos.y,     size, ColorAlpha(t.pop > 0.3f ? WHITE : t.color, a));
    }
}

void Effects::DrawFlash() const {
    if (_flash > 0.0f)
        DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(_flashColor, std::min(_flash, 1.0f)));
}

void Effects::Clear() {
    _particles.clear();
    _smoke.clear();
    _chunks.clear();
    _homing.clear();
    _texts.clear();
    _rings.clear();
    _shake = 0.0f;
    _flash = 0.0f;
}
