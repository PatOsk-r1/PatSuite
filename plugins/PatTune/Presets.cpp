// Factory presets of PatTune. Values in real units; parameters left out keep their default.
// Key: 0 = C ... 11 = B. Scale: index in dsp/tune/Scales.cpp.
#include "plugins/PatTune/Processor.h"

using namespace tune;

namespace {
enum Scale {
    Chromatic,
    Major,
    Minor,
    HarmonicMinor,
    MelodicMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    MajorPenta,
    MinorPenta,
    Blues,
    Rast,
    Bayati,
    Hijaz,
    Saba,
    Nahawand,
    Kurd,
    Sikah,
    Ajam,
    InSen,
    Hirajoshi,
    Iwato,
    Yo,
    Kumoi,
    Ryukyu,
};
}  // namespace

std::vector<pat::ui::FactoryPreset> TuneProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // key, scale, retune speed (ms), humanize, formant (semitones), mix, confidence gate
    struct P {
        const char* name;
        const char* category;
        int key, scale;
        float speed, humanize, formant, mix, gate;
    };
    const P list[] = {
        {"Init", "", 0, Major, 40, 0.3f, 0, 1, 0.6f},
        {"Hard Tune Robot", "Effect/Hard", 0, Chromatic, 0, 0, 0, 1, 0.55f},
        {"Trap Hard Tune", "Effect/Hard", 0, Minor, 0, 0.05f, 0, 1, 0.55f},
        {"Melodic Rap", "Effect/Hard", 9, Minor, 8, 0.1f, 0, 1, 0.6f},
        {"Pop Polish", "Natural", 0, Major, 35, 0.4f, 0, 1, 0.6f},
        {"Ballad Gentle", "Natural", 0, Major, 90, 0.6f, 0, 1, 0.65f},
        {"Live Safety Net", "Natural", 0, Chromatic, 140, 0.7f, 0, 1, 0.7f},
        {"Backing Vocals Tight", "Natural", 0, Major, 20, 0.2f, 0, 1, 0.6f},
        {"R&B Smooth", "Natural", 5, Dorian, 45, 0.45f, 0, 1, 0.6f},
        {"Reggaeton Minor", "Genre", 2, Minor, 12, 0.15f, 0, 1, 0.6f},
        {"Afro Pentatonic", "Genre", 7, MajorPenta, 25, 0.3f, 0, 1, 0.6f},
        {"Blues Bend", "Genre", 4, Blues, 60, 0.5f, 0, 1, 0.6f},
        {"Flamenco Phrygian", "Genre", 4, Phrygian, 30, 0.35f, 0, 1, 0.6f},
        {"Arabic Rast", "World/Maqam", 0, Rast, 30, 0.35f, 0, 1, 0.6f},
        {"Arabic Bayati", "World/Maqam", 2, Bayati, 30, 0.35f, 0, 1, 0.6f},
        {"Arabic Hijaz", "World/Maqam", 2, Hijaz, 20, 0.25f, 0, 1, 0.6f},
        {"Arabic Saba", "World/Maqam", 2, Saba, 30, 0.35f, 0, 1, 0.6f},
        {"Japanese Insen", "World/Japanese", 9, InSen, 25, 0.3f, 0, 1, 0.6f},
        {"Japanese Hirajoshi", "World/Japanese", 9, Hirajoshi, 25, 0.3f, 0, 1, 0.6f},
        {"Okinawa Ryukyu", "World/Japanese", 0, Ryukyu, 35, 0.4f, 0, 1, 0.6f},
        {"Chipmunk", "Character", 0, Chromatic, 20, 0, 6, 1, 0.6f},
        {"Deep Monster", "Character", 0, Chromatic, 20, 0, -7, 1, 0.6f},
        {"Parallel Blend", "Character", 0, Major, 0, 0, 0, 0.5f, 0.6f},
        // Producers: Uruguayan names that describe the effect.
        {"Robot de ANTEL", "Producers/Torbey Beats", 0, Chromatic, 0, 0, 0, 1, 0.5f},
        {"Cumbia de la Feria", "Producers/Torbey Beats", 7, Minor, 15, 0.2f, 0, 1, 0.6f},
        {"Afinado Tipo Conaprole", "Producers/Torbey Beats", 0, Major, 30, 0.35f, 0, 1, 0.6f},
        {"Grillo de Rocha", "Producers/Torbey Beats", 0, Chromatic, 5, 0, 5, 1, 0.6f},
        {"Ta Re Afinado", "Producers/Torbey Beats", 9, Minor, 0, 0.05f, 0, 1, 0.55f},
        {"Payador de Tacuarembó", "Producers/Pat Osk", 4, HarmonicMinor, 70, 0.55f, 0, 1, 0.65f},
        {"Gaucho Robot", "Producers/Pat Osk", 4, HarmonicMinor, 0, 0, -2, 1, 0.55f},
        {"Hielo de Pilsen", "Producers/Pat Osk", 1, Minor, 3, 0.1f, 1, 1, 0.6f},
        {"Ñandú Pitufo", "Producers/Pat Osk", 0, MajorPenta, 10, 0, 7, 1, 0.6f},
        {"Murga Afinada", "Producers/Pat Osk", 2, Major, 25, 0.4f, 0, 1, 0.6f},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list)
        out.push_back({u(p.name), u(p.category),
                       {{kAuto, 1.0f},
                        {kKey, static_cast<float>(p.key)},
                        {kScale, static_cast<float>(p.scale)},
                        {kSpeed, p.speed},
                        {kHumanize, p.humanize},
                        {kFormant, p.formant},
                        {kMix, p.mix},
                        {kGate, p.gate}}});
    return out;
}
