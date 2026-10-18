// Factory presets of PatDeEsser. Values in real units; parameters left out keep their default.
#include "plugins/PatDeEsser/Processor.h"

using namespace deesser;

std::vector<pat::ui::FactoryPreset> DeEsserProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // name, category, frequency, width, threshold, ratio, release, mode (0 split / 1 wide), output
    struct P {
        const char* name;
        const char* category;
        float freq, width, thr, ratio, rel, mode, out;
    };
    const P list[] = {
        {"Init", "", 6500, 1.17f, -27, 6, 60, 0, 0},
        {"Male Vocal", "Vocals/Male", 5200, 1.2f, -26, 5, 60, 0, 0},
        {"Male Deep Voice", "Vocals/Male", 4600, 1.0f, -28, 4, 70, 0, 0},
        {"Male Bright Mic", "Vocals/Male", 6000, 1.4f, -30, 7, 50, 0, 0},
        {"Female Vocal", "Vocals/Female", 7200, 1.2f, -26, 5, 55, 0, 0},
        {"Female Airy Pop", "Vocals/Female", 8400, 1.5f, -29, 6, 45, 0, 0},
        {"Female Belt", "Vocals/Female", 6800, 1.1f, -24, 8, 50, 0, 0},
        {"Rap Upfront", "Vocals/Rap", 5800, 1.3f, -31, 8, 40, 0, 0},
        {"Rap Fast Flow", "Vocals/Rap", 6200, 1.2f, -30, 10, 25, 0, 0},
        {"Trap Ad-libs", "Vocals/Rap", 7000, 1.6f, -33, 9, 30, 0, 0},
        {"Whisper Control", "Vocals/Special", 7500, 1.8f, -38, 6, 80, 0, 0},
        {"Podcast Voice", "Vocals/Special", 5500, 1.3f, -27, 4, 90, 1, 0},
        {"Choir Stack", "Vocals/Special", 6800, 1.6f, -30, 4, 120, 0, 0},
        {"Gentle Touch", "Vocals/Special", 6500, 1.2f, -20, 2.5f, 80, 0, 0},
        {"Hard Lisp Killer", "Vocals/Special", 6300, 0.9f, -40, 16, 35, 0, 0},
        {"Hi-Hat Tamer", "Instruments", 9000, 1.4f, -28, 5, 40, 0, 0},
        {"Cymbal Wash", "Instruments", 10500, 1.8f, -30, 4, 120, 0, 0},
        {"Acoustic Guitar Squeak", "Instruments", 3800, 1.0f, -30, 5, 60, 0, 0},
        {"Bright Synth", "Instruments", 4800, 2.0f, -26, 3, 80, 0, 0},
        {"Mix Bus Polish", "Mix", 7000, 2.0f, -22, 2, 100, 0, 0},
        {"Master Air Safety", "Mix", 9500, 2.2f, -18, 2, 140, 0, 0},
        {"Broadcast Wide", "Mix", 6000, 1.5f, -25, 4, 70, 1, 0},
        // Producers: Uruguayan names that describe the sound.
        {"Salus Sin Gas", "Producers/Torbey Beats", 6600, 1.3f, -24, 3, 70, 0, 0},
        {"Nix de Pomelo", "Producers/Torbey Beats", 7600, 1.0f, -34, 10, 30, 0, 0},
        {"Silbido del 121", "Producers/Torbey Beats", 8200, 1.4f, -32, 8, 35, 0, 0},
        {"Ta de Más", "Producers/Torbey Beats", 6000, 1.2f, -27, 5, 55, 0, 0},
        {"Manteca Calcar", "Producers/Torbey Beats", 5600, 1.6f, -28, 4, 100, 0, 0},
        {"Bizcocho Sin Grasa", "Producers/Pat Osk", 7200, 1.1f, -30, 7, 45, 0, 0},
        {"Medio y Medio", "Producers/Pat Osk", 6400, 1.5f, -26, 4, 65, 1, 0},
        {"Yerba Sin Palo", "Producers/Pat Osk", 6900, 0.8f, -35, 12, 30, 0, 0},
        {"Chajá Liviano", "Producers/Pat Osk", 8800, 1.8f, -25, 3, 90, 0, 0},
        {"Rambla en Invierno", "Producers/Pat Osk", 5400, 1.4f, -30, 6, 120, 0, 0},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list)
        out.push_back({u(p.name), u(p.category),
                       {{kFreq, p.freq}, {kWidth, p.width}, {kThreshold, p.thr}, {kRatio, p.ratio}, {kRelease, p.rel}, {kMode, p.mode}, {kOutput, p.out}}});
    return out;
}
