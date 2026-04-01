#pragma once
#include "raylib.h"
#include "../game/GameState.hpp"
#include <vector>
#include <string>

// ─── Données passées depuis main ─────────────────────────────────────────────

struct SectorDef {
    float       seed;
    Color       c1;
    Color       c2;
    const char* name;
};

// ─── Nœud de la carte ────────────────────────────────────────────────────────

enum class NodeThreat { LOW, MEDIUM, HIGH };

// Événement rencontré lorsque le joueur saute sur ce nœud
enum class NodeEvent { EMPTY, COMBAT, MERCHANT, DERELICT };

struct MapNode {
    Vector2    pos;
    int        sectorIdx;
    bool       visited;
    bool       isCurrentPos;
    bool       isExit;          // Nœud de sortie de zone
    NodeThreat threat;
    NodeEvent  event;           // Type d'événement à ce nœud
    int        column;          // Colonne dans la grille (0 = départ)
    std::vector<int> links;
};

class GalaxyMap {
public:
    GalaxyMap() = default;

    void Generate(int screenW, int screenH,
                  const SectorDef* sectors, int sectorCount);
    void Regenerate(int screenW, int screenH);   // Nouvelle zone (même secteurs)

    void Update(Vector2 mousePos, float dt);
    void Draw(Font font) const;

    void Toggle()                { _active = !_active; _scanTimer = 0.0f; }
    bool IsActive()        const { return _active; }
    bool IsJumpRequested() const { return _jumpRequested; }
    int  GetTargetSector() const { return _targetSectorIdx; }
    void ResetJumpRequest()      { _jumpRequested = false; }
    void MarkCurrentVisited(int nodeIdx);
    void SetFuelInfo(int fuel, int maxFuel) { _playerFuel = fuel; _playerMaxFuel = maxFuel; }

    // Infos sur le dernier saut
    NodeEvent GetLastJumpEvent()  const { return _lastJumpEvent; }
    bool      IsLastJumpToExit()  const { return _lastJumpIsExit; }
    int       GetZoneLevel()      const { return _zoneLevel; }

private:
    std::vector<MapNode> _nodes;
    const SectorDef*     _sectors      = nullptr;
    int                  _sectorCount  = 0;
    int                  _screenW      = 800;
    int                  _screenH      = 600;
    int                  _zoneLevel    = 0;

    bool  _active         = false;
    bool  _jumpRequested  = false;
    int   _targetSectorIdx = -1;
    int   _hoveredIdx     = -1;
    int   _currentNodeIdx = 0;
    int   _playerFuel     = 0;
    int   _playerMaxFuel  = 20;

    NodeEvent _lastJumpEvent  = NodeEvent::EMPTY;
    bool      _lastJumpIsExit = false;

    // Animations
    float _scanTimer   = 0.0f;
    float _pulseTimer  = 0.0f;
    float _openTimer   = 0.0f;

    void _BuildNodes();      // Génération colonne par colonne
    void _DrawConnection(const MapNode& a, const MapNode& b, bool reachable) const;
    void _DrawNode(const MapNode& n, int idx, Font font) const;
    void _DrawScanLine() const;
    void _DrawHeader(Font font) const;
    void _DrawTooltip(Font font) const;

    Color _ThreatColor(NodeThreat t) const;
    bool  _IsReachable(int idx) const;
};