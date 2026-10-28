#include "Starfield.hpp"
#include "../core/Random.hpp"
#include <algorithm>
#include <cmath>

void Starfield::Init(int count, int width, int height) {
    _w = width;
    _h = height;
    _stars.resize(count);
    for (auto& s : _stars) {
        s.x = rng::Float(0.0f, (float)width);
        s.y = rng::Float(0.0f, (float)height);
        s.z = rng::Float(0.1f, 1.0f);
        s.phase = rng::Float(0.0f, 2.0f * PI);
        unsigned char b = (unsigned char)(s.z * 200);
        float hue = rng::Float(0.0f, 1.0f);
        s.color = hue < 0.18f ? Color{ (unsigned char)(b * 0.75f), (unsigned char)(b * 0.85f), 255, 255 }   // bleues
                : hue < 0.26f ? Color{ 255, (unsigned char)(b * 0.85f), (unsigned char)(b * 0.6f), 255 }    // orangées
                              : Color{ b, b, (unsigned char)std::min(255, b + 20), 255 };
    }
}

void Starfield::Update(float dt, float ftl) {
    _t += dt;
    const float base = 200.0f + 5000.0f * ftl;
    for (auto& s : _stars) {
        s.x -= base * s.z * dt;
        if (s.x < -60.0f) {
            s.x += (float)_w + 60.0f;
            s.y  = rng::Float(0.0f, (float)_h);
        }
    }
}

void Starfield::Draw(float ftl) const {
    for (const auto& s : _stars) {
        if (ftl > 0.05f) {
            float len = 60.0f * s.z * ftl;
            DrawLineEx({ s.x, s.y }, { s.x + len, s.y }, 1.0f + s.z,
                       ColorAlpha(s.color, 0.4f + ftl * 0.4f));
        } else {
            float sz = s.z > 0.75f ? 3.0f : s.z > 0.4f ? 2.0f : 1.0f;
            float twinkle = 0.65f + 0.35f * std::sin(_t * (1.5f + 2.5f * s.z) + s.phase);
            Color c = ColorAlpha(s.color, twinkle);
            float x = std::round(s.x), y = std::round(s.y);
            DrawRectangleRec({ x, y, sz, sz }, c);
            // Étoiles les plus proches : petite croix de diffraction
            if (s.z > 0.9f) {
                Color f = ColorAlpha(s.color, 0.35f * twinkle);
                DrawRectangleRec({ x - 3, y + 1, 3, 1 }, f);
                DrawRectangleRec({ x + sz, y + 1, 3, 1 }, f);
                DrawRectangleRec({ x + 1, y - 3, 1, 3 }, f);
                DrawRectangleRec({ x + 1, y + sz, 1, 3 }, f);
            }
        }
    }
}
