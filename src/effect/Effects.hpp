#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// Particules, débris, textes flottants, ondes de choc, tremblement et flash d'écran.
class Effects {
public:
    void Spark(Vector2 pos, Color color, int count, float speed,
               float size = 3.0f, float life = 0.6f);
    void Explosion(Vector2 pos, float scale = 1.0f);
    void Debris(Vector2 pos, Color color, int count, float speed);
    void Flare(Vector2 pos, Color color, float size);          // éclair bref (tir, impact)
    void Shockwave(Vector2 pos, float radius, Color color);    // anneau qui s'étend
    void Ripple(Vector2 center, float rx, float ry, Color color);
    void Pickup(Vector2 from, Vector2 to, Color color, int count);   // éclats aspirés vers le HUD
    // big : texte qui « pop » plus fort (gros dégâts, kills)
    void Text(const std::string& text, Vector2 pos, Color color, float size = 20.0f, bool big = false);

    void Shake(float amount);
    void Flash(float amount, Color color = WHITE);
    // Réglages d'accessibilité (0..1)
    void SetIntensity(float shake, float flash) { _shakeScale = shake; _flashScale = flash; }
    float Impact() const { return _shake / 12.0f; }

    // drift : décalage horizontal (px/s) appliqué pendant un saut FTL
    void Update(float dt, float drift = 0.0f);
    void DrawWorld() const;
    void DrawFlash() const;
    Vector2 ShakeOffset() const { return _offset; }
    void Clear();

private:
    struct Particle {
        Vector2 pos, vel;
        float   life, maxLife, size, drag;
        Color   color;
    };
    struct Chunk {
        Vector2 pos, vel;
        float   rot, spin, life, maxLife, w, h;
        Color   color;
    };
    struct Homing {
        Vector2 pos, vel, target;
        float   delay, life;
        Color   color;
    };
    struct FloatingText {
        std::string text;
        Vector2     pos;
        float       life, maxLife, size, pop;
        Color       color;
    };
    struct Ring {
        Vector2 center;
        float   rx, ry, life, growth;
        Color   color;
    };

    std::vector<Particle>     _particles;
    std::vector<Chunk>        _chunks;
    std::vector<Homing>       _homing;
    std::vector<FloatingText> _texts;
    std::vector<Ring>         _rings;

    static constexpr size_t MAX_PARTICLES = 1500;   // garde-fous mémoire / perf
    static constexpr size_t MAX_CHUNKS    = 200;
    static constexpr size_t MAX_HOMING    = 120;

    float   _shake      = 0.0f;
    float   _shakeScale = 1.0f;
    float   _flashScale = 1.0f;
    float   _flash      = 0.0f;
    Color   _flashColor = WHITE;
    Vector2 _offset     = { 0, 0 };
};
