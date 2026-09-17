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
    _sheets[(int)ProjType::LASER]      = load("asset/Projectiles/PNGs/Laser.png",      4, false, -90.0f);
    _sheets[(int)ProjType::TORPEDO]    = load("asset/Projectiles/PNGs/Torpedo.png",    3, false, -90.0f);
    _sheets[(int)ProjType::WAVE]       = load("asset/Projectiles/PNGs/Wave.png",       6, false, -90.0f);
}

void ProjectileManager::Unload() {
    for (auto& s : _sheets)
        if (s.tex.id > 0) UnloadTexture(s.tex);
}

// ─── Spawn ────────────────────────────────────────────────────────────────────

int ProjectileManager::SpawnTargeted(ProjType type, Vector2 from, Vector2 to,
                                     bool isEnemy, int shotId) {
    if (_proj.size() >= MAX_PROJECTILES) return -1;

    Projectile p{};
    p.type      = type;
    p.pos       = from;
    p.life      = 10.0f;
    p.active    = true;
    p.isEnemy   = isEnemy;
    p.hasTarget = true;
    p.shotId    = shotId;
    Retarget(p, to);

    _proj.push_back(p);
    return (int)_proj.size() - 1;
}

void ProjectileManager::Retarget(Projectile& p, Vector2 to) {
    float angle = atan2f(to.y - p.pos.y, to.x - p.pos.x) * RAD2DEG;
    Vector2 dir = AngleToDir(angle);
    float   spd = STATS[(int)p.type].speed;

    p.vel       = { dir.x * spd, dir.y * spd };
    p.rotation  = angle;
    p.target    = to;
    p.hasTarget = true;
    p.arrived   = false;
}

void ProjectileManager::Release(Projectile& p, float extraLife) {
    p.hasTarget = false;
    p.arrived   = false;
    p.shotId    = -1;
    p.life      = extraLife;
}

// ─── Update ───────────────────────────────────────────────────────────────────

void ProjectileManager::Update(float dt) {
    _animTimer += dt;

    for (auto& p : _proj) {
        if (!p.active || p.arrived) continue;

        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;

        if (p.hasTarget) {
            // Arrivé quand la cible est passée "derrière" le projectile
            float dx = p.target.x - p.pos.x;
            float dy = p.target.y - p.pos.y;
            if (dx * p.vel.x + dy * p.vel.y <= 0.0f) {
                p.pos     = p.target;
                p.arrived = true;
            }
        } else {
            p.life -= dt;
            if (p.life <= 0.0f) p.active = false;
        }
    }

    _proj.erase(
        std::remove_if(_proj.begin(), _proj.end(),
            [](const Projectile& p) { return !p.active; }),
        _proj.end()
    );
}

void ProjectileManager::Clear() { _proj.clear(); }

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

void ProjectileManager::_DrawLaser(const Projectile& p, Color core, Color halo) const {
    float alpha = p.hasTarget ? 1.0f : std::clamp(p.life * 2.0f, 0.0f, 1.0f);

    float rad  = p.rotation * DEG2RAD;
    Vector2 dir = { cosf(rad), sinf(rad) };

    const float LEN = 38.0f;
    Vector2 tail = { p.pos.x - dir.x * LEN, p.pos.y - dir.y * LEN };

    DrawLineEx(tail, p.pos, 9.0f, ColorAlpha(halo, alpha * 0.15f));
    DrawLineEx(tail, p.pos, 5.0f, ColorAlpha(halo, alpha * 0.35f));
    DrawLineEx(tail, p.pos, 2.5f, ColorAlpha(core, alpha * 0.85f));
    DrawLineEx(tail, p.pos, 1.0f, ColorAlpha(WHITE, alpha));

    DrawCircleV(p.pos, 3.0f, ColorAlpha(core, alpha));
    DrawCircleV(p.pos, 1.5f, ColorAlpha(WHITE, alpha));
}

// ─── Draw ─────────────────────────────────────────────────────────────────────

void ProjectileManager::Draw() const {
    const float FPS = 12.0f;

    // Passe 1 : rochers et sprites (mélange alpha)
    bool anyLaser = false;
    for (const auto& p : _proj) {
        if (!p.active) continue;

        if (p.type == ProjType::ROCK) {
            float spin = _animTimer * 120.0f + (float)((int)(p.target.x) % 90);
            DrawPoly(p.pos, 6, 11.0f, spin, { 110, 100, 92, 255 });
            DrawPolyLinesEx(p.pos, 6, 11.0f, spin, 2.0f, { 170, 160, 150, 255 });
            DrawRectangleRec({ p.pos.x - 3, p.pos.y - 4, 4, 4 }, { 70, 64, 60, 255 });
        } else if (p.type == ProjType::LASER) {
            anyLaser = true;
        } else {
            const SpriteSheet& s = _sheets[(int)p.type];
            int frame = (int)(_animTimer * FPS) % s.frameCount;
            float alpha = p.hasTarget ? 1.0f : std::clamp(p.life * 2.0f, 0.0f, 1.0f);
            _DrawSheet(s, frame, p.pos, p.rotation + s.rotOffset,
                       ColorAlpha(STATS[(int)p.type].tint, alpha), 1.5f);
        }
    }
    if (!anyLaser) return;

    // Passe 2 : tous les lasers dans un seul bloc additif (un seul changement d'état GPU)
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& p : _proj) {
        if (!p.active || p.type != ProjType::LASER) continue;
        // Lasers ennemis en rouge, joueur en cyan
        Color core = p.isEnemy ? Color{ 255, 150, 130, 255 } : Color{ 180, 235, 255, 255 };
        Color halo = p.isEnemy ? Color{ 255,  70,  50, 255 } : Color{  80, 180, 255, 255 };
        _DrawLaser(p, core, halo);
    }
    EndBlendMode();
}

void ProjectileManager::DrawGlow() const {
    BeginBlendMode(BLEND_ADDITIVE);
    for (const auto& p : _proj) {
        if (!p.active || p.type == ProjType::ROCK) continue;

        Color glow = p.isEnemy ? Color{ 255, 90, 60, 255 } : Color{ 60, 140, 255, 255 };
        if (p.type == ProjType::WAVE)    glow = { 170, 110, 255, 255 };
        if (p.type == ProjType::TORPEDO) glow = { 255, 140, 40, 255 };

        if (p.type == ProjType::LASER) {
            float rad = p.rotation * DEG2RAD;
            Vector2 dir  = { cosf(rad), sinf(rad) };
            Vector2 tail = { p.pos.x - dir.x * 38.0f, p.pos.y - dir.y * 38.0f };
            DrawLineEx(tail, p.pos, 14.0f, ColorAlpha(glow, 0.12f));
            DrawLineEx(tail, p.pos,  7.0f, ColorAlpha(glow, 0.20f));
        } else {
            DrawCircleV(p.pos, 14.0f, ColorAlpha(glow, 0.12f));
            DrawCircleV(p.pos,  7.0f, ColorAlpha(glow, 0.22f));
        }
    }
    EndBlendMode();
}
