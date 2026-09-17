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
    void SetLevels(float levels) { _levels = levels; _dirty = true; }
    float GetFtlLerp() const { return ftlLerp; }
    // La nébuleuse évolue lentement : au repos, un rendu sur deux suffit (~1 ms GPU gagnée).
    // Toujours vrai pendant un saut FTL, un impact ou après un changement de secteur.
    bool ShouldRedraw() {
        bool calm = ftlLerp < 0.01f && impactAnim <= 0.0f;
        bool redraw = _dirty || !calm || (++_frame & 1u) == 0u;
        _dirty = false;
        return redraw;
    }

private:
    Shader shader;
    Texture2D whiteTex;
    
    int timeLoc, resLoc, ftlLoc, impactLoc;
    int seedLoc, col1Loc, col2Loc, levelsLoc;

    float time;
    float ftlLerp;
    float impactAnim;
    
    float currentSeed = 1.0f;
    float _levels     = 12.0f;
    bool  _dirty      = true;
    unsigned _frame   = 0;
    Vector3 color1 = {0.1f, 0.1f, 0.2f};
    Vector3 color2 = {0.2f, 0.05f, 0.3f};
};

#endif