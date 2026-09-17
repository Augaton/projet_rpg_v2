#include "Settings.hpp"
#include "Paths.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>

namespace {

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r");
    size_t b = s.find_last_not_of(" \t\r");
    return (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
}

bool ParseInt(const std::string& v, int lo, int hi, int& out) {
    int x = 0;
    auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x);
    if (ec != std::errc() || p != v.data() + v.size()) return false;
    out = std::clamp(x, lo, hi);
    return true;
}

bool ParseFloat(const std::string& v, float lo, float hi, float& out) {
    float x = 0.0f;
    auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), x);
    if (ec != std::errc() || p != v.data() + v.size() || !std::isfinite(x)) return false;
    out = std::clamp(x, lo, hi);
    return true;
}

bool ParseBool(const std::string& v, bool& out) {
    if (v == "1" || v == "true")  { out = true;  return true; }
    if (v == "0" || v == "false") { out = false; return true; }
    return false;
}

} // namespace

void Settings::Parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    int lines = 0;

    while (std::getline(in, line) && ++lines < 200) {
        if (line.size() > 200) continue;
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));

        if      (key == "master_volume")  ParseFloat(val, 0.0f, 1.0f, masterVolume);
        else if (key == "music_volume")   ParseFloat(val, 0.0f, 1.0f, musicVolume);
        else if (key == "sfx_volume")     ParseFloat(val, 0.0f, 1.0f, sfxVolume);
        else if (key == "fullscreen")     ParseBool(val, fullscreen);
        else if (key == "vsync")          ParseBool(val, vsync);
        else if (key == "fps_cap")        ParseInt(val, 0, 360, fpsCap);
        else if (key == "crt")            ParseInt(val, 0, 2, crt);
        else if (key == "shake")          ParseFloat(val, 0.0f, 1.0f, shake);
        else if (key == "reduce_flashes") ParseBool(val, reduceFlashes);
        else if (key == "pause_on_focus") ParseBool(val, pauseOnFocus);
        else if (key == "text_speed")     ParseInt(val, 0, 2, textSpeed);
        else if (key == "tutorial")       ParseBool(val, tutorial);
        else if (key == "auto_pause")     ParseBool(val, autoPause);
    }
    if (fpsCap != 0) fpsCap = std::max(fpsCap, 30);
}

std::string Settings::Serialize() const {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "# Augaton - parametres\n"
        << "master_volume="  << masterVolume << "\n"
        << "music_volume="   << musicVolume  << "\n"
        << "sfx_volume="     << sfxVolume    << "\n"
        << "fullscreen="     << (fullscreen ? 1 : 0) << "\n"
        << "vsync="          << (vsync ? 1 : 0) << "\n"
        << "fps_cap="        << fpsCap << "\n"
        << "crt="            << crt << "\n"
        << "shake="          << shake << "\n"
        << "reduce_flashes=" << (reduceFlashes ? 1 : 0) << "\n"
        << "pause_on_focus=" << (pauseOnFocus ? 1 : 0) << "\n"
        << "text_speed="     << textSpeed << "\n"
        << "tutorial="       << (tutorial ? 1 : 0) << "\n"
        << "auto_pause="     << (autoPause ? 1 : 0) << "\n";
    return out.str();
}

void Settings::Load() {
    std::string text;
    if (paths::ReadSmallFile(paths::SettingsFile(), text, 16 * 1024)) Parse(text);
}

void Settings::Save() const {
    paths::WriteFileAtomic(paths::SettingsFile(), Serialize());
}
