#include "SpaceBackground.hpp"
#include <algorithm>

SpaceBackground::SpaceBackground() : time(0.0f), ftlLerp(0.0f), impactAnim(0.0f) {}

void SpaceBackground::Load(const char* shaderPath) {
    shader = LoadShader(0, shaderPath);
    timeLoc   = GetShaderLocation(shader, "iTime");
    resLoc    = GetShaderLocation(shader, "iResolution");
    ftlLoc    = GetShaderLocation(shader, "iFTL");
    impactLoc = GetShaderLocation(shader, "iImpact");
    
    // Récupérer les nouvelles adresses dans le shader
    seedLoc   = GetShaderLocation(shader, "iSeed");
    col1Loc   = GetShaderLocation(shader, "iColorPrimary");
    col2Loc   = GetShaderLocation(shader, "iColorSecondary");

    Image white = GenImageColor(1, 1, WHITE);
    whiteTex = LoadTextureFromImage(white);
    UnloadImage(white);
}

void SpaceBackground::SetSector(float seed, Color c1, Color c2) {
    currentSeed = seed;
    color1 = {(float)c1.r/255.0f, (float)c1.g/255.0f, (float)c1.b/255.0f};
    color2 = {(float)c2.r/255.0f, (float)c2.g/255.0f, (float)c2.b/255.0f};
}

void SpaceBackground::Update(float dt, float ftlTarget, bool impacted) {
    time += dt;
    ftlLerp += (ftlTarget - ftlLerp) * 3.0f * dt;
    if (impacted) impactAnim = 1.0f;
    impactAnim = std::max(0.0f, impactAnim - 4.0f * dt);

    float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    
    SetShaderValue(shader, timeLoc,   &time,        SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, resLoc,    res,          SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, ftlLoc,    &ftlLerp,     SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, impactLoc, &impactAnim,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, seedLoc, &currentSeed, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, col1Loc, &color1, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, col2Loc, &color2, SHADER_UNIFORM_VEC3);
}
void SpaceBackground::Draw(int screenWidth, int screenHeight) {
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

SpaceBackground::~SpaceBackground() {
    // La sécurité avant tout
}