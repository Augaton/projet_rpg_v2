#include "SpaceBackground.hpp"
#include <algorithm>

SpaceBackground::SpaceBackground() : time(0.0f), ftlLerp(0.0f), impactAnim(0.0f) {}

void SpaceBackground::Load(const char* shaderPath) {
    shader = LoadShader(0, shaderPath);
    
    // Récupération des locations
    timeLoc   = GetShaderLocation(shader, "iTime");
    resLoc    = GetShaderLocation(shader, "iResolution");
    ftlLoc    = GetShaderLocation(shader, "iFTL");
    impactLoc = GetShaderLocation(shader, "iImpact");

    // Création de la texture 1x1 pour le dessin plein écran
    Image white = GenImageColor(1, 1, WHITE);
    whiteTex = LoadTextureFromImage(white);
    UnloadImage(white);
}

void SpaceBackground::Update(float dt, float ftlTarget, bool impacted) {
    time += dt;

    // Lissage du passage en FTL
    ftlLerp += (ftlTarget - ftlLerp) * 3.0f * dt;

    // Gestion du flash d'impact (décroissance rapide)
    if (impacted) impactAnim = 1.0f;
    impactAnim = std::max(0.0f, impactAnim - 4.0f * dt);

    // Mise à jour des valeurs dans le GPU
    float res[2] = { (float)GetScreenWidth(), (float)GetScreenHeight() };
    SetShaderValue(shader, timeLoc, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, resLoc, res, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, ftlLoc, &ftlLerp, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, impactLoc, &impactAnim, SHADER_UNIFORM_FLOAT);
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