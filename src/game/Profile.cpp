#include "Profile.hpp"
#include "../core/Paths.hpp"
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <sstream>

namespace {

std::string ProfileFile() {
    return (std::filesystem::path(paths::DataDir()) / "profile.cfg").string();
}

bool ToInt(const std::string& v, int lo, int hi, int& out) {
    int x = 0;
    auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x);
    if (ec != std::errc() || p != v.data() + v.size()) return false;
    out = std::clamp(x, lo, hi);
    return true;
}

} // namespace

// Format : clé=valeur, une partie par ligne « run=ship,difficulté,secteur,victoire,kills,ferraille »
void Profile::Parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    int lines = 0;
    history.clear();

    while (std::getline(in, line) && ++lines < 100) {
        if (line.size() > 200 || line.empty() || line[0] == '#') continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);

        if      (key == "runs")        ToInt(val, 0, 1000000, runs);
        else if (key == "wins")        ToInt(val, 0, 1000000, wins);
        else if (key == "best_sector") ToInt(val, 0, 99, bestSector);
        else if (key == "total_kills") ToInt(val, 0, 100000000, totalKills);
        else if (key == "total_scrap") ToInt(val, 0, 1000000000, totalScrap);
        else if (key.rfind("unlock_", 0) == 0) {
            // Identifiant valide et valeur strictement 0 ou 1 (pas de bornage silencieux)
            int id = -1;
            std::string idText = key.substr(7);
            auto [p, ec] = std::from_chars(idText.data(), idText.data() + idText.size(), id);
            bool idOk = ec == std::errc() && p == idText.data() + idText.size() && id >= 0 && id < PLAYER_SHIP_COUNT;
            if (idOk && (val == "0" || val == "1"))
                unlocked[id] = unlocked[id] || val == "1";
        } else if (key == "run" && history.size() < 8) {
            std::vector<int> f;
            std::stringstream ss(val);
            std::string part;
            while (std::getline(ss, part, ',') && f.size() < 6) {
                int x = 0;
                if (!ToInt(part, -1000000, 1000000000, x)) { f.clear(); break; }
                f.push_back(x);
            }
            if (f.size() != 6) continue;
            RunRecord r;
            r.ship       = std::clamp(f[0], 0, PLAYER_SHIP_COUNT - 1);
            r.difficulty = std::clamp(f[1], 0, DIFFICULTY_COUNT - 1);
            r.sector     = std::clamp(f[2], 1, 99);
            r.won        = f[3] != 0;
            r.kills      = std::max(0, f[4]);
            r.scrap      = std::max(0, f[5]);
            history.push_back(r);
        }
    }
    unlocked[SHIP_AUGATON] = true;
}

std::string Profile::Serialize() const {
    std::ostringstream o;
    o << "# Augaton - progression\n"
      << "runs=" << runs << "\n"
      << "wins=" << wins << "\n"
      << "best_sector=" << bestSector << "\n"
      << "total_kills=" << totalKills << "\n"
      << "total_scrap=" << totalScrap << "\n";
    for (int i = 0; i < PLAYER_SHIP_COUNT; i++) o << "unlock_" << i << "=" << (unlocked[i] ? 1 : 0) << "\n";
    for (const auto& r : history)
        o << "run=" << r.ship << ',' << r.difficulty << ',' << r.sector << ',' << (r.won ? 1 : 0)
          << ',' << r.kills << ',' << r.scrap << "\n";
    return o.str();
}

void Profile::Load() {
    std::string text;
    if (paths::ReadSmallFile(ProfileFile(), text, 32 * 1024)) Parse(text);
}

void Profile::Save() const {
    paths::WriteFileAtomic(ProfileFile(), Serialize());
}

void Profile::_CheckUnlocks(std::vector<int>& fresh) {
    auto unlock = [&](int id, bool condition) {
        if (condition && !unlocked[id]) { unlocked[id] = true; fresh.push_back(id); }
    };
    unlock(SHIP_VIPERE,  bestSector >= 3 || wins > 0);
    unlock(SHIP_BASTION, totalKills >= 20 || wins > 0);
}

std::vector<int> Profile::RecordRun(const RunRecord& run) {
    runs++;
    if (run.won) wins++;
    bestSector  = std::max(bestSector, run.sector);
    totalKills += run.kills;
    totalScrap += run.scrap;

    history.insert(history.begin(), run);
    if (history.size() > 8) history.resize(8);

    std::vector<int> fresh;
    _CheckUnlocks(fresh);
    return fresh;
}
