#include "PostFX.hpp"
#include "../core/Config.hpp"
#include "rlgl.h"
#include <algorithm>

void PostFX::Load() {
    _bg   = LoadRenderTexture(BG_W, BG_H);
    _glow = LoadRenderTexture(GLOW_W, GLOW_H);
    _ping = LoadRenderTexture(GLOW_W, GLOW_H);
    SetTextureFilter(_bg.texture,   TEXTURE_FILTER_POINT);
    SetTextureFilter(_glow.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_ping.texture, TEXTURE_FILTER_BILINEAR);

    _bloom = LoadShader(0, "src/ressources/shader/bloom.fs");
    _bloomOk = IsShaderValid(_bloom) && _bloom.id != rlGetShaderIdDefault();
    if (_bloomOk) {
        _dirLoc   = GetShaderLocation(_bloom, "direction");
        _texelLoc = GetShaderLocation(_bloom, "texelSize");
        _gainLoc  = GetShaderLocation(_bloom, "gain");
        float texel[2] = { 1.0f / GLOW_W, 1.0f / GLOW_H };
        SetShaderValue(_bloom, _texelLoc, texel, SHADER_UNIFORM_VEC2);
    }

    _crt = LoadShader(0, "src/ressources/shader/crt.fs");
    _crtOk = IsShaderValid(_crt) && _crt.id != rlGetShaderIdDefault();
    if (_crtOk) {
        _srcSizeLoc    = GetShaderLocation(_crt, "sourceSize");
        _timeLoc       = GetShaderLocation(_crt, "time");
        _intensityLoc  = GetShaderLocation(_crt, "intensity");
        _aberrationLoc = GetShaderLocation(_crt, "aberration");
        float size[2] = { (float)VIRTUAL_W, (float)VIRTUAL_H };
        SetShaderValue(_crt, _srcSizeLoc, size, SHADER_UNIFORM_VEC2);
    }
    if (!_bloomOk || !_crtOk) TraceLog(LOG_WARNING, "PostFX: shader indisponible, effet désactivé");
}

void PostFX::Unload() {
    UnloadRenderTexture(_bg);
    UnloadRenderTexture(_glow);
    UnloadRenderTexture(_ping);
    if (_bloomOk) UnloadShader(_bloom);
    if (_crtOk)   UnloadShader(_crt);
}

// ─── Fond ────────────────────────────────────────────────────────────────────

void PostFX::BeginBackground() {
    BeginTextureMode(_bg);
    ClearBackground(BLACK);
}

void PostFX::EndBackground() { EndTextureMode(); }

void PostFX::DrawBackground(float ftl, float impact) const {
    ftl    = std::clamp(ftl, 0.0f, 1.0f);
    impact = std::clamp(impact, 0.0f, 1.0f);

    // Copie opaque : le fond couvre tout le canevas, inutile de lire ce qu'il y avait dessous
    Color tint = { (unsigned char)(255 - 102 * ftl), (unsigned char)(255 - 38 * ftl), 255, 255 };
    rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM);
        DrawTexturePro(_bg.texture, { 0, 0, (float)BG_W, -(float)BG_H },
                       { 0, 0, (float)VIRTUAL_W, (float)VIRTUAL_H }, { 0, 0 }, 0.0f, tint);
    EndBlendMode();

    // Lueur du tunnel FTL et flash d'impact, seulement quand ils sont visibles
    if (ftl > 0.01f || impact > 0.0f) {
        BeginBlendMode(BLEND_ADDITIVE);
            if (ftl > 0.01f) DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha({ 5, 13, 31, 255 }, ftl));
            if (impact > 0.0f) DrawRectangle(0, 0, VIRTUAL_W, VIRTUAL_H, ColorAlpha(WHITE, 0.3f * impact));
        EndBlendMode();
    }
}

// ─── Bloom ───────────────────────────────────────────────────────────────────

void PostFX::BeginGlow() {
    BeginTextureMode(_glow);
    ClearBackground(BLANK);
    Camera2D cam{};
    cam.zoom = (float)GLOW_W / (float)VIRTUAL_W;
    BeginMode2D(cam);
}

void PostFX::EndGlow() {
    EndMode2D();
    EndTextureMode();
}

void PostFX::BlurGlow() {
    if (!_bloomOk) return;
    Rectangle full = { 0, 0, (float)GLOW_W, (float)GLOW_H };
    Rectangle flip = { 0, 0, (float)GLOW_W, -(float)GLOW_H };
    float h[2] = { 1.0f, 0.0f }, v[2] = { 0.0f, 1.0f };
    // Le gain de la passe verticale double l'intensité : une seule composition suffit ensuite
    float gainH = 1.0f, gainV = 2.0f;

    BeginTextureMode(_ping);
        ClearBackground(BLANK);
        SetShaderValue(_bloom, _dirLoc, h, SHADER_UNIFORM_VEC2);
        SetShaderValue(_bloom, _gainLoc, &gainH, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(_bloom);
            DrawTexturePro(_glow.texture, flip, full, { 0, 0 }, 0.0f, WHITE);
        EndShaderMode();
    EndTextureMode();

    BeginTextureMode(_glow);
        ClearBackground(BLANK);
        SetShaderValue(_bloom, _dirLoc, v, SHADER_UNIFORM_VEC2);
        SetShaderValue(_bloom, _gainLoc, &gainV, SHADER_UNIFORM_FLOAT);
        BeginShaderMode(_bloom);
            DrawTexturePro(_ping.texture, flip, full, { 0, 0 }, 0.0f, WHITE);
        EndShaderMode();
    EndTextureMode();
}

void PostFX::DrawBloom() const {
    Rectangle flip = { 0, 0, (float)GLOW_W, -(float)GLOW_H };
    BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(_glow.texture, flip, { 0, 0, (float)VIRTUAL_W, (float)VIRTUAL_H }, { 0, 0 }, 0.0f, WHITE);
    EndBlendMode();
}

// ─── Présentation ────────────────────────────────────────────────────────────

void PostFX::Present(const Texture2D& canvas, float time, int crtLevel, float impact) {
    float sw = (float)GetScreenWidth(), sh = (float)GetScreenHeight();
    float scale = std::min(sw / VIRTUAL_W, sh / VIRTUAL_H);
    Rectangle dst = { (sw - VIRTUAL_W * scale) * 0.5f, (sh - VIRTUAL_H * scale) * 0.5f,
                      VIRTUAL_W * scale, VIRTUAL_H * scale };
    Rectangle src = { 0, 0, (float)VIRTUAL_W, -(float)VIRTUAL_H };

    BeginDrawing();
        ClearBackground(BLACK);
        bool useCrt = _crtOk && crtLevel > 0;
        if (useCrt) {
            float intensity  = crtLevel >= 2 ? 1.0f : 0.5f;
            float aberration = 0.6f * intensity + std::clamp(impact, 0.0f, 1.0f) * 3.0f;
            SetShaderValue(_crt, _timeLoc, &time, SHADER_UNIFORM_FLOAT);
            SetShaderValue(_crt, _intensityLoc, &intensity, SHADER_UNIFORM_FLOAT);
            SetShaderValue(_crt, _aberrationLoc, &aberration, SHADER_UNIFORM_FLOAT);
            BeginShaderMode(_crt);
        }
        // Copie opaque : l'alpha du canevas (réduit par les éléments translucides) est ignoré
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
            DrawTexturePro(canvas, src, dst, { 0, 0 }, 0.0f, WHITE);
        EndBlendMode();
        if (useCrt) EndShaderMode();
    EndDrawing();
}
