#pragma once
#include "../ship/Ship.hpp"
#include <vector>

class HUD;
class AudioManager;

class Store {
public:
    // seed : identifie la balise pour garder le même stock si on revient
    void Generate(int sector, int seed);

    void Open()                   { _open = true; _closeRequested = false; }
    void Close()                  { _open = false; }
    bool IsOpen() const           { return _open; }
    bool CloseRequested() const   { return _closeRequested; }

    // Interface immédiate : dessine et applique les achats
    void Draw(Ship& player, HUD& hud, AudioManager& audio);


private:
    struct Offer {
        int  weaponId;
        bool sold;
    };

    std::vector<Offer> _offers;
    std::vector<int>   _modules;
    int                _augment     = -1;
    bool               _augmentSold = false;
    int  _sector         = 0;
    int  _seed           = -1;
    bool _open           = false;
    bool _closeRequested = false;
};
