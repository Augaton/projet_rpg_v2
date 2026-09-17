#pragma once
#include <cstddef>
#include <string>

namespace paths {

// Se place dans le dossier qui contient asset/ (permet de lancer le binaire
// depuis n'importe où). Renvoie false si introuvable.
bool LocateAssetRoot();

// Dossier de données utilisateur (paramètres, sauvegarde, captures), créé si besoin
const std::string& DataDir();
std::string SettingsFile();
std::string SaveFile();
std::string ScreenshotDir();

// Lecture bornée : échoue si le fichier dépasse maxBytes
bool ReadSmallFile(const std::string& path, std::string& out, size_t maxBytes = 256 * 1024);
// Écriture atomique : fichier temporaire puis renommage
bool WriteFileAtomic(const std::string& path, const std::string& data);

} // namespace paths
