#ifndef SPACE_BACKGROUND_HPP
#define SPACE_BACKGROUND_HPP

#include "raylib.h"

// Fond procédural (nébuleuse, soleil, planète) propre à chaque balise.
// Le shader est coûteux : il n'est exécuté que lorsque la balise change ;
// le saut FTL et le flash d'impact sont appliqués à part, en surimpression (PostFX::DrawBackground).
class SpaceBackground {
public:
    void Load(const char* shaderPath);
    void Update(float dt, float ftlTarget, bool impacted);
    void Draw(int screenWidth, int screenHeight);
    void Unload();
    void SetSector(float seed, Color c1, Color c2);
    void SetLevels(float levels) { _levels = levels; _dirty = true; }
    float GetFtlLerp() const { return ftlLerp; }
    float Impact() const     { return impactAnim; }
    // Vrai une seule fois après un changement de balise
    bool ShouldRedraw() {
        bool redraw = _dirty;
        _dirty = false;
        return redraw;
    }

private:
    Shader shader{};
    Texture2D whiteTex{};

    int resLoc = -1, seedLoc = -1, col1Loc = -1, col2Loc = -1, levelsLoc = -1;

    float ftlLerp    = 0.0f;
    float impactAnim = 0.0f;

    float currentSeed = 1.0f;
    float _levels     = 12.0f;
    bool  _dirty      = true;
    Vector3 color1 = {0.1f, 0.1f, 0.2f};
    Vector3 color2 = {0.2f, 0.05f, 0.3f};
};

#endif
