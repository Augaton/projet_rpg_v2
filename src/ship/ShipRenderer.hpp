#pragma once
#include "raylib.h"
#include "Ship.hpp"
#include "Hulls.hpp"

// Position / orientation d'un vaisseau à l'écran
struct ShipView {
    Vector2 pos      = { 0, 0 };
    float   rotation = 90.0f;   // 90 = vers la droite (joueur), 270 = vers la gauche
    float   destroyT = -1.0f;   // >= 0 : animation de destruction en cours
    Vector2 kick     = { 0, 0 };  // recul visuel (tir, impact), amorti par le combat
    float   thrust   = 0.0f;    // 0..1 : réacteurs poussés (préparation de fuite)
    float   alpha    = 1.0f;    // < 1 : vaisseau invisible
    float   scaleMult = 1.0f;   // aperçus (écran de sélection)
};

class ShipRenderer {
public:
    void Load();
    void Unload();

    void DrawShip(const Ship& ship, const ShipView& view, float t) const;
    void DrawShield(const Ship& ship, const ShipView& view, float t) const;
    // targetMask[s] : bits des armes du joueur qui visent le système s
    void DrawRooms(const Ship& ship, const ShipView& view, bool hostile,
                   int hoveredSys, const int* targetMask, float t) const;

    Vector2 RoomPos(const Ship& ship, const ShipView& view, int sys) const;
    Vector2 MuzzlePos(const Ship& ship, const ShipView& view) const;
    Vector2 ShieldRadii(const Ship& ship) const;
    int     RoomAt(const Ship& ship, const ShipView& view, Vector2 point) const;
    float   DestructionDuration(const Ship& ship) const;
    float   Size(const Ship& ship) const;   // taille à l'écran (px)

private:
    struct Sheet {
        Texture2D tex{};
        int       frames = 0;
        bool      Valid() const { return tex.id > 0 && frames > 0; }
        float     FrameSize() const { return (float)tex.height; }
    };
    struct Visual {
        Sheet base, engine, shield, weapons, destruction;
    };

    Visual _visuals[HULL_COUNT];

    void _DrawFrame(const Sheet& s, int frame, const ShipView& view, float scale, Color tint) const;
};
