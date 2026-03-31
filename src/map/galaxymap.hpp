#ifndef GALAXY_MAP_HPP
#define GALAXY_MAP_HPP

#include "raylib.h"
#include <vector>

struct Beacon {
    Vector2 pos;
    bool visited;
    int sectorType; // Index vers tes presets de couleurs/seeds
    const char* name;
};

class GalaxyMap {
public:
    GalaxyMap();
    void Generate(int screenW, int screenH);
    void Update(Vector2 mousePos);
    void Draw(Font font);
    
    bool IsJumpRequested() const { return jumpRequested; }
    int GetTargetSector() const { return selectedSectorIdx; }
    void ResetJumpRequest() { jumpRequested = false; }
    
    void Toggle() { active = !active; }
    bool IsActive() const { return active; }

private:
    std::vector<Beacon> beacons;
    bool active = false;
    bool jumpRequested = false;
    int selectedSectorIdx = -1;
    int hoveredIndex = -1;
};

#endif