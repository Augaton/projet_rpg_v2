#pragma once
#include "raylib.h"
#include "Projectile.hpp"
#include <vector>

class ProjectileManager {
public:
    void Load();
    void Unload();

    // Tire un projectile vers un point précis. Renvoie son index.
    int  SpawnTargeted(ProjType type, Vector2 from, Vector2 to, bool isEnemy, int shotId);
    // Redirige un projectile arrivé vers un nouveau point
    void Retarget(Projectile& p, Vector2 to);
    // Laisse filer un projectile (tir raté) : il continue tout droit puis disparaît
    void Release(Projectile& p, float extraLife = 1.2f);

    void Update(float dt);
    void Draw() const;
    void DrawGlow() const;
    void Clear();

    std::vector<Projectile>&       GetAllMutable() { return _proj; }

private:
    static constexpr size_t MAX_PROJECTILES = 256;
    std::vector<Projectile> _proj;
    float _animTimer = 0.0f;

    struct SpriteSheet {
        Texture2D tex{};
        int       frameCount = 1;
        bool      vertical   = false;
        float     rotOffset  = 0.0f;
    };

    SpriteSheet _sheets[6];

    struct ProjStats {
        float speed;
        Color tint;
    };
    static constexpr ProjStats STATS[6] = {
        {  900.0f, { 255, 255, 255, 255 } },  // BULLET
        {  650.0f, { 255, 230, 200, 255 } },  // BIG_BULLET
        { 1100.0f, { 255, 255, 255, 255 } },  // LASER
        {  420.0f, { 255, 255, 255, 255 } },  // TORPEDO
        {  600.0f, { 200, 170, 255, 255 } },  // WAVE (ion)
        {  380.0f, { 150, 140, 130, 255 } },  // ROCK
    };

    void _DrawSheet(const SpriteSheet& s, int frame,
                    Vector2 pos, float rotation,
                    Color tint, float scale = 1.0f) const;

    void _DrawLaser(const Projectile& p, Color core, Color halo) const;
};
