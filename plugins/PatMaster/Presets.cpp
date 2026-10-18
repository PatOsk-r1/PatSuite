// Factory presets of PatMaster. Values in real units; parameters left out keep their default.
// The platform target (Spotify, YouTube...) is chosen in the meter, not by the preset.
#include "plugins/PatMaster/Processor.h"

using namespace master;

std::vector<pat::ui::FactoryPreset> MasterProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // gain dB, ceiling dB, release ms, auto release, link, true peak, soft clip
    struct P {
        const char* name;
        const char* category;
        float gain, ceiling, release, autoRel, link, truePeak, clip;
    };
    const P list[] = {
        {"Init", "", 0, -1.0f, 100, 1, 1, 1, 0},
        {"Transparent Safety", "Clean", 0, -1.0f, 100, 1, 1, 1, 0},
        {"Streaming Ready (-1 dBTP)", "Clean", 4, -1.0f, 100, 1, 1, 1, 0},
        {"Gentle Lift", "Clean", 2, -1.0f, 200, 0, 1, 1, 0},
        {"Wide & Open", "Clean", 3, -1.0f, 150, 1, 0.5f, 1, 0},
        {"Loud Pop", "Loud", 7, -0.8f, 80, 1, 1, 1, 0.2f},
        {"Loud Trap Master", "Loud", 9, -0.3f, 50, 1, 1, 1, 0.35f},
        {"Club Banger", "Loud", 10, -0.3f, 40, 1, 1, 1, 0.5f},
        {"EDM Brickwall", "Loud", 12, -0.3f, 30, 1, 1, 1, 0.6f},
        {"Drill Smash", "Loud", 11, -0.5f, 40, 1, 1, 1, 0.45f},
        {"Rap Vocal Forward", "Genre", 6, -1.0f, 70, 1, 1, 1, 0.15f},
        {"Reggaeton Punch", "Genre", 8, -0.5f, 60, 1, 1, 1, 0.3f},
        {"Lo-Fi Soft", "Genre", 3, -1.5f, 300, 0, 1, 1, 0.1f},
        {"Rock Glue", "Genre", 6, -1.0f, 120, 1, 1, 1, 0.25f},
        {"Acoustic Natural", "Genre", 2, -1.0f, 250, 1, 1, 1, 0},
        {"Podcast Voice", "Voice", 5, -1.0f, 150, 1, 1, 1, 0},
        {"Fast Release Punch", "Character", 6, -1.0f, 15, 0, 1, 1, 0},
        {"Slow Release Smooth", "Character", 6, -1.0f, 600, 0, 1, 1, 0},
        {"Grit & Glue", "Character", 6, -1.0f, 80, 1, 1, 1, 0.8f},
        {"Stem Limiter (Unlinked)", "Character", 4, -1.0f, 80, 1, 0, 1, 0},
        // Producers: Uruguayan names.
        {"Pa' Que Suene en el Boliche", "Producers/Torbey Beats", 10, -0.3f, 45, 1, 1, 1, 0.45f},
        {"Fuerte Como Tannat", "Producers/Torbey Beats", 9, -0.5f, 60, 1, 1, 1, 0.35f},
        {"Radio de la Feria", "Producers/Torbey Beats", 7, -1.0f, 80, 1, 1, 1, 0.3f},
        {"Listo Pa' Spotify, Bo", "Producers/Torbey Beats", 5, -1.0f, 100, 1, 1, 1, 0.1f},
        {"Llamadas en el Barrio Sur", "Producers/Pat Osk", 8, -0.5f, 50, 1, 1, 1, 0.4f},
        {"Ta Más Fuerte Que el Pampero", "Producers/Pat Osk", 12, -0.3f, 35, 1, 1, 1, 0.6f},
        {"Liso Como la Rambla", "Producers/Pat Osk", 3, -1.0f, 250, 1, 1, 1, 0},
        {"Master del Centenario", "Producers/Pat Osk", 8, -0.5f, 70, 1, 1, 1, 0.3f},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list)
        out.push_back({u(p.name), u(p.category),
                       {{kGain, p.gain}, {kCeiling, p.ceiling}, {kRelease, p.release}, {kAutoRelease, p.autoRel}, {kLink, p.link},
                        {kTruePeak, p.truePeak}, {kClip, p.clip}}});
    return out;
}
