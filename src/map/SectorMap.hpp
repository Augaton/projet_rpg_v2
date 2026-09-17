#pragma once
#include "raylib.h"
#include <algorithm>
#include <vector>

enum class BeaconKind {
    START,
    EMPTY,
    COMBAT,
    STORE,
    DISTRESS,
    DERELICT,
    NEBULA,
    EXIT,
    BOSS,
};
inline constexpr int BEACON_KIND_COUNT = 9;

// Environnement dangereux d'une balise (effets pendant les combats)
enum class BeaconHazard {
    NONE,
    ASTEROIDS,     // astéroïdes qui frappent les deux vaisseaux
    SOLAR_FLARE,   // éruptions solaires périodiques qui traversent les boucliers
    ION_STORM,     // boucliers ralentis, impulsions ioniques
};
inline constexpr int HAZARD_COUNT = 4;
const char* HazardName(BeaconHazard h);

struct Beacon {
    Vector2          pos;
    int              column  = 0;
    BeaconKind       kind    = BeaconKind::EMPTY;
    bool             visited = false;
    BeaconHazard           hazard  = BeaconHazard::NONE;
    std::vector<int> links;
};

struct SectorDef {
    float       seed;
    Color       c1;
    Color       c2;
    const char* name;
};

inline constexpr SectorDef SECTORS[] = {
    {  1.1f, { 200,  80,  40, 255 }, {  40,  40,  50, 255 }, "TARANTULA"  },
    {  4.5f, { 180,  60,  20, 255 }, {  10,  20,  40, 255 }, "GÉANTE ROUGE" },
    {  8.9f, {  40, 150, 180, 255 }, { 200, 100,  40, 255 }, "FAILLE D'AZUR" },
    { 12.3f, { 120,  80, 150, 255 }, { 255, 200, 150, 255 }, "COURONNE DU VIDE" },
};

// État sérialisable de la carte
struct MapState {
    int                 sector  = 0;
    int                 current = 0;
    float               fleet   = 0.0f;
    std::vector<Beacon> beacons;
};

class SectorMap {
public:
    static constexpr int   COLUMNS         = 8;
    static constexpr int   MAX_BEACONS     = 64;
    static constexpr float FLEET_PER_JUMP  = 0.8f;
    static constexpr float FLEET_NEBULA    = 0.45f;

    void Generate(int sectorIdx, bool finalSector);

    MapState Export() const;
    bool     Import(const MapState& state);   // valide entièrement l'état

    void Open(bool closable = true);
    void Close()                { _open = false; }
    bool IsOpen() const         { return _open; }
    bool IsClosable() const     { return _closable; }

    // Renvoie l'index de la balise choisie par le joueur, -1 sinon
    int  Update(float dt, int fuel);
    void Draw(int fuel, float t);

    // Renvoie true si la balise avait déjà été visitée
    bool JumpTo(int idx);
    // Bornée à la plage acceptée par la sauvegarde (sinon une très longue partie deviendrait illisible)
    void AdvanceFleet(float columns) { _fleet = std::clamp(_fleet + columns, -5.0f, (float)COLUMNS + 2.0f); }
    bool IsOvertaken(int idx) const;
    int  FleetEta(int idx) const;          // sauts avant que la flotte n'atteigne la balise
    int  JumpsToExit(int idx) const;       // -1 si inaccessible

    int           CurrentIdx() const { return _current; }
    const Beacon& Current() const    { return _beacons[_current]; }
    int           Sector() const     { return _sector; }
    void          SetSensorLevel(int level) { _sensors = level; }

    bool CloseRequested() const    { return _closeRequested; }
    bool DistressRequested() const { return _distressRequested; }
    void ClearRequests()           { _closeRequested = _distressRequested = false; }

    static const char* KindName(BeaconKind k);

private:
    std::vector<Beacon> _beacons;
    std::vector<int>    _distExit;
    int   _sector   = 0;
    int   _current  = 0;
    int   _hovered  = -1;
    int   _sensors  = 0;
    float _fleet    = -1.5f;
    bool  _open     = false;
    bool  _closable = true;
    float _openAnim = 0.0f;
    bool  _closeRequested    = false;
    bool  _distressRequested = false;

    bool  _IsLinked(int a, int b) const;
    bool  _KindKnown(int idx) const;
    float _ColumnX(float column) const;
    void  _Link(int a, int b);
    void  _ComputeDistances();
    // Voisins triés de haut en bas (tampon réutilisé : aucune allocation par frame)
    const std::vector<int>& _Reachable() const;
    mutable std::vector<int> _reachBuf;
};
