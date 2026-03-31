#pragma once
#include "raylib.h"
#include "../ship/ShipStats.hpp"
#include <string>
#include <vector>

// Notification flottante (dégâts, événements)
struct HUDNotification {
    std::string text;
    Vector2     pos;
    float       life;       // 0 → 1 (1 = vivant)
    Color       color;
};

class HUD {
public:
    HUD();
    void LoadFont(const char* path, int size = 20);
    void Update(float dt, const ShipStats& stats);
    void Draw(const ShipStats& stats, int screenW, int screenH) const;
    void PushNotification(const std::string& text, Color color = WHITE);
    void Unload();
    Font GetFont() const { return _font; }

private:
    Font  _font;
    bool  _fontLoaded = false;

    // Animations internes
    float _shieldFlash   = 0.0f;  // Flash rouge quand le bouclier prend un coup
    float _hullFlash     = 0.0f;
    float _shieldPrev    = 0.0f;  // Pour détecter les changements
    float _hullPrev      = 0.0f;

    std::vector<HUDNotification> _notifications;

    // Helpers de rendu
    void _DrawBar(Vector2 pos, float w, float h,
                  float pct, Color fill, Color bg,
                  const char* label, float labelSize) const;
    void _DrawFuelPips(Vector2 pos, int current, int max) const;
    void _DrawShipStatus(Vector2 pos, const ShipStats& stats) const;
    void _DrawText(const char* text, Vector2 pos,
                   float size, Color color) const;
    float _MeasureText(const char* text, float size) const;
};