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
        unsigned char b = (unsigned char)(s.z * 180);
        s.color = rng::Chance(0.18f) ? Color{ b, (unsigned char)(b * 0.9f), 255, 255 }
                                     : Color{ b, b, b, 255 };
    }
}

void Starfield::Update(float dt, float ftl) {
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
            DrawRectangleRec({ std::round(s.x), std::round(s.y), sz, sz }, s.color);
        }
    }
}
