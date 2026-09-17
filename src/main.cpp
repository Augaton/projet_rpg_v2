/*
   █████████                                  █████
  ███░░░░░███                                ░░███
 ░███    ░███  █████ ████  ███████  ██████   ███████    ██████  ████████
 ░███████████ ░░███ ░███  ███░░███ ░░░░░███ ░░░███░    ███░░███░░███░░███
 ░███░░░░░███  ░███ ░███ ░███ ░███  ███████   ░███    ░███ ░███ ░███ ░███
 ░███    ░███  ░███ ░███ ░███ ░███ ███░░███   ░███ ███░███ ░███ ░███ ░███
 █████   █████ ░░████████░░███████░░████████  ░░█████ ░░██████  ████ █████
░░░░░   ░░░░░   ░░░░░░░░  ░░░░░███ ░░░░░░░░    ░░░░░   ░░░░░░  ░░░░ ░░░░░
                          ███ ░███
                         ░░██████
                          ░░░░░░
*/

#include "core/Paths.hpp"
#include "game/Game.hpp"
#include "raylib.h"

int main() {
    // Permet de lancer le binaire depuis n'importe quel dossier
    if (!paths::LocateAssetRoot())
        TraceLog(LOG_WARNING, "Dossier asset/ introuvable : lancez le jeu depuis la racine du projet");

    // Instance statique : le jeu est volumineux (textures, vaisseaux, sons)
    static Game game;
    game.Init();
    game.Run();
    game.Shutdown();
    return 0;
}
