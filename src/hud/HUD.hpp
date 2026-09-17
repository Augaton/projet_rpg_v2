#pragma once
#include "raylib.h"

class AudioManager;
#include "../ship/Ship.hpp"
#include <string>
#include <vector>

class HUD {
public:
    void SetAudio(AudioManager* audio) { _audio = audio; }
    void Update(float dt, const Ship& player);

    // Clics sur les éléments du joueur (énergie, armes).
    // Renvoie true si la souris est au-dessus d'un élément (clic consommé).
    bool HandleInput(Ship& player, int* selectedWeapon, bool inCombat);

    void DrawPlayer(const Ship& player, int selectedWeapon, bool inCombat, float t) const;
    void DrawFtlPanel(const Ship& player, bool inCombat, const char* label, bool enabled) const;
    // fleeState : 0 aucune, 1 intention (valeur = secondes), 2 charge (valeur = 0..1)
    void DrawEnemy(const Ship& enemy, int fleeState, float fleeValue, float t) const;
    void DrawLog() const;

    void Log(const std::string& text, Color color);
    void ClearLog() { _log.clear(); }
    void ResetVisuals() { _hullDisplayed = -1.0f; }

    // Arme en cours de glisser-déposer (-1 sinon)
    int  DraggedWeapon() const { return _dragging ? _dragSlot : -1; }
    // Arme lâchée hors du bandeau cette frame : renvoie true une seule fois
    bool TakeWeaponDrop(int& slot, Vector2& pos) {
        if (_dropSlot < 0) return false;
        slot = _dropSlot; pos = _dropPos; _dropSlot = -1;
        return slot < MAX_WEAPON_SLOTS;
    }

    static Rectangle JumpButtonRect();
    static Rectangle SystemColumnRect(int slot);
    static int       VisibleSystems(const Ship& player, int out[SYS_COUNT]);
    static Rectangle WeaponCardRect(int slot);
    static Vector2   ScrapAnchor();     // cible des éclats de ferraille
    static Rectangle AugmentRect(int slot);

private:
    struct LogLine {
        std::string text;
        Color       color;
        float       age;
    };
    std::vector<LogLine> _log;
    AudioManager*        _audio = nullptr;

    float _hullDisplayed = -1.0f;
    float _hullLag       = -1.0f;
    float _hullFlash     = 0.0f;
    float _shieldFlash   = 0.0f;
    int   _prevLayers    = 0;
    int   _prevHull      = 0;
    int   _prevScrap     = -1;
    float _scrapBump     = 0.0f;
    int   _hoverSys      = -1;
    int   _hoverWeapon   = -1;
    int   _hoverAug      = -1;
    // Glisser-déposer des cartes d'armes (clic court = alimenter / sélectionner)
    int     _dragSlot  = -1;
    bool    _dragging  = false;
    Vector2 _dragStart = { 0, 0 };
    Vector2 _dragMouse = { 0, 0 };
    int     _dropSlot  = -1;
    Vector2 _dropPos   = { 0, 0 };
    float _prevCharge[MAX_WEAPON_SLOTS] = {};
    float _readyFlash[MAX_WEAPON_SLOTS] = {};

    void _DrawStatus(const Ship& p, bool inCombat, float t) const;
    void _DrawSystems(const Ship& p) const;
    void _DrawWeapons(const Ship& p, int selected, bool inCombat, float t) const;
    void _DrawWeaponCard(const Ship& p, int i, Rectangle r, int selected, bool inCombat, float t) const;
    int  _WeaponSlotAt(const Ship& p, Vector2 m) const;
    void _ClickWeapon(Ship& p, int i, int* selected, bool inCombat);
    void _DropWeapon(Ship& p, Vector2 m, int* selected, bool inCombat);
    void _DrawTooltip(const Ship& p) const;
    void _DrawHullBar(const Ship& s, Rectangle area, float displayed, float lag, float flash) const;
    float _DrawShieldCells(const Ship& s, float x, float y, float flash, bool alignRight) const;
};
