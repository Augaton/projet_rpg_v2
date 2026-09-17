#include "SaveGame.hpp"
#include "../core/Paths.hpp"
#include "../ship/Hulls.hpp"
#include "../ship/Weapons.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace save {

static constexpr const char* MAGIC   = "AUGATON_SAVE";
static constexpr int         VERSION = 3;   // v3 : vaisseau, difficulté, augmentations, dangers ; v2 : 8 modules ; v1 : 4
static constexpr int         MAX_RESOURCE = 99999;

// FNV-1a 64 bits : détecte une corruption ou une modification accidentelle
// (ce n'est pas une signature cryptographique).
static uint64_t Fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

// ─── Écriture ────────────────────────────────────────────────────────────────

std::string Serialize(const SaveData& d) {
    std::ostringstream o;
    o.imbue(std::locale::classic());

    o << MAGIC << ' ' << VERSION << '\n';
    o << "resume " << (int)d.resume << ' ' << (int)d.combatKind << '\n';
    o << "run " << d.ship.shipType << ' ' << d.difficulty << '\n';
    o << "stats " << d.stats.jumps << ' ' << d.stats.kills << ' ' << d.stats.scrap << ' ' << d.stats.events << '\n';

    const Ship& s = d.ship;
    o << "ship " << s.hull << ' ' << s.maxHull << ' ' << s.reactor << ' '
      << s.fuel << ' ' << s.missiles << ' ' << s.scrap << '\n';
    for (int i = 0; i < SYS_COUNT; i++)
        o << "sys " << s.sys[i].level << ' ' << s.sys[i].desired << '\n';
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++)
        o << "weapon " << s.weapons[i].id << ' ' << (s.weapons[i].wanted ? 1 : 0) << '\n';
    o << "aug";
    for (int a : s.augments) o << ' ' << a;
    o << '\n';

    const MapState& m = d.map;
    char fleet[32];
    std::snprintf(fleet, sizeof(fleet), "%.3f", m.fleet);
    o << "map " << m.sector << ' ' << m.current << ' ' << fleet << ' ' << m.beacons.size() << '\n';
    for (const Beacon& b : m.beacons) {
        o << "b " << (int)std::lround(b.pos.x) << ' ' << (int)std::lround(b.pos.y) << ' '
          << b.column << ' ' << (int)b.kind << ' ' << (b.visited ? 1 : 0) << ' ' << (int)b.hazard << ' ' << b.links.size();
        for (int l : b.links) o << ' ' << l;
        o << '\n';
    }

    std::string body = o.str();
    char sum[40];
    std::snprintf(sum, sizeof(sum), "checksum %016llx\n", (unsigned long long)Fnv1a(body));
    return body + sum;
}

// ─── Lecture stricte ─────────────────────────────────────────────────────────

namespace {

struct Reader {
    std::istringstream in;
    explicit Reader(const std::string& s) : in(s) { in.imbue(std::locale::classic()); }

    bool Word(const char* expected) {
        std::string w;
        return static_cast<bool>(in >> w) && w == expected;
    }
    bool Int(int& v, int lo, int hi) {
        long long x;
        if (!(in >> x) || x < lo || x > hi) return false;
        v = (int)x;
        return true;
    }
    bool Float(float& v, float lo, float hi) {
        double x;
        if (!(in >> x) || !std::isfinite(x) || x < lo || x > hi) return false;
        v = (float)x;
        return true;
    }
};

} // namespace

bool Deserialize(const std::string& text, SaveData& out, std::string& error) {
    // Vérifie la somme de contrôle (dernière ligne)
    size_t pos = text.rfind("checksum ");
    if (pos == std::string::npos) { error = "somme de contrôle absente"; return false; }
    std::string body = text.substr(0, pos);
    unsigned long long stored = 0;
    if (std::sscanf(text.c_str() + pos, "checksum %16llx", &stored) != 1 || stored != Fnv1a(body)) {
        error = "fichier corrompu";
        return false;
    }

    Reader r(body);
    SaveData d;
    d.ship = MakePlayerShip();
    int version = 0, tmp = 0, tmp2 = 0;

    auto fail = [&](const char* what) { error = std::string("champ invalide : ") + what; return false; };

    if (!r.Word(MAGIC) || !r.Int(version, 1, 1000)) return fail("en-tête");
    if (version < 1 || version > VERSION) { error = "version de sauvegarde incompatible"; return false; }
    const int sysCount = (version == 1) ? SYS_COUNT_V1 : SYS_COUNT;

    if (!r.Word("resume") || !r.Int(tmp, 0, 2) || !r.Int(tmp2, 0, 3)) return fail("reprise");
    d.resume     = (ResumePoint)tmp;
    d.combatKind = (EnemyKind)tmp2;

    int shipType = SHIP_AUGATON;
    if (version >= 3 && (!r.Word("run") || !r.Int(shipType, 0, PLAYER_SHIP_COUNT - 1) ||
                         !r.Int(d.difficulty, 0, DIFFICULTY_COUNT - 1))) return fail("partie");

    if (!r.Word("stats") || !r.Int(d.stats.jumps, 0, 100000) || !r.Int(d.stats.kills, 0, 100000) ||
        !r.Int(d.stats.scrap, 0, 10000000) || !r.Int(d.stats.events, 0, 100000)) return fail("stats");

    d.ship = MakePlayerShip(shipType);
    Ship& s = d.ship;
    if (!r.Word("ship") || !r.Int(s.hull, 1, 200) || !r.Int(s.maxHull, 1, 200) ||
        !r.Int(s.reactor, 1, 40) || !r.Int(s.fuel, 0, MAX_RESOURCE) ||
        !r.Int(s.missiles, 0, MAX_RESOURCE) || !r.Int(s.scrap, 0, MAX_RESOURCE)) return fail("vaisseau");
    if (s.hull > s.maxHull) return fail("coque");

    for (auto& x : s.sys) x = ShipSystem{};
    for (int i = 0; i < sysCount; i++) {
        ShipSystem& x = s.sys[i];
        if (!r.Word("sys") || !r.Int(x.level, 0, SystemMaxLevel(i)) || !r.Int(x.desired, 0, SystemUsesPower(i) ? x.level : 0))
            return fail("système");
    }
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        WeaponSlot& w = s.weapons[i];
        w = WeaponSlot{};
        if (!r.Word("weapon") || !r.Int(w.id, -1, WPN_COUNT - 1) || !r.Int(tmp, 0, 1)) return fail("arme");
        w.wanted = (w.id >= 0) && tmp == 1;
    }
    for (int& a : s.augments) a = -1;
    if (version >= 3) {
        if (!r.Word("aug")) return fail("augmentations");
        for (int& a : s.augments) {
            if (!r.Int(a, -1, AUG_COUNT - 1)) return fail("augmentation");
        }
        for (int i = 0; i < MAX_AUGMENTS; i++)          // pas de doublon
            for (int j = i + 1; j < MAX_AUGMENTS; j++)
                if (s.augments[i] >= 0 && s.augments[i] == s.augments[j]) return fail("augmentation en double");
    }

    MapState& m = d.map;
    int count = 0;
    if (!r.Word("map") || !r.Int(m.sector, 0, 1000) || !r.Int(m.current, 0, 1000) ||
        !r.Float(m.fleet, -5.0f, 50.0f) || !r.Int(count, 2, SectorMap::MAX_BEACONS)) return fail("carte");

    m.beacons.resize(count);
    for (Beacon& b : m.beacons) {
        int x, y, col, kind, visited, hazard = 0, links;
        if (!r.Word("b") || !r.Int(x, 0, 4096) || !r.Int(y, 0, 4096) || !r.Int(col, 0, 64) ||
            !r.Int(kind, 0, BEACON_KIND_COUNT - 1) || !r.Int(visited, 0, 1) ||
            (version >= 3 && !r.Int(hazard, 0, HAZARD_COUNT - 1)) ||
            !r.Int(links, 0, SectorMap::MAX_BEACONS)) return fail("balise");
        b.hazard  = (BeaconHazard)hazard;
        b.pos     = { (float)x, (float)y };
        b.column  = col;
        b.kind    = (BeaconKind)kind;
        b.visited = visited == 1;
        b.links.resize(links);
        for (int& l : b.links) if (!r.Int(l, 0, count - 1)) return fail("lien");
    }

    std::string trailing;
    if (r.in >> trailing) return fail("données en trop");

    // hullType, nom et type proviennent de MakePlayerShip(shipType)
    s.RecomputePower();
    s.shieldLayers = s.MaxShieldLayers();

    out = std::move(d);
    return true;
}

// ─── Fichier ─────────────────────────────────────────────────────────────────

bool Exists() {
    std::error_code ec;
    return std::filesystem::is_regular_file(paths::SaveFile(), ec);
}

bool Write(const SaveData& data) {
    return paths::WriteFileAtomic(paths::SaveFile(), Serialize(data));
}

bool Read(SaveData& data, std::string& error) {
    std::string text;
    if (!paths::ReadSmallFile(paths::SaveFile(), text, 64 * 1024)) {
        error = "sauvegarde introuvable ou trop volumineuse";
        return false;
    }
    return Deserialize(text, data, error);
}

void Delete() {
    std::error_code ec;
    std::filesystem::remove(paths::SaveFile(), ec);
}

} // namespace save
