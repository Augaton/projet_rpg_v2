#pragma once
#include "raylib.h"

// Chaîne de rendu : fond basse résolution tramé, bloom quart de résolution,
// composition finale façon écran cathodique.
// Pensée pour les GPU intégrés : peu de passes plein écran, aucune lecture inutile du canevas.
class PostFX {
public:
    static constexpr int BG_W   = 640;
    static constexpr int BG_H   = 360;
    static constexpr int GLOW_W = 320;
    static constexpr int GLOW_H = 180;

    void Load();
    void Unload();

    // Fond : dessiner dans BeginBackground/EndBackground puis DrawBackground.
    // ftl (0..1) teinte le fond en tunnel bleuté, impact (0..1) l'éclaire brièvement.
    void BeginBackground();
    void EndBackground();
    void DrawBackground(float ftl, float impact) const;

    // Bloom : dessiner les éléments lumineux (coordonnées virtuelles),
    // flouter hors de tout BeginTextureMode, puis composer dans le canevas
    void BeginGlow();
    void EndGlow();
    void BlurGlow();
    void DrawBloom() const;

    // Affiche le canevas à l'écran (letterbox) avec l'effet CRT
    void Present(const Texture2D& canvas, float time, int crtLevel, float impact);

private:
    RenderTexture2D _bg{};
    RenderTexture2D _glow{};
    RenderTexture2D _ping{};
    Shader _bloom{};
    Shader _crt{};
    int _dirLoc = -1, _texelLoc = -1, _gainLoc = -1;
    int _srcSizeLoc = -1, _timeLoc = -1, _intensityLoc = -1, _aberrationLoc = -1;
    bool _bloomOk = false, _crtOk = false;
};
