#pragma once
#include "../ship/Ship.hpp"

class HUD;
class AudioManager;

// Écran « VAISSEAU » : amélioration du réacteur et des modules installés
class ShipMenu {
public:
    void Open()                 { _open = true; _closeRequested = false; _hovered = -1; }
    void Close()                { _open = false; }
    bool IsOpen() const         { return _open; }
    bool CloseRequested() const { return _closeRequested; }

    // Interface immédiate : dessine et applique les améliorations
    void Draw(Ship& player, HUD& hud, AudioManager& audio, float t);

private:
    bool _open           = false;
    bool _closeRequested = false;
    int  _hovered        = -1;
};
