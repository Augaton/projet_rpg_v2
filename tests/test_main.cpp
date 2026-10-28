// Tests unitaires, sans fenêtre : sauvegarde, carte du secteur, répartition de l'énergie.
//   make test   (compilés avec les vérifications de la cible debug : UBSan, conteneurs vérifiés)
#include "core/Config.hpp"
#include "core/Random.hpp"
#include "game/SaveGame.hpp"
#include "map/SectorMap.hpp"
#include "ship/Augments.hpp"
#include "ship/ShipFactory.hpp"
#include "ship/Weapons.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

static int g_checks = 0, g_failed = 0;

#define CHECK(cond) do {                                                          \
    g_checks++;                                                                   \
    if (!(cond)) {                                                                \
        g_failed++;                                                               \
        std::fprintf(stderr, "%s:%d: échec : %s\n", __FILE__, __LINE__, #cond);   \
    }                                                                             \
} while (0)

// ─── Outils ──────────────────────────────────────────────────────────────────

// Recalcule la somme de contrôle d'une sauvegarde modifiée : on teste alors la validation
// des champs eux-mêmes, et non la détection de corruption
static std::string Reseal(const std::string& text) {
    std::string body = text.substr(0, text.rfind("checksum "));
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : body) { h ^= c; h *= 1099511628211ULL; }
    char sum[40];
    std::snprintf(sum, sizeof sum, "checksum %016llx\n", (unsigned long long)h);
    return body + sum;
}

static std::string ReplaceFirst(std::string s, const std::string& from, const std::string& to) {
    size_t pos = s.find(from);
    if (pos != std::string::npos) s.replace(pos, from.size(), to);
    return s;
}

static SaveData SampleSave() {
    rng::Engine().seed(42);
    SectorMap map;
    map.Generate(1, false);
    SaveData d;
    d.map         = map.Export();
    d.ship        = MakePlayerShip(SHIP_AUGATON);
    d.ship.hull   = d.ship.maxHull - 4;
    d.ship.scrap  = 123;
    d.ship.fuel   = 7;
    d.ship.augments[0] = 0;
    d.stats       = { 5, 2, 300, 4 };
    d.resume      = ResumePoint::ARRIVAL;
    d.difficulty  = 2;
    return d;
}

static bool Rejects(const std::string& text) {
    SaveData out;
    std::string err;
    return !save::Deserialize(text, out, err) && !err.empty();
}

// Invariants de l'énergie : rien n'est alloué au-delà du réacteur ni de la capacité des systèmes
static bool PowerConsistent(const Ship& s) {
    if (s.PowerInUse() > s.reactor) return false;
    int weapons = 0;
    for (const auto& w : s.weapons)
        if (!w.Empty() && w.powered) weapons += GetWeaponDef(w.id).power;
    if (weapons != s.sys[SYS_WEAPONS].power) return false;
    for (int i = 0; i < SYS_COUNT; i++)
        if (s.sys[i].power < 0 || s.sys[i].power > s.sys[i].Capacity()) return false;
    return true;
}

// ─── Sauvegarde ──────────────────────────────────────────────────────────────

static void TestSaveRoundTrip() {
    SaveData d = SampleSave();
    std::string text = save::Serialize(d);

    SaveData out;
    std::string err;
    CHECK(save::Deserialize(text, out, err));
    CHECK(save::Serialize(out) == text);           // relecture fidèle, champ pour champ
    CHECK(out.ship.hull == d.ship.hull && out.ship.scrap == 123 && out.ship.augments[0] == 0);
    CHECK(out.resume == ResumePoint::ARRIVAL && out.difficulty == 2);

    SectorMap map;
    CHECK(map.Import(out.map));
    CHECK(map.JumpsToExit(map.CurrentIdx()) > 0);
}

static void TestSaveCorruption() {
    std::string text = save::Serialize(SampleSave());

    std::string flipped = text;
    flipped[flipped.find("stats ") + 6] ^= 1;        // un chiffre modifié sans resceller
    CHECK(Rejects(flipped));
    CHECK(Rejects(text.substr(0, text.rfind("checksum "))));
    CHECK(Rejects(""));
    CHECK(Rejects("checksum zzzz\n"));
    CHECK(Rejects(Reseal(text.substr(0, text.size() / 2) + "checksum 0\n")));   // tronquée
}

static void TestSaveInvalidFields() {
    const SaveData base = SampleSave();
    std::string text = save::Serialize(base);

    CHECK(Rejects(Reseal(ReplaceFirst(text, "AUGATON_SAVE 3", "AUGATON_SAVE 99"))));
    CHECK(Rejects(Reseal(ReplaceFirst(text, "checksum", "extra 1\nchecksum"))));
    SaveData d = base;
    d.map.fleet = std::nanf("");
    CHECK(Rejects(save::Serialize(d)));

    d = base;
    d.ship.hull = d.ship.maxHull + 1;
    CHECK(Rejects(save::Serialize(d)));

    d = base;
    d.ship.weapons[0].id = WPN_COUNT;
    CHECK(Rejects(save::Serialize(d)));

    d = base;
    d.ship.augments[1] = d.ship.augments[0];
    CHECK(Rejects(save::Serialize(d)));

    d = base;
    d.ship.fuel = 100000;
    CHECK(Rejects(save::Serialize(d)));

    // Lien vers une balise inexistante
    d = base;
    d.map.beacons[0].links.push_back((int)d.map.beacons.size());
    CHECK(Rejects(save::Serialize(d)));
}

// Énergie demandée supérieure au réacteur (sauvegarde modifiée à la main) :
// la partie chargée ne doit jamais consommer plus que le réacteur
static void TestSaveReactorOvercommit() {
    SaveData d = SampleSave();
    d.ship.reactor = 2;
    SaveData out;
    std::string err;
    if (save::Deserialize(save::Serialize(d), out, err)) {
        CHECK(PowerConsistent(out.ship));
        CHECK(out.ship.FreePower() >= 0);
    }
}

// Mutations aléatoires d'une sauvegarde valide : jamais de plantage, et toute
// sauvegarde acceptée respecte les invariants du jeu
static void TestSaveFuzz() {
    const std::string text = save::Serialize(SampleSave());
    const std::string body = text.substr(0, text.rfind("checksum "));
    static const char ALPHABET[] = "0123456789 -.\nabxyz";
    rng::Engine().seed(7);

    for (int iter = 0; iter < 4000; iter++) {
        std::string m = body;
        int edits = rng::Int(1, 4);
        for (int e = 0; e < edits && !m.empty(); e++) {
            size_t pos = (size_t)rng::Int(0, (int)m.size() - 1);
            switch (rng::Int(0, 2)) {
            case 0:  m[pos] = ALPHABET[rng::Int(0, (int)sizeof ALPHABET - 2)]; break;
            case 1:  m.erase(pos, 1); break;
            default: m.insert(pos, 1, ALPHABET[rng::Int(0, 9)]); break;
            }
        }
        SaveData out;
        std::string err;
        if (!save::Deserialize(Reseal(m + "checksum 0\n"), out, err)) continue;

        const Ship& s = out.ship;
        CHECK(s.hull >= 1 && s.hull <= s.maxHull);
        CHECK(s.fuel >= 0 && s.scrap >= 0 && s.missiles >= 0);
        CHECK(PowerConsistent(s));
        for (const auto& w : s.weapons) CHECK(w.id >= -1 && w.id < WPN_COUNT);
        SectorMap map;
        if (map.Import(out.map)) CHECK(map.JumpsToExit(map.CurrentIdx()) >= 0);
    }
}

// ─── Carte du secteur ────────────────────────────────────────────────────────

static void TestMapGeneration() {
    for (int sector = 0; sector < SECTOR_COUNT; sector++) {
        bool final = sector == SECTOR_COUNT - 1;
        for (unsigned seed = 1; seed <= 100; seed++) {
            rng::Engine().seed(seed * 7919u + (unsigned)sector);
            SectorMap map;
            map.Generate(sector, final);
            MapState st = map.Export();

            int n = (int)st.beacons.size(), goals = 0;
            CHECK(n >= 2 && n <= SectorMap::MAX_BEACONS);
            CHECK(map.JumpsToExit(map.CurrentIdx()) > 0);
            for (int i = 0; i < n; i++) {
                const Beacon& b = st.beacons[i];
                if (b.kind == (final ? BeaconKind::BOSS : BeaconKind::EXIT)) goals++;
                for (int l : b.links) {
                    const auto& back = st.beacons[l].links;
                    CHECK(std::find(back.begin(), back.end(), i) != back.end());   // liens symétriques
                }
            }
            CHECK(goals >= 1);

            SectorMap copy;
            CHECK(copy.Import(st));
        }
    }
}

static void TestMapImportRejects() {
    rng::Engine().seed(3);
    SectorMap map;
    map.Generate(0, false);
    const MapState good = map.Export();
    SectorMap target;

    MapState st = good;
    st.current = (int)st.beacons.size();
    CHECK(!target.Import(st));

    st = good;
    st.sector = SECTOR_COUNT;
    CHECK(!target.Import(st));

    // Lien à sens unique
    st = good;
    int a = st.current, b = st.beacons[a].links.front();
    auto& back = st.beacons[b].links;
    back.erase(std::find(back.begin(), back.end(), a));
    CHECK(!target.Import(st));

    // Balise de départ isolée : la sortie devient inaccessible
    st = good;
    for (int l : st.beacons[a].links) {
        auto& v = st.beacons[l].links;
        v.erase(std::find(v.begin(), v.end(), a));
    }
    st.beacons[a].links.clear();
    CHECK(!target.Import(st));
    CHECK(target.Import(good));
}

// ─── Énergie ─────────────────────────────────────────────────────────────────

static void TestPowerStartingShips() {
    for (int id = 0; id < PLAYER_SHIP_COUNT; id++) {
        Ship s = MakePlayerShip(id);
        CHECK(PowerConsistent(s));
        CHECK(s.FreePower() >= 0);
    }
}

static void TestPowerLimits() {
    Ship s = MakePlayerShip(SHIP_AUGATON);
    while (s.RemovePower(SYS_SHIELDS)) {}
    CHECK(s.sys[SYS_SHIELDS].power == 0);
    CHECK(!s.RemovePower(SYS_SHIELDS));

    int added = 0;
    while (s.AddPower(SYS_SHIELDS)) added++;
    CHECK(added > 0);
    CHECK(s.sys[SYS_SHIELDS].power <= s.sys[SYS_SHIELDS].Capacity());
    CHECK(PowerConsistent(s));
}

// Séquences d'opérations aléatoires (énergie, armes, dégâts, ions) : invariants préservés
static void TestPowerRandomOps() {
    rng::Engine().seed(11);
    for (int run = 0; run < 200; run++) {
        Ship s = MakePlayerShip(run % PLAYER_SHIP_COUNT);
        for (int op = 0; op < 120; op++) {
            int sys = rng::Int(0, SYS_COUNT - 1), slot = rng::Int(0, MAX_WEAPON_SLOTS - 1);
            switch (rng::Int(0, 5)) {
            case 0: s.AddPower(sys); break;
            case 1: s.RemovePower(sys); break;
            case 2: s.PowerWeapon(slot); break;
            case 3: s.DepowerWeapon(slot); break;
            case 4:
                s.sys[sys].damage = rng::Int(0, s.sys[sys].level);
                s.RecomputePower();
                break;
            default:
                s.sys[sys].ionTimer = rng::Chance(0.5f) ? 3.0f : 0.0f;
                s.RecomputePower();
                break;
            }
            CHECK(PowerConsistent(s));
        }
    }
}

// ─── Armes ───────────────────────────────────────────────────────────────────

// Table des armes cohérente : une arme mal saisie fausserait le combat sans erreur visible
static void TestWeaponTable() {
    for (int id = 0; id < WPN_COUNT; id++) {
        const WeaponDef& d = GetWeaponDef(id);
        CHECK(d.charge > 0.0f && d.shots >= 1 && d.cost > 0);
        CHECK(d.power >= 1 && d.power <= 4);
        CHECK(d.damage > 0 || d.ion > 0);                // chaque arme fait quelque chose
        CHECK(!(d.missile && d.pierce > 0));             // un missile traverse déjà tout
        CHECK(d.pierce >= 0 && d.ion >= 0);
    }
    CHECK(&GetWeaponDef(-1) == &GetWeaponDef(0));        // identifiant invalide : repli sûr
    CHECK(&GetWeaponDef(WPN_COUNT) == &GetWeaponDef(0));
}

int main() {
    TestWeaponTable();
    TestSaveRoundTrip();
    TestSaveCorruption();
    TestSaveInvalidFields();
    TestSaveReactorOvercommit();
    TestSaveFuzz();
    TestMapGeneration();
    TestMapImportRejects();
    TestPowerStartingShips();
    TestPowerLimits();
    TestPowerRandomOps();

    std::printf("%d vérifications, %d échec(s)\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
