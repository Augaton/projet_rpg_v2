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

private:
    Shader shader;
    Texture2D whiteTex; // Pour le support UV du shader
    
    // Locations
    int timeLoc;
    int resLoc;
    int ftlLoc;
    int impactLoc;

    // État interne
    float time;
    float ftlLerp;
    float impactAnim;
};

#endif