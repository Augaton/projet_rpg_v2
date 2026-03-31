#include "AudioManager.hpp"

void AudioManager::LoadSfx(std::string name, std::string path) {
    sounds[name] = LoadSound(path.c_str());
}

void AudioManager::LoadMusic(std::string name, std::string path) {
    musicTracks[name] = LoadMusicStream(path.c_str());
    musicTracks[name].looping = true;
}

void AudioManager::PlaySfx(std::string name, float pitch, float volume) {
    if (sounds.count(name)) {
        SetSoundPitch(sounds[name], pitch);
        SetSoundVolume(sounds[name], volume * masterVolume);
        PlaySound(sounds[name]);
    }
}

void AudioManager::PlayMusic(std::string name) {
    if (musicTracks.count(name)) PlayMusicStream(musicTracks[name]);
}

void AudioManager::Update() {
    for (auto const& [name, music] : musicTracks) {
        UpdateMusicStream(music);
    }
}

void AudioManager::SetMusicVolume(std::string name, float volume) {
    if (musicTracks.count(name)) ::SetMusicVolume(musicTracks[name], volume * masterVolume);
}

void AudioManager::SetMusicPitch(std::string name, float pitch) {
    if (musicTracks.count(name)) ::SetMusicPitch(musicTracks[name], pitch);
}

void AudioManager::Close() {
    for (auto& [name, sound] : sounds) UnloadSound(sound);
    for (auto& [name, music] : musicTracks) UnloadMusicStream(music);
    CloseAudioDevice();
}