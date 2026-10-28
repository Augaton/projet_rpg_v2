#include "SpaceBackground.hpp"
#include "../core/Config.hpp"
#include <algorithm>

void SpaceBackground::Load(const char* shaderPath) {
    shader    = LoadShader(0, shaderPath);
    resLoc    = GetShaderLocation(shader, "iResolution");
    seedLoc   = GetShaderLocation(shader, "iSeed");
    col1Loc   = GetShaderLocation(shader, "iColorPrimary");
    col2Loc   = GetShaderLocation(shader, "iColorSecondary");
    levelsLoc = GetShaderLocation(shader, "iLevels");

    Image white = GenImageColor(1, 1, WHITE);
    whiteTex = LoadTextureFromImage(white);
    UnloadImage(white);
}

void SpaceBackground::SetSector(float seed, Color c1, Color c2) {
    currentSeed = seed;
    _dirty = true;
    color1 = {(float)c1.r/255.0f, (float)c1.g/255.0f, (float)c1.b/255.0f};
    color2 = {(float)c2.r/255.0f, (float)c2.g/255.0f, (float)c2.b/255.0f};
}

void SpaceBackground::Update(float dt, float ftlTarget, bool impacted) {
    ftlLerp += (ftlTarget - ftlLerp) * 3.0f * dt;
    if (impacted) impactAnim = 1.0f;
    impactAnim = std::max(0.0f, impactAnim - 4.0f * dt);
}

void SpaceBackground::Draw(int screenWidth, int screenHeight) {
    float res[2] = { (float)VIRTUAL_W, (float)VIRTUAL_H };
    SetShaderValue(shader, resLoc,    res,          SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, seedLoc,   &currentSeed, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, col1Loc,   &color1,      SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, col2Loc,   &color2,      SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, levelsLoc, &_levels,     SHADER_UNIFORM_FLOAT);

    BeginShaderMode(shader);
        DrawTexturePro(whiteTex,
            { 0, 0, 1, 1 },
            { 0, 0, (float)screenWidth, (float)screenHeight },
            { 0, 0 }, 0.0f, WHITE);
    EndShaderMode();
}

void SpaceBackground::Unload() {
    UnloadShader(shader);
    UnloadTexture(whiteTex);
}
