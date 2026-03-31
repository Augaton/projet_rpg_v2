#ifndef SPACE_BACKGROUND_HPP
#define SPACE_BACKGROUND_HPP

#include "raylib.h"

class SpaceBackground {
public:
    SpaceBackground();
    ~SpaceBackground();

    void Load(const char* shaderPath);
    void Update(float dt, float ftlTarget, bool impacted);
    void Draw(int screenWidth, int screenHeight);
    void Unload();
    void SetSector(float seed, Color c1, Color c2);
    float GetFtlLerp() const { return ftlLerp; }

private:
    Shader shader;
    Texture2D whiteTex;
    
    int timeLoc, resLoc, ftlLoc, impactLoc;
    int seedLoc, col1Loc, col2Loc;

    float time;
    float ftlLerp;
    float impactAnim;
    
    float currentSeed = 1.0f;
    Vector3 color1 = {0.1f, 0.1f, 0.2f};
    Vector3 color2 = {0.2f, 0.05f, 0.3f};
};

#endif