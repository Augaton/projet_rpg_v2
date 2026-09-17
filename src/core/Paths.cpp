#include "Paths.hpp"
#include "raylib.h"
#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace paths {

static bool IsAssetRoot(const fs::path& dir) {
    std::error_code ec;
    return fs::is_directory(dir / "asset", ec) &&
           fs::is_regular_file(dir / "src" / "ressources" / "shader" / "nebula.fs", ec);
}

bool LocateAssetRoot() {
    std::error_code ec;
    std::vector<fs::path> candidates = { fs::current_path(ec) };

    fs::path app = GetApplicationDirectory();
    for (int i = 0; i < 3 && !app.empty(); i++) {
        candidates.push_back(app);
        app = app.parent_path();
    }

    for (const auto& dir : candidates) {
        if (!dir.empty() && IsAssetRoot(dir)) {
            fs::current_path(dir, ec);
            return !ec;
        }
    }
    return false;
}

const std::string& DataDir() {
    static std::string dir = [] {
        fs::path base;
#ifdef _WIN32
        if (const char* appdata = std::getenv("APPDATA"); appdata && *appdata) base = fs::path(appdata) / "Augaton";
#else
        if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg && fs::path(xdg).is_absolute())
            base = fs::path(xdg) / "augaton";
        else if (const char* home = std::getenv("HOME"); home && *home && fs::path(home).is_absolute())
            base = fs::path(home) / ".local" / "share" / "augaton";
#endif
        std::error_code ec;
        if (base.empty() || (fs::create_directories(base, ec), ec)) {
            base = fs::current_path(ec) / "userdata";
            fs::create_directories(base, ec);
        }
        return base.string();
    }();
    return dir;
}

std::string SettingsFile()  { return (fs::path(DataDir()) / "settings.cfg").string(); }
std::string SaveFile()      { return (fs::path(DataDir()) / "run.sav").string(); }

std::string ScreenshotDir() {
    fs::path p = fs::path(DataDir()) / "screenshots";
    std::error_code ec;
    fs::create_directories(p, ec);
    return p.string();
}

bool ReadSmallFile(const std::string& path, std::string& out, size_t maxBytes) {
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    auto size = fs::file_size(path, ec);
    if (ec || size > maxBytes) return false;

    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign((size_t)size, '\0');
    in.read(out.data(), (std::streamsize)size);
    return (size_t)in.gcount() == size;
}

bool WriteFileAtomic(const std::string& path, const std::string& data) {
    std::string tmp = path + ".tmp";
    std::error_code ec;

    // stdio + fsync : toutes les erreurs d'écriture (disque plein, quota, E/S) sont détectées
    // AVANT de remplacer l'ancien fichier, et le contenu est sur disque avant le renommage.
    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    ok = ok && std::fflush(f) == 0;
#ifdef _WIN32
    ok = ok && _commit(_fileno(f)) == 0;
#else
    ok = ok && fsync(fileno(f)) == 0;
#endif
    ok = (std::fclose(f) == 0) && ok;
    if (!ok) {
        fs::remove(tmp, ec);
        return false;
    }

    fs::rename(tmp, path, ec);
    if (ec) {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

} // namespace paths
