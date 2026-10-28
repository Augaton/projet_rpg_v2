#pragma once
#include "raylib.h"
#include <vector>

// Champ d'étoiles en parallaxe, étiré en traînées pendant un saut FTL
class Starfield {
public:
    void Init(int count, int width, int height);
    void Update(float dt, float ftl);
    void Draw(float ftl) const;

private:
    struct Star {
        float x, y, z;
        float phase;   // scintillement
        Color color;
    };
    std::vector<Star> _stars;
    int   _w = 0, _h = 0;
    float _t = 0.0f;
};
