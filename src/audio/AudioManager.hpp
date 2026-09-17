#ifndef AUDIO_MANAGER_HPP
#define AUDIO_MANAGER_HPP

#include "raylib.h"
#include <map>
#include <string>
#include <vector>

class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager() { Close(); }

    void Init();

    // Chargement — `voices` = nombre de lectures simultanées possibles
    void LoadSfx(const std::string& name, const std::string& path, int voices = 4);
    void LoadMusic(const std::string& name, const std::string& path);
    // Son généré en mémoire (PCM 16 bits mono)
    void LoadSfxSamples(const std::string& name, std::vector<short> samples, int sampleRate, int voices = 3);

    // Contrôle
    void PlaySfx(const std::string& name, float pitch = 1.0f, float volume = 1.0f);
    void PlayMusic(const std::string& name);
    void Update(); // À appeler dans la boucle main

    // Utilitaires
    void SetMusicVolume(const std::string& name, float volume);
    void SetMusicPitch(const std::string& name, float pitch);
    void SetVolumes(float master, float music, float sfx) { masterVolume = master; musicVolume = music; sfxVolume = sfx; }
    void Close();

private:
    struct Voices {
        Sound              source{};
        std::vector<Sound> aliases;
        size_t             next = 0;
    };

    std::map<std::string, Voices> sounds;
    void _StoreSfx(const std::string& name, Voices&& v, int voices);
    std::map<std::string, Music>  musicTracks;
    float masterVolume = 1.0f;
    float musicVolume  = 1.0f;
    float sfxVolume    = 1.0f;
    bool  initialized  = false;
};

#endif
