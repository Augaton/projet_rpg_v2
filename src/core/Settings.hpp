#pragma once
#include <string>

struct Settings {
    float masterVolume   = 0.8f;
    float musicVolume    = 0.6f;
    float sfxVolume      = 0.8f;
    bool  fullscreen     = false;
    bool  vsync          = true;
    int   fpsCap         = 0;      // 0 = synchro écran (ou illimité sans vsync)
    int   crt            = 1;      // 0 aucun, 1 léger, 2 fort
    float shake          = 1.0f;   // 0..1
    bool  reduceFlashes  = false;
    bool  pauseOnFocus   = true;   // pause auto quand la fenêtre perd le focus
    int   textSpeed      = 1;      // 0 instantané, 1 normal, 2 rapide
    bool  tutorial       = true;   // guide du premier combat
    bool  autoPause      = false;  // pause auto : début de combat, fuite, coque critique, boss

    // Chargement tolérant : clés inconnues ignorées, valeurs bornées
    void Load();
    void Save() const;

    std::string Serialize() const;
    void        Parse(const std::string& text);
};
