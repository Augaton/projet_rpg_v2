#pragma once
#include "../ship/ShipFactory.hpp"
#include <string>
#include <vector>

// Progression persistante entre les parties : records, historique, vaisseaux débloqués
struct RunRecord {
    int  ship       = 0;
    int  difficulty = 1;
    int  sector     = 1;
    bool won        = false;
    int  kills      = 0;
    int  scrap      = 0;
};

struct Profile {
    int  runs        = 0;
    int  wins        = 0;
    int  bestSector  = 0;
    int  totalKills  = 0;
    int  totalScrap  = 0;
    bool unlocked[PLAYER_SHIP_COUNT] = { true, false, false };
    std::vector<RunRecord> history;     // plus récente en premier, 8 max

    void Load();
    void Save() const;

    // Enregistre une partie terminée ; renvoie les vaisseaux nouvellement débloqués
    std::vector<int> RecordRun(const RunRecord& run);

    std::string Serialize() const;
    void        Parse(const std::string& text);

private:
    void _CheckUnlocks(std::vector<int>& newlyUnlocked);
};
