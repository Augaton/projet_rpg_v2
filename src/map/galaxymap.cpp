#include "galaxymap.hpp"
#include <math.h>

GalaxyMap::GalaxyMap() {}

void GalaxyMap::Generate(int screenW, int screenH) {
    beacons.clear();
    // Génération de quelques points aléatoires
    for (int i = 0; i < 6; i++) {
        beacons.push_back({
            { (float)GetRandomValue(150, screenW - 150), (float)GetRandomValue(150, screenH - 150) },
            false,
            GetRandomValue(0, 3), // Choisi un des 4 secteurs définis dans ton main
            "Secteur Inconnu"
        });
    }
}

void GalaxyMap::Update(Vector2 mousePos) {
    if (!active) return;

    hoveredIndex = -1;
    // Utilisation de size_t pour correspondre au type de beacons.size()
    for (size_t i = 0; i < beacons.size(); i++) {
        if (CheckCollisionPointCircle(mousePos, beacons[i].pos, 20)) {
            hoveredIndex = (int)i; // On cast en int pour stocker dans l'index
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                selectedSectorIdx = beacons[i].sectorType;
                jumpRequested = true;
                active = false;
            }
            break;
        }
    }
}

void GalaxyMap::Draw(Font font) {
    if (!active) return;

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, 0.8f));
    DrawTextEx(font, "CARTE DE NAVIGATION (SELECTIONNEZ UNE DESTINATION)", { 50, 50 }, 22, 1, SKYBLUE);

    // Sécurité : on vérifie que le vecteur n'est pas vide avant de faire .size() - 1
    if (beacons.size() > 1) {
        for (size_t i = 0; i < beacons.size() - 1; i++) {
            DrawLineEx(beacons[i].pos, beacons[i+1].pos, 2.0f, ColorAlpha(DARKGRAY, 0.5f));
        }
    }

    for (size_t i = 0; i < beacons.size(); i++) {
        Color c = ((int)i == hoveredIndex) ? GOLD : SKYBLUE;
        DrawCircleLinesV(beacons[i].pos, 15, c);
        DrawCircleV(beacons[i].pos, 5, c);
        
        if ((int)i == hoveredIndex) {
            DrawTextEx(font, beacons[i].name, { beacons[i].pos.x + 20, beacons[i].pos.y - 10 }, 18, 1, GOLD);
        }
    }
}