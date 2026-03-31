#ifndef AUDIO_MANAGER_HPP
#define AUDIO_MANAGER_HPP

#include "raylib.h"
#include <map>
#include <string>

class AudioManager {
private:
    std::map<std::string, Sound> sounds;
    std::map<std::string, Music> musicTracks;
    float masterVolume;

public:
    AudioManager() : masterVolume(1.0f) { InitAudioDevice(); }
    ~AudioManager() { Close(); }

    // Chargement
    void LoadSfx(std::string name, std::string path);
    void LoadMusic(std::string name, std::string path);

    // Contrôle
    void PlaySfx(std::string name, float pitch = 1.0f, float volume = 1.0f);
    void PlayMusic(std::string name);
    void Update(); // À appeler dans la boucle main
    
    // Utilitaires
    void SetMusicVolume(std::string name, float volume);
    void SetMusicPitch(std::string name, float pitch);
    void Close();
};

#endif