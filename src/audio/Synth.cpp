#include "Synth.hpp"
#include "AudioManager.hpp"
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {

constexpr int   RATE = 22050;
constexpr float TAU  = 6.2831853f;

using Buffer = std::vector<float>;

int Frames(float seconds) { return (int)(seconds * (float)RATE); }

// Enveloppe : attaque courte puis décroissance exponentielle
float Envelope(float t, float duration, float attack = 0.004f) {
    if (t < attack) return t / attack;
    float k = (t - attack) / std::max(0.001f, duration - attack);
    return std::exp(-4.0f * k) * (1.0f - k);
}

void Mix(Buffer& out, const Buffer& in, int offset = 0, float gain = 1.0f) {
    if ((int)out.size() < offset + (int)in.size()) out.resize(offset + in.size(), 0.0f);
    for (size_t i = 0; i < in.size(); i++) out[offset + i] += in[i] * gain;
}

// Onde carrée légèrement adoucie (son « rétro »)
Buffer Blip(float freq, float duration, float gain = 0.5f) {
    Buffer b(Frames(duration));
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / RATE;
        float s = std::sin(TAU * freq * t);
        float square = s >= 0.0f ? 1.0f : -1.0f;
        b[i] = (0.6f * square + 0.4f * s) * Envelope(t, duration) * gain;
    }
    return b;
}

// Glissando de fréquence
Buffer Sweep(float f0, float f1, float duration, float gain = 0.5f) {
    Buffer b(Frames(duration));
    float phase = 0.0f;
    for (size_t i = 0; i < b.size(); i++) {
        float t = (float)i / RATE;
        float f = f0 + (f1 - f0) * (t / duration);
        phase += TAU * f / RATE;
        b[i] = std::sin(phase) * Envelope(t, duration, 0.01f) * gain;
    }
    return b;
}

// Bruit filtré (passe-bas simple), générateur déterministe
Buffer Noise(float duration, float smoothing, float gain = 0.5f) {
    Buffer b(Frames(duration));
    unsigned state = 0x12345678u;
    float last = 0.0f;
    for (size_t i = 0; i < b.size(); i++) {
        state = state * 1664525u + 1013904223u;
        float white = (float)(state >> 8) / (float)(1u << 24) * 2.0f - 1.0f;
        last += (white - last) * smoothing;
        b[i] = last * Envelope((float)i / RATE, duration, 0.002f) * gain;
    }
    return b;
}

void Register(AudioManager& audio, const char* name, Buffer b, int voices) {
    // Normalisation douce pour éviter la saturation
    float peak = 0.0f;
    for (float s : b) peak = std::max(peak, std::fabs(s));
    float norm = peak > 0.9f ? 0.9f / peak : 1.0f;

    std::vector<short> pcm(b.size());
    for (size_t i = 0; i < b.size(); i++)
        pcm[i] = (short)std::clamp(b[i] * norm * 32767.0f, -32768.0f, 32767.0f);
    audio.LoadSfxSamples(name, std::move(pcm), RATE, voices);
}

} // namespace

void RegisterSynthSounds(AudioManager& audio) {
    Register(audio, "sx_click", Blip(1320.0f, 0.045f, 0.35f), 3);

    Register(audio, "sx_error", Blip(180.0f, 0.14f, 0.45f), 2);

    Buffer ready;
    Mix(ready, Blip(660.0f, 0.06f, 0.3f));
    Mix(ready, Blip(990.0f, 0.08f, 0.3f), Frames(0.05f));
    Register(audio, "sx_ready", ready, 4);

    Buffer alarm;
    for (int k = 0; k < 3; k++) {
        Mix(alarm, Blip(440.0f, 0.12f, 0.45f), Frames(0.26f * k));
        Mix(alarm, Blip(330.0f, 0.12f, 0.45f), Frames(0.26f * k + 0.13f));
    }
    Register(audio, "sx_alarm", alarm, 1);

    Buffer shield = Noise(0.16f, 0.35f, 0.6f);
    Mix(shield, Sweep(900.0f, 300.0f, 0.12f, 0.25f));
    Register(audio, "sx_shield", shield, 4);

    Register(audio, "sx_cloak", Sweep(1600.0f, 220.0f, 0.35f, 0.5f), 1);

    Buffer hack;
    const float notes[] = { 1200, 800, 1500, 600, 1800, 950 };
    for (int k = 0; k < 6; k++) Mix(hack, Blip(notes[k], 0.04f, 0.3f), Frames(0.045f * k));
    Register(audio, "sx_hack", hack, 1);

    Buffer unlock;
    const float arpeggio[] = { 523, 659, 784, 1047 };
    for (int k = 0; k < 4; k++) Mix(unlock, Blip(arpeggio[k], 0.14f, 0.35f), Frames(0.09f * k));
    Register(audio, "sx_unlock", unlock, 1);
}
