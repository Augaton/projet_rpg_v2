#pragma once
#include "../map/SectorMap.hpp"
#include "../ship/Ship.hpp"
#include "../ship/ShipFactory.hpp"
#include <string>

struct RunStats {
    int jumps  = 0;
    int kills  = 0;
    int scrap  = 0;
    int events = 0;
};

// Point de reprise de la sauvegarde automatique
enum class ResumePoint {
    BEACON  = 0,   // à l'arrêt sur une balise
    ARRIVAL = 1,   // vient d'arriver : l'événement de la balise sera rejoué
    COMBAT  = 2,   // le combat reprend depuis le début
};

struct SaveData {
    MapState    map;
    Ship        ship;
    RunStats    stats;
    ResumePoint resume     = ResumePoint::BEACON;
    EnemyKind   combatKind = EnemyKind::NORMAL;
    int         difficulty = 1;
};

namespace save {

bool Exists();
bool Write(const SaveData& data);
// Charge et valide ; renvoie false (avec un message) si le fichier est absent ou invalide
bool Read(SaveData& data, std::string& error);
void Delete();

// Exposés pour les tests
std::string Serialize(const SaveData& data);
bool        Deserialize(const std::string& text, SaveData& data, std::string& error);

} // namespace save
