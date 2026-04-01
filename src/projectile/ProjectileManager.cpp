#include "ProjectileManager.hpp"
#include <cmath>
#include <algorithm>

static Vector2 AngleToDir(float deg) {
    float r = deg * DEG2RAD;
    return { cosf(r), sinf(r) };
}

// ─── Load / Unload ────────────────────────────────────────────────────────────

void ProjectileManager::Load() {
    auto load = [](const char* path, int frames, bool vert, float rot) -> SpriteSheet {
        SpriteSheet s;
        s.tex        = LoadTexture(path);
        s.frameCount = frames;
        s.vertical   = vert;
        s.rotOffset  = rot;
        SetTextureFilter(s.tex, TEXTURE_FILTER_POINT);
        return s;
    };

    _sheets[(int)ProjType::BULLET]     = load("asset/Projectiles/PNGs/Bullet.png",     4, false, -90.0f);
    _sheets[(int)ProjType::BIG_BULLET] = load("asset/Projectiles/PNGs/Big_Bullet.png", 4, false, -90.0f);
    _sheets[(int)ProjType::LASER]      = load("asset/Projectiles/PNGs/Laser.png",      4, false,  -90.0f);
    _sheets[(int)ProjType::TORPEDO]    = load("asset/Projectiles/PNGs/Torpedo.png",    3, false, -90.0f);
    _sheets[(int)ProjType::WAVE]       = load("asset/Projectiles/PNGs/Wave.png",       6, false, -90.0f);
}

void ProjectileManager::Unload() {
    for (auto& s : _sheets)
        UnloadTexture(s.tex);
}

// ─── Spawn ────────────────────────────────────────────────────────────────────

// Dégâts par défaut selon le type
static const float DEFAULT_DMG[5] = {
    10.0f,  // BULLET
    20.0f,  // BIG_BULLET
    12.0f,  // LASER
    35.0f,  // TORPEDO
    20.0f,  // WAVE
};

void ProjectileManager::Spawn(ProjType type, Vector2 origin, float angleDeg, float damage) {
    Vector2 dir = AngleToDir(angleDeg);
    float   spd = STATS[(int)type].speed;

    Projectile p{};
    p.type     = type;
    p.pos      = origin;
    p.vel      = { dir.x * spd, dir.y * spd };
    p.rotation = angleDeg;
    p.life     = STATS[(int)type].life;
    p.active   = true;
    p.damage   = (damage < 0.0f) ? DEFAULT_DMG[(int)type] : damage;
    _proj.push_back(p);
}

// ─── Update ───────────────────────────────────────────────────────────────────

void ProjectileManager::Update(float dt) {
    _animTimer += dt;

    for (auto& p : _proj) {
        if (!p.active) continue;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
        p.life  -= dt;
        if (p.life <= 0) p.active = false;
    }

    _proj.erase(
        std::remove_if(_proj.begin(), _proj.end(),
            [](const Projectile& p) { return !p.active; }),
        _proj.end()
    );
}

// ─── Draw helpers ─────────────────────────────────────────────────────────────

void ProjectileManager::_DrawSheet(const SpriteSheet& s, int frame,
                                   Vector2 pos, float rotation,
                                   Color tint, float scale) const {
    if (s.tex.id == 0) return;
    frame = frame % s.frameCount;

    Rectangle src;
    if (s.vertical) {
        float fh = (float)s.tex.height / s.frameCount;
        src = { 0, frame * fh, (float)s.tex.width, fh };
    } else {
        float fw = (float)s.tex.width / s.frameCount;
        src = { frame * fw, 0, fw, (float)s.tex.height };
    }

    Rectangle dst = {
        pos.x, pos.y,
        fabsf(src.width)  * scale,
        fabsf(src.height) * scale,
    };
    Vector2 origin = { dst.width / 2.0f, dst.height / 2.0f };

    DrawTexturePro(s.tex, src, dst, origin, rotation, tint);
}

void ProjectileManager::_DrawLaser(const Projectile& p) const {
    float lifeRatio = p.life / STATS[(int)ProjType::LASER].life;
    float alpha     = std::min(lifeRatio * 3.0f, 1.0f);

    float rad  = p.rotation * DEG2RAD;
    Vector2 dir = { cosf(rad), sinf(rad) };

    // Longueur fixe du trait
    const float LEN = 38.0f;
    Vector2 tail = { p.pos.x - dir.x * LEN, p.pos.y - dir.y * LEN };

    // Passe 1 — halo large très transparent
    DrawLineEx(tail, p.pos, 9.0f,
               ColorAlpha({ 80, 180, 255, 255 }, alpha * 0.15f));

    // Passe 2 — halo moyen
    DrawLineEx(tail, p.pos, 5.0f,
               ColorAlpha({ 120, 210, 255, 255 }, alpha * 0.35f));

    // Passe 3 — corps principal cyan
    DrawLineEx(tail, p.pos, 2.5f,
               ColorAlpha({ 180, 235, 255, 255 }, alpha * 0.85f));

    // Passe 4 — cœur blanc pur, fin
    DrawLineEx(tail, p.pos, 1.0f,
               ColorAlpha({ 255, 255, 255, 255 }, alpha));

    // Pointe lumineuse à la tête
    DrawCircleV(p.pos, 3.0f,
                ColorAlpha({ 200, 240, 255, 255 }, alpha));
    DrawCircleV(p.pos, 1.5f,
                ColorAlpha({ 255, 255, 255, 255 }, alpha));
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void ProjectileManager::Draw() const {
    const float FPS = 12.0f;

    for (const auto& p : _proj) {
        if (!p.active) continue;

        const SpriteSheet& s     = _sheets[(int)p.type];
        int                frame = (int)(_animTimer * FPS) % s.frameCount;
        float              rot   = p.rotation + s.rotOffset;

       if (p.type == ProjType::LASER) {
            BeginBlendMode(BLEND_ADDITIVE);
                _DrawLaser(p);
            EndBlendMode();

        } else {
            _DrawSheet(s, frame, p.pos, rot, WHITE);
        }
    }
}

void ProjectileManager::DrawGlow(float t) const {
    for (const auto& p : _proj) {
        if (!p.active || p.type != ProjType::LASER) continue;

        float lifeRatio = p.life / STATS[(int)ProjType::LASER].life;
        float alpha     = std::min(lifeRatio * 3.0f, 1.0f);

        float rad = p.rotation * DEG2RAD;
        Vector2 dir  = { cosf(rad), sinf(rad) };
        Vector2 tail = { p.pos.x - dir.x * 38.0f, p.pos.y - dir.y * 38.0f };

        BeginBlendMode(BLEND_ADDITIVE);
            // Halo diffus — c'est lui qui sera flouté par le shader
            DrawLineEx(tail, p.pos, 14.0f,
                       ColorAlpha({ 60, 140, 255, 255 }, alpha * 0.12f));
            DrawLineEx(tail, p.pos, 7.0f,
                       ColorAlpha({ 100, 180, 255, 255 }, alpha * 0.2f));
        EndBlendMode();
    }
}

void ProjectileManager::SpawnEnemy(Vector2 origin, float angleDeg) {
    Vector2 dir = AngleToDir(angleDeg);

    Projectile p{};
    p.type     = ProjType::BULLET;         // Les ennemis tirent des bullets
    p.pos      = origin;
    p.vel      = { dir.x * 520.0f, dir.y * 520.0f };
    p.rotation = angleDeg;
    p.life     = 2.0f;
    p.active   = true;
    p.isEnemy  = true;                     // ← marqueur
    p.damage   = 12.0f;
    _proj.push_back(p);
}

void ProjectileManager::Clear() { _proj.clear(); }