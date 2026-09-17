#include "AudioManager.hpp"
#include <algorithm>

// Remplace une entrée existante en libérant d'abord ses ressources (pas de fuite au rechargement)
void AudioManager::_StoreSfx(const std::string& name, Voices&& v, int voices) {
    voices = std::clamp(voices, 1, 16);
    for (int i = 1; i < voices; i++) v.aliases.push_back(LoadSoundAlias(v.source));
    auto it = sounds.find(name);
    if (it != sounds.end()) {
        for (Sound& a : it->second.aliases) UnloadSoundAlias(a);
        UnloadSound(it->second.source);
    }
    sounds[name] = std::move(v);
}

void AudioManager::Init() {
    if (initialized) return;
    InitAudioDevice();
    initialized = IsAudioDeviceReady();
}

void AudioManager::LoadSfx(const std::string& name, const std::string& path, int voices) {
    if (!initialized) return;
    Voices v;
    v.source = LoadSound(path.c_str());
    if (!IsSoundValid(v.source)) return;
    _StoreSfx(name, std::move(v), voices);
}

void AudioManager::LoadSfxSamples(const std::string& name, std::vector<short> samples,
                                  int sampleRate, int voices) {
    if (!initialized || samples.empty() || sampleRate <= 0) return;
    Wave wave{};
    wave.frameCount = (unsigned int)samples.size();
    wave.sampleRate = (unsigned int)sampleRate;
    wave.sampleSize = 16;
    wave.channels   = 1;
    wave.data       = samples.data();   // copié par LoadSoundFromWave

    Voices v;
    v.source = LoadSoundFromWave(wave);
    if (!IsSoundValid(v.source)) return;
    _StoreSfx(name, std::move(v), voices);
}

void AudioManager::LoadMusic(const std::string& name, const std::string& path) {
    if (!initialized) return;
    Music m = LoadMusicStream(path.c_str());
    if (!IsMusicValid(m)) return;
    m.looping = true;
    musicTracks[name] = m;
}

void AudioManager::PlaySfx(const std::string& name, float pitch, float volume) {
    auto it = sounds.find(name);
    if (it == sounds.end()) return;
    Voices& v = it->second;

    // Première voix libre, sinon on recycle à tour de rôle
    Sound* chosen = nullptr;
    if (!IsSoundPlaying(v.source)) chosen = &v.source;
    for (size_t i = 0; !chosen && i < v.aliases.size(); i++)
        if (!IsSoundPlaying(v.aliases[i])) chosen = &v.aliases[i];
    if (!chosen) {
        chosen = v.aliases.empty() ? &v.source : &v.aliases[v.next % v.aliases.size()];
        v.next++;
    }

    SetSoundPitch(*chosen, pitch);
    SetSoundVolume(*chosen, volume * masterVolume * sfxVolume);
    PlaySound(*chosen);
}

void AudioManager::PlayMusic(const std::string& name) {
    if (musicTracks.count(name)) PlayMusicStream(musicTracks[name]);
}

void AudioManager::Update() {
    for (auto const& [name, music] : musicTracks) UpdateMusicStream(music);
}

void AudioManager::SetMusicVolume(const std::string& name, float volume) {
    if (musicTracks.count(name)) ::SetMusicVolume(musicTracks[name], volume * masterVolume * musicVolume);
}

void AudioManager::SetMusicPitch(const std::string& name, float pitch) {
    if (musicTracks.count(name)) ::SetMusicPitch(musicTracks[name], pitch);
}

void AudioManager::Close() {
    if (!initialized) return;
    for (auto& [name, v] : sounds) {
        for (Sound& a : v.aliases) UnloadSoundAlias(a);
        UnloadSound(v.source);
    }
    for (auto& [name, music] : musicTracks) UnloadMusicStream(music);
    sounds.clear();
    musicTracks.clear();
    CloseAudioDevice();
    initialized = false;
}
