#pragma once
#include "raylib.h"
#include "Projectile.hpp"
#include <vector>
#include <functional>



class ProjectileManager {
public:
    void Load();
    void Unload();

    void Spawn(ProjType type, Vector2 origin, float angleDeg);
    void Update(float dt);
    void Draw() const;
    void Clear();
    void DrawGlow(float t) const;
    void SpawnEnemy(Vector2 origin, float angleDeg);

    std::function<void(Vector2, ProjType)> onImpact;
    const std::vector<Projectile>& GetAll() const { return _proj; }
    std::vector<Projectile>& GetAllMutable() { return _proj; }

private:
    std::vector<Projectile> _proj;
    float _animTimer = 0.0f;

    struct SpriteSheet {
        Texture2D tex;
        int       frameCount;
        bool      vertical;
        float     rotOffset;
    };

    SpriteSheet _sheets[5];

    struct ProjStats { float speed; float life; };
    static constexpr ProjStats STATS[5] = {
        { 900.0f,  1.5f },  // BULLET
        { 550.0f,  2.0f },  // BIG_BULLET
        { 1100.0f, 1.2f },  // LASER
        { 340.0f,  3.5f },  // TORPEDO
        { 600.0f,  2.0f },  // WAVE
    };

    void _DrawSheet(const SpriteSheet& s, int frame,
                    Vector2 pos, float rotation,
                    Color tint, float scale = 1.0f) const;

    void _DrawLaser(const Projectile& p) const;
};