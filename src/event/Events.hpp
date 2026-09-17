#pragma once
#include "../map/SectorMap.hpp"
#include "../ship/Ship.hpp"
#include "../ship/ShipFactory.hpp"
#include <string>
#include <vector>

constexpr int RANDOM_WEAPON  = -2;
constexpr int RANDOM_AUGMENT = -2;

struct Outcome {
    const char* text     = "";
    int  scrap           = 0;
    int  fuel            = 0;
    int  missiles        = 0;
    int  hull            = 0;
    int  weapon          = -1;     // id d'arme, RANDOM_WEAPON ou -1
    int  augment         = -1;     // id d'augmentation, RANDOM_AUGMENT ou -1
    int  reactor         = 0;
    int  upgradeSys      = -1;
    bool combat          = false;
    EnemyKind enemy      = EnemyKind::NORMAL;
    bool store           = false;
    bool nextSector      = false;
    float fleet          = 0.0f;   // colonnes ajoutées à la flotte (négatif = retard)
};

struct Choice {
    const char* label   = "";
    int     reqScrap    = 0;
    int     reqFuel     = 0;
    int     reqMissiles = 0;
    int     reqSys      = -1;
    int     reqLevel    = 0;
    float   chance      = 1.0f;
    Outcome success     = {};
    Outcome failure     = {};
};

struct EventDef {
    const char*         title;
    const char*         body;
    std::vector<Choice> choices;
};

// Événement aléatoire adapté au type de balise
const EventDef& PickEvent(BeaconKind kind);
const std::vector<EventDef>& DerelictEvents();
const EventDef& IntroEvent();
const EventDef& PatrolEvent();
const EventDef& FleetEvent();
const EventDef& RevisitEvent(BeaconKind kind);

bool        ChoiceAvailable(const Choice& c, const Ship& ship);
std::string ChoiceRequirement(const Choice& c);

// Applique l'issue au vaisseau et renvoie les lignes de résumé (+20 ferraille…)
std::vector<std::string> ApplyOutcome(const Outcome& o, Ship& ship);
