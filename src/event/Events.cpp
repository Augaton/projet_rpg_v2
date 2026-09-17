#include "Events.hpp"
#include "../core/Random.hpp"
#include "../ship/Weapons.hpp"
#include <algorithm>

using EK = EnemyKind;

// ─── Base d'événements ───────────────────────────────────────────────────────

static const std::vector<EventDef>& DistressEvents() {
    static const std::vector<EventDef> list = {
        { "Signal de détresse",
          "Un cargo civil émet un appel à l'aide : deux chasseurs pirates le harcèlent. L'équipage vous supplie d'intervenir.",
          {
            { .label = "Intervenir",
              .success = { .text = "Vous vous interposez. Les pirates reportent leur attention sur vous !", .combat = true } },
            { .label = "Ignorer l'appel et partir",
              .success = { .text = "Vous coupez la fréquence. Le silence radio pèse sur la passerelle." } },
          } },
        { "Station minière en perdition",
          "Une station minière signale une fuite dans son réacteur. Le chef de station promet une récompense à qui saura la colmater.",
          {
            { .label = "Envoyer les drones de réparation", .reqSys = SYS_REPAIR, .reqLevel = 1,
              .success = { .text = "Vos drones colmatent la fuite en un temps record. Les mineurs vous paient généreusement.", .scrap = 35 } },
            { .label = "Tenter une réparation manuelle", .chance = 0.55f,
              .success = { .text = "Après des heures de travail, la fuite est maîtrisée. On vous remercie avec de la ferraille.", .scrap = 25 },
              .failure = { .text = "Une surtension traverse le câble d'amarrage et fait exploser un panneau de coque.", .hull = -4 } },
            { .label = "Partir",
              .success = { .text = "Vous reprenez votre route." } },
          } },
        { "Capsule de sauvetage",
          "Une capsule de survie dérive, balise allumée. Les capteurs détectent des signes de vie... ou un leurre.",
          {
            { .label = "Récupérer la capsule", .chance = 0.7f,
              .success = { .text = "Des réfugiés reconnaissants vous cèdent leurs provisions.", .fuel = 3, .missiles = 2 },
              .failure = { .text = "C'était un piège ! Un vaisseau pirate surgit de derrière un astéroïde.", .combat = true } },
            { .label = "Laisser la capsule",
              .success = { .text = "La capsule disparaît dans l'obscurité." } },
          } },
        { "Marchand en panne",
          "Un marchand ambulant est à court de carburant. Il propose d'échanger ce que vous pourriez lui céder.",
          {
            { .label = "Lui céder 2 carburant", .reqFuel = 2,
              .success = { .text = "Le marchand vous paie bien plus que la valeur du carburant.", .scrap = 32, .fuel = -2 } },
            { .label = "Lui vendre 3 missiles", .reqMissiles = 3,
              .success = { .text = "Il embarque vos missiles avec un sourire satisfait.", .scrap = 30, .missiles = -3 } },
            { .label = "Partir",
              .success = { .text = "Vous lui souhaitez bonne chance." } },
          } },
        { "Ingénieur itinérant",
          "Une ingénieure indépendante propose d'optimiser votre réacteur. Ses tarifs sont élevés, sa réputation excellente.",
          {
            { .label = "Payer 30 ferraille", .reqScrap = 30,
              .success = { .text = "Quelques ajustements plus tard, votre réacteur ronronne plus fort que jamais.", .scrap = -30, .reactor = 1 } },
            { .label = "Refuser poliment",
              .success = { .text = "Elle hausse les épaules et reprend sa route." } },
          } },
        { "Colons assiégés",
          "Une petite colonie minière est harcelée par des pillards. Les colons manquent de carburant pour évacuer.",
          {
            { .label = "Chasser les pillards",
              .success = { .text = "Vous vous placez entre la colonie et les pillards.", .combat = true } },
            { .label = "Leur donner 3 carburant", .reqFuel = 3,
              .success = { .text = "Les colons évacuent et vous offrent leurs réserves de munitions en remerciement.", .scrap = 22, .fuel = -3, .missiles = 3 } },
            { .label = "Passer votre chemin", .success = { .text = "Les appels à l'aide s'éteignent derrière vous." } },
          } },
        { "Drone de réparation perdu",
          "Un drone de réparation civil tourne en rond, son programme corrompu. Il émet encore un code d'appairage.",
          {
            { .label = "Reprogrammer le drone", .reqSys = SYS_HACKING, .reqLevel = 1,
              .success = { .text = "Le drone reconnaît votre vaisseau et colmate plusieurs brèches avant de s'éteindre.", .hull = 8 } },
            { .label = "Le capturer à la main", .chance = 0.5f,
              .success = { .text = "Le drone répare quelques plaques de blindage.", .hull = 4 },
              .failure = { .text = "Le drone panique et percute votre coque.", .hull = -3 } },
            { .label = "Le démonter pour les pièces", .success = { .text = "Des composants de qualité.", .scrap = 15 } },
          } },
        { "Déserteurs rebelles",
          "Un petit transport rebelle vous hèle : ses occupants veulent déserter et offrent des informations contre une escorte.",
          {
            { .label = "Accepter (flotte ralentie)", .chance = 0.8f,
              .success = { .text = "Les déserteurs vous transmettent des codes qui brouillent les capteurs de la flotte. Ils ajoutent un peu de carburant.", .fuel = 2, .fleet = -1.0f },
              .failure = { .text = "Une patrouille les a suivis ! Préparez-vous au combat.", .combat = true } },
            { .label = "Refuser",
              .success = { .text = "Le transport s'éloigne en silence." } },
          } },
    };
    return list;
}

const std::vector<EventDef>& DerelictEvents() {
    static const std::vector<EventDef> list = {
        { "Épave abandonnée",
          "Une frégate éventrée flotte à la dérive. Ses soutes semblent intactes, mais ses systèmes de défense clignotent encore.",
          {
            { .label = "Fouiller l'épave", .chance = 0.65f,
              .success = { .text = "Vous récupérez de la ferraille et quelques missiles intacts.", .scrap = 22, .missiles = 3 },
              .failure = { .text = "Une tourelle automatique s'active et touche votre coque avant d'être neutralisée.", .scrap = 8, .hull = -4 } },
            { .label = "Scanner à distance puis partir",
              .success = { .text = "Le scan révèle quelques débris récupérables.", .scrap = 6 } },
          } },
        { "Satellite militaire",
          "Un vieux satellite d'armement tourne lentement sur lui-même. Son arme principale paraît en état de marche.",
          {
            { .label = "Démonter l'arme", .chance = 0.55f,
              .success = { .text = "Vous récupérez une arme fonctionnelle !", .weapon = RANDOM_WEAPON },
              .failure = { .text = "Le satellite réveille un drone de défense automatisé !", .combat = true, .enemy = EK::DRONE } },
            { .label = "Le désosser pour les pièces",
              .success = { .text = "Vous repartez avec une bonne quantité de ferraille.", .scrap = 16 } },
          } },
        { "Champ d'astéroïdes",
          "Un champ d'astéroïdes riche en minerais bloque une partie de la balise.",
          {
            { .label = "Manœuvrer entre les rochers", .reqSys = SYS_ENGINES, .reqLevel = 3,
              .success = { .text = "Vos moteurs vous permettent de récolter des minerais rares sans une égratignure.", .scrap = 28 } },
            { .label = "Miner les astéroïdes", .chance = 0.6f,
              .success = { .text = "La récolte est bonne.", .scrap = 22 },
              .failure = { .text = "Un astéroïde percute la coque.", .scrap = 6, .hull = -5 } },
            { .label = "Contourner le champ",
              .success = { .text = "Vous passez votre chemin." } },
          } },
        { "Cache de contrebandiers",
          "Vos capteurs repèrent un coffre blindé dissimulé dans une coque de cargo.",
          {
            { .label = "Pirater la serrure", .reqSys = SYS_HACKING, .reqLevel = 1,
              .success = { .text = "Votre module de piratage ouvre le coffre en silence.", .scrap = 38, .fuel = 2 } },
            { .label = "Forcer le coffre", .chance = 0.5f,
              .success = { .text = "Le coffre contient de la ferraille et du carburant de contrebande.", .scrap = 38, .fuel = 2 },
              .failure = { .text = "Les propriétaires reviennent chercher leur butin. Ils ne sont pas contents.", .combat = true } },
            { .label = "Ne pas s'attirer d'ennuis",
              .success = { .text = "Vous laissez le coffre où il est." } },
          } },
        { "Cargo à la dérive",
          "Un cargo sans équipage dérive lentement. Ses soutes sont scellées, et quelque chose bouge dans ses systèmes.",
          {
            { .label = "Analyser avec les capteurs", .reqSys = SYS_SENSORS, .reqLevel = 1,
              .success = { .text = "Vos capteurs repèrent le drone de sécurité et la soute la plus riche. Pillage sans risque.", .scrap = 32, .fuel = 1 } },
            { .label = "Aborder le cargo", .chance = 0.65f,
              .success = { .text = "Les soutes regorgent de pièces détachées.", .scrap = 26, .fuel = 2 },
              .failure = { .text = "Un drone de sécurité s'active et vous prend en chasse !", .combat = true, .enemy = EK::DRONE } },
            { .label = "L'ignorer", .success = {} },
          } },
        { "Épave de croiseur",
          "La carcasse d'un croiseur de bataille. Une tourelle intacte pend encore à son flanc, mais la structure menace de céder.",
          {
            { .label = "Récupérer la tourelle", .chance = 0.45f,
              .success = { .text = "Vous arrachez l'arme avant l'effondrement de la coque.", .weapon = RANDOM_WEAPON },
              .failure = { .text = "La structure s'effondre sur votre vaisseau.", .scrap = 6, .hull = -5 } },
            { .label = "Récupérer du blindage", .success = { .text = "Quelques plaques en bon état.", .scrap = 14 } },
          } },
        { "Laboratoire abandonné",
          "Un laboratoire d'ingénierie flotte en orbite, ses sas grands ouverts. Des prototypes sont encore sur les établis.",
          {
            { .label = "Récupérer un prototype", .chance = 0.6f,
              .success = { .text = "Vous installez un prototype encore fonctionnel.", .augment = RANDOM_AUGMENT },
              .failure = { .text = "Le système de sécurité du labo déclenche une décharge sur votre coque.", .hull = -4 } },
            { .label = "Pirater les archives", .reqSys = SYS_HACKING, .reqLevel = 1,
              .success = { .text = "Les plans récupérés vous permettent d'assembler un prototype sans risque.", .augment = RANDOM_AUGMENT } },
            { .label = "Repartir", .success = {} },
          } },
        { "Station de ravitaillement abandonnée",
          "Une station automatique est toujours alimentée. Ses réservoirs ne sont pas complètement vides.",
          {
            { .label = "Siphonner les réservoirs",
              .success = { .text = "Vous remplissez une partie de vos réserves.", .fuel = 3 } },
            { .label = "Pirater le terminal", .chance = 0.5f,
              .success = { .text = "Le terminal libère aussi un stock de pièces de réparation.", .fuel = 2, .hull = 5 },
              .failure = { .text = "Le système de sécurité purge les réservoirs. Il ne reste presque rien.", .fuel = 1 } },
          } },
    };
    return list;
}

static const std::vector<EventDef>& CombatEvents() {
    static const std::vector<EventDef> list = {
        { "Contact hostile",
          "Un vaisseau rebelle sort de l'ombre d'une lune et verrouille ses armes sur vous.",
          {
            { .label = "Aux postes de combat !",
              .success = { .text = "", .combat = true } },
          } },
        { "Pirates",
          "« Coupez vos moteurs et préparez-vous à être abordés. » Un pirate vous hèle, sûr de lui.",
          {
            { .label = "Combattre",
              .success = { .text = "", .combat = true } },
            { .label = "Payer 20 ferraille de tribut", .reqScrap = 20,
              .success = { .text = "Le pirate empoche votre ferraille et vous laisse partir en ricanant.", .scrap = -20 } },
          } },
        { "Embuscade",
          "Deux signatures surgissent de l'ombre d'un astéroïde. Le premier vaisseau verrouille déjà ses armes.",
          {
            { .label = "Riposter",
              .success = { .text = "", .combat = true } },
            { .label = "Disparaître des radars", .reqSys = SYS_CLOAK, .reqLevel = 1,
              .success = { .text = "Invisible, vous vous faufilez hors de portée. Au passage, vous récupérez une caisse abandonnée.", .scrap = 12 } },
          } },
        { "Mercenaire",
          "Un mercenaire bloque le relais. « Payez le passage, ou payez-le autrement. »",
          {
            { .label = "Payer 30 ferraille", .reqScrap = 30,
              .success = { .text = "Il empoche la ferraille et vous laisse passer.", .scrap = -30 } },
            { .label = "Refuser",
              .success = { .text = "", .combat = true } },
          } },
        { "Chasseur de primes",
          "Un chasseur de primes a reconnu votre signature. Votre tête vaut cher.",
          {
            { .label = "Engager le combat",
              .success = { .text = "", .combat = true } },
            { .label = "Tenter de le semer", .reqSys = SYS_ENGINES, .reqLevel = 4,
              .success = { .text = "Vos moteurs surpuissants le laissent loin derrière." } },
          } },
    };
    return list;
}

static const std::vector<EventDef>& EmptyEvents() {
    static const std::vector<EventDef> list = {
        { "Balise silencieuse",
          "Le silence radio est total. Trop total : vos capteurs détectent une signature thermique masquée.",
          {
            { .label = "Scanner la zone", .chance = 0.5f,
              .success = { .text = "Fausse alerte : une vieille sonde en fin de vie. Vous la démontez.", .scrap = 8 },
              .failure = { .text = "Un chasseur coupe son camouflage et ouvre le feu !", .combat = true } },
            { .label = "Repartir en silence", .success = {} },
          } },
        { "Signal fantôme",
          "Un signal intermittent, sur une fréquence militaire désaffectée. Il pourrait mener à une cache... ou nulle part.",
          {
            { .label = "Suivre le signal", .chance = 0.35f,
              .success = { .text = "Une capsule de ravitaillement militaire contient un prototype encore emballé.", .augment = RANDOM_AUGMENT },
              .failure = { .text = "Le signal s'éteint. Vous avez brûlé du carburant pour rien.", .fuel = -1 } },
            { .label = "Ignorer", .success = {} },
          } },
        { "Débris flottants",
          "Les restes d'une bataille récente dérivent autour de la balise. Le vainqueur n'est peut-être pas loin.",
          {
            { .label = "Fouiller les débris", .chance = 0.6f,
              .success = { .text = "Vous récupérez des pièces encore chaudes.", .scrap = 14 },
              .failure = { .text = "Le vainqueur revient finir le travail !", .scrap = 5, .combat = true } },
            { .label = "Ne pas traîner", .success = {} },
          } },
    };
    return list;
}

static const std::vector<EventDef>& PatrolEvents() {
    static const std::vector<EventDef> list = {
        { "Patrouille rebelle",
          "Votre passage a été signalé. Une patrouille rebelle attendait votre retour sur cette balise.",
          {
            { .label = "Combattre", .success = { .text = "", .combat = true } },
            { .label = "S'éclipser sans être vu", .reqSys = SYS_CLOAK, .reqLevel = 1,
              .success = { .text = "Votre vaisseau disparaît des écrans. La patrouille fouille le vide." } },
          } },
        { "Charognards",
          "Des pirates fouillent les restes de votre dernier passage. Ils vous repèrent immédiatement.",
          { { .label = "Aux armes !", .success = { .text = "", .combat = true } } } },
    };
    return list;
}

static const std::vector<EventDef>& NebulaEvents() {
    static const std::vector<EventDef> list = {
        { "Nébuleuse ionique",
          "Les capteurs sont brouillés par les gaz ionisés. La flotte rebelle aura du mal à suivre votre trace... et vous ne verrez rien venir.",
          {
            { .label = "Traverser la nébuleuse", .chance = 0.55f,
              .success = { .text = "Vous traversez sans encombre." },
              .failure = { .text = "Un pirate embusqué dans les gaz surgit à bout portant !", .combat = true } },
          } },
        { "Épave dans la nébuleuse",
          "Une épave repose au cœur des gaz ionisés. Les décharges électriques rendent l'approche délicate.",
          {
            { .label = "Approcher avec précaution", .chance = 0.7f,
              .success = { .text = "Vous récupérez des composants protégés par la nébuleuse.", .scrap = 20 },
              .failure = { .text = "Une décharge ionique frappe votre coque.", .scrap = 5, .hull = -2 } },
            { .label = "Rester à distance", .success = {} },
          } },
        { "Nébuleuse — poches de gaz",
          "Des poches de gaz instables contiennent du deutérium récupérable.",
          {
            { .label = "Récolter le gaz", .chance = 0.7f,
              .success = { .text = "Vous raffinez un peu de carburant.", .fuel = 2 },
              .failure = { .text = "Une poche s'enflamme contre la coque.", .hull = -3 } },
            { .label = "Rester prudent", .success = {} },
          } },
    };
    return list;
}

static const EventDef STORE_EVENT = {
    "Avant-poste marchand",
    "Un avant-poste commercial neutre émet sur toutes les fréquences : « Pièces, armes, carburant. Paiement en ferraille uniquement. »",
    {
        { .label = "Commercer", .success = { .text = "", .store = true } },
        { .label = "Partir",    .success = {} },
    }
};

static const EventDef EXIT_EVENT = {
    "Balise de sortie",
    "Le relais vers le secteur suivant est à portée. Une fois le saut effectué, impossible de revenir en arrière.",
    {
        { .label = "Sauter vers le secteur suivant", .success = { .text = "", .nextSector = true } },
        { .label = "Rester dans ce secteur",        .success = {} },
    }
};

static const EventDef BOSS_EVENT = {
    "Le vaisseau amiral",
    "Le cuirassé amiral de la flotte rebelle vous barre la route. Ses batteries pivotent lentement vers vous. C'est maintenant ou jamais.",
    {
        { .label = "Engager le vaisseau amiral", .success = { .text = "", .combat = true, .enemy = EK::BOSS } },
    }
};

static const EventDef INTRO_EVENT = {
    "Mission : AUGATON",
    "Vous transportez des données capables de mettre fin à la guerre. La flotte rebelle est à vos trousses.\n\n"
    "Traversez les 4 secteurs, rejoignez chaque balise de sortie et détruisez le vaisseau amiral rebelle qui vous attend au bout du voyage.\n\n"
    "Astuce : en combat, [ESPACE] met le jeu en pause pour donner vos ordres.",
    { { .label = "En route !", .success = {} } }
};

static const EventDef FLEET_EVENT = {
    "La flotte vous a rattrapé !",
    "Les éclaireurs de la flotte rebelle émergent de l'hyperespace tout autour de vous. Pas le temps de discuter.",
    { { .label = "Combattre pour survivre", .success = { .text = "", .combat = true, .enemy = EK::FLEET } } }
};

static const EventDef REVISIT_EVENT = {
    "Balise déjà explorée",
    "Vous connaissez déjà cet endroit. Il n'y a plus rien d'intéressant ici.",
    { { .label = "Continuer", .success = {} } }
};

// ─── API ─────────────────────────────────────────────────────────────────────

const EventDef& PickEvent(BeaconKind kind) {
    switch (kind) {
    case BeaconKind::COMBAT:   return rng::Pick(CombatEvents());
    case BeaconKind::DISTRESS: return rng::Pick(DistressEvents());
    case BeaconKind::DERELICT: return rng::Pick(DerelictEvents());
    case BeaconKind::NEBULA:   return rng::Pick(NebulaEvents());
    case BeaconKind::STORE:    return STORE_EVENT;
    case BeaconKind::EXIT:     return EXIT_EVENT;
    case BeaconKind::BOSS:     return BOSS_EVENT;
    case BeaconKind::START:
    case BeaconKind::EMPTY:
    default:                   return rng::Pick(EmptyEvents());
    }
}

const EventDef& IntroEvent()  { return INTRO_EVENT; }
const EventDef& PatrolEvent() { return rng::Pick(PatrolEvents()); }
const EventDef& FleetEvent() { return FLEET_EVENT; }

const EventDef& RevisitEvent(BeaconKind kind) {
    switch (kind) {
    case BeaconKind::STORE: return STORE_EVENT;
    case BeaconKind::EXIT:  return EXIT_EVENT;
    case BeaconKind::BOSS:  return BOSS_EVENT;
    default:                return REVISIT_EVENT;
    }
}

bool ChoiceAvailable(const Choice& c, const Ship& s) {
    if (s.scrap    < c.reqScrap)    return false;
    if (s.fuel     < c.reqFuel)     return false;
    if (s.missiles < c.reqMissiles) return false;
    if (c.reqSys >= 0 && s.sys[c.reqSys].level < c.reqLevel) return false;
    return true;
}

std::string ChoiceRequirement(const Choice& c) {
    if (c.reqSys >= 0)
        return std::string("[") + SystemName(c.reqSys) + (c.reqLevel > 1 ? " " + std::to_string(c.reqLevel) + "+]" : "]");
    if (c.chance < 1.0f)
        return "[risqué]";
    return "";
}

std::vector<std::string> ApplyOutcome(const Outcome& o, Ship& s) {
    std::vector<std::string> lines;
    auto delta = [&](int v, const char* what) {
        if (v == 0) return;
        lines.push_back((v > 0 ? "+" : "") + std::to_string(v) + " " + what);
    };

    s.scrap    = std::clamp(s.scrap + o.scrap, 0, 99999);
    s.fuel     = std::clamp(s.fuel + o.fuel, 0, 999);
    s.missiles = std::clamp(s.missiles + o.missiles, 0, 999);
    delta(o.scrap, "ferraille");
    delta(o.fuel, "carburant");
    delta(o.missiles, "missiles");

    if (o.hull != 0) {
        int before = s.hull;
        s.hull = std::clamp(s.hull + o.hull, 1, s.maxHull);   // un événement ne tue jamais
        delta(s.hull - before, "coque");
    }

    if (o.reactor > 0) {
        s.reactor = std::min(s.reactor + o.reactor, 25);
        s.AutoPowerNew();
        delta(o.reactor, "énergie de réacteur");
    }

    if (o.upgradeSys >= 0 && s.UpgradeSystem(o.upgradeSys))
        lines.push_back(std::string("+1 niveau ") + SystemName(o.upgradeSys));

    if (o.augment != -1) {
        int id = o.augment;
        if (id == RANDOM_AUGMENT) {
            std::vector<int> pool;
            for (int a = 0; a < AUG_COUNT; a++) if (!s.HasAug(a)) pool.push_back(a);
            id = pool.empty() ? -1 : rng::Pick(pool);
        }
        if (id >= 0) {
            const AugDef& def = GetAugDef(id);
            if (s.AddAugment(id)) {
                lines.push_back(std::string("Augmentation : ") + def.name);
            } else {
                int value = def.cost / 2;
                s.scrap = std::min(s.scrap + value, 99999);
                lines.push_back(std::string(def.name) + " revendue (emplacements pleins) : +" + std::to_string(value) + " ferraille");
            }
        }
    }

    if (o.weapon != -1) {
        int id = (o.weapon == RANDOM_WEAPON) ? rng::Int(0, WPN_COUNT - 1) : o.weapon;
        const WeaponDef& def = GetWeaponDef(id);
        if (s.AddWeapon(id)) {
            s.AutoPowerNew();
            lines.push_back(std::string("Nouvelle arme : ") + def.name);
        } else {
            int value = def.cost / 2;
            s.scrap = std::min(s.scrap + value, 99999);
            lines.push_back(std::string(def.name) + " revendu (soutes pleines) : +" + std::to_string(value) + " ferraille");
        }
    }

    return lines;
}
