// Factory presets of PatSquash. Values in real units; parameters left out keep their default.
#include "plugins/PatSquash/Processor.h"

using namespace squash;

std::vector<pat::ui::FactoryPreset> SquashProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // depth, time, upward, downward, input dB, output dB, low split, high split,
    // band gains (low, mid, high), threshold offset (dB added to every threshold: lower = more squash)
    struct P {
        const char* name;
        const char* category;
        float depth, time, up, down, in, out, xlo, xhi, gl, gm, gh, offset;
    };
    const P list[] = {
        {"Init", "", 0.5f, 1, 1, 1, 0, 0, 120, 2500, 0, 0, 0, 0},
        {"Classic Squash 100", "Full", 1.0f, 1, 1, 1, 0, -2, 120, 2500, 0, 0, 0, 0},
        {"Classic Squash 30", "Full", 0.3f, 1, 1, 1, 0, 0, 120, 2500, 0, 0, 0, 0},
        {"Gentle Glue", "Full", 0.25f, 1.5f, 0.6f, 1, 0, 0, 120, 2500, 0, 0, 0, 4},
        {"Brutal Smash", "Full", 1.0f, 0.5f, 1, 1, 4, -6, 120, 2500, 0, 0, 0, -6},
        {"Upward Only Detail", "Full", 0.6f, 1, 1, 0, 0, -2, 120, 2500, 0, 0, 0, 0},
        {"Downward Only Tame", "Full", 0.6f, 1, 0, 1, 0, 2, 120, 2500, 0, 0, 0, 0},
        {"808 Thicken", "Bass", 0.5f, 1.2f, 1, 0.8f, 0, 0, 90, 1500, 1, 2, -2, 0},
        {"Reese Grit", "Bass", 0.8f, 0.8f, 1, 1, 2, -3, 150, 2000, 0, 2, 2, -3},
        {"Dubstep Growl Glue", "Bass", 1.0f, 0.6f, 1, 1, 0, -4, 120, 2500, 0, 1, 3, -4},
        {"Sub Control", "Bass", 0.4f, 1.5f, 0.3f, 1, 0, 0, 100, 2000, 0, 0, 0, 3},
        {"Vocal Air & Presence", "Vocals", 0.35f, 1, 1, 0.7f, 0, 0, 200, 3500, -1, 0, 2, 0},
        {"Rap Vocal Upfront", "Vocals", 0.5f, 0.8f, 1, 1, 0, -1, 180, 3000, -1, 1, 1, -2},
        {"Whisper Lift", "Vocals", 0.6f, 1.2f, 1, 0.3f, 0, -2, 200, 4000, 0, 0, 3, 0},
        {"Drum Bus Punch", "Drums", 0.45f, 0.6f, 0.8f, 1, 0, 0, 120, 3000, 1, 0, 1, 0},
        {"Hi-Hat Sizzle", "Drums", 0.5f, 0.5f, 1, 0.8f, 0, 0, 300, 6000, 0, 0, 2, 0},
        {"Room Mic Crush", "Drums", 1.0f, 0.4f, 1, 1, 3, -5, 150, 3000, 0, 0, 0, -6},
        {"Lead Synth Bright", "Synths", 0.5f, 0.8f, 1, 1, 0, -1, 150, 2500, -2, 0, 2, 0},
        {"Pad Wide Bloom", "Synths", 0.4f, 2.0f, 1, 0.6f, 0, 0, 150, 3000, 0, 0, 1, 0},
        {"Pluck Sparkle", "Synths", 0.6f, 0.7f, 1, 1, 0, -2, 200, 3000, -1, 0, 3, -2},
        {"Future Bass Chords", "Synths", 0.8f, 0.8f, 1, 1, 0, -3, 120, 2500, 0, 1, 2, -3},
        {"Master Subtle", "Mastering", 0.15f, 1.5f, 0.7f, 1, 0, 0, 120, 2500, 0, 0, 0, 4},
        {"Master Loud & Bright", "Mastering", 0.3f, 1.2f, 1, 1, 0, 0, 100, 3000, 0, 0, 1, 2},
        {"Lo-Fi Squeeze", "Effect", 1.0f, 2.0f, 1, 1, 0, -4, 300, 2000, -4, 2, -6, -4},
        {"Radio Crunch", "Effect", 1.0f, 0.4f, 1, 1, 6, -8, 400, 2500, -8, 4, -6, -6},
        // Producers: Uruguayan names that describe the sound.
        {"Aplastado Como Bizcocho", "Producers/Torbey Beats", 1.0f, 0.8f, 1, 1, 2, -5, 120, 2500, 0, 0, 0, -4},
        {"Chivito Completo", "Producers/Torbey Beats", 0.7f, 1, 1, 1, 0, -2, 110, 2500, 2, 0, 2, -2},
        {"Pilsen en el Freezer", "Producers/Torbey Beats", 0.5f, 0.6f, 1, 0.8f, 0, -1, 150, 3500, -1, 0, 3, 0},
        {"Bajo del Cerro", "Producers/Torbey Beats", 0.6f, 1.3f, 1, 0.8f, 0, 0, 90, 1500, 2, 1, -2, 0},
        {"Ta Re Comprimido", "Producers/Pat Osk", 1.0f, 0.5f, 1, 1, 3, -6, 120, 2500, 0, 0, 0, -6},
        {"Mate con Azúcar", "Producers/Pat Osk", 0.35f, 1.5f, 0.8f, 0.8f, 0, 0, 150, 3000, 0, 1, 1, 2},
        {"Murga en el Tablado", "Producers/Pat Osk", 0.7f, 0.8f, 1, 1, 0, -2, 180, 3000, -1, 2, 2, -2},
        {"Feria del Domingo", "Producers/Pat Osk", 0.5f, 1, 1, 1, 0, -1, 200, 4000, -2, 1, 2, 0},
    };
    const float defDown[3] = {-28.0f, -30.0f, -32.0f}, defUp[3] = {-38.0f, -40.0f, -42.0f};
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list) {
        pat::ui::FactoryPreset f{u(p.name), u(p.category),
                                 {{kDepth, p.depth}, {kTime, p.time}, {kUpward, p.up}, {kDownward, p.down}, {kInput, p.in}, {kOutput, p.out},
                                  {kLowCross, p.xlo}, {kHighCross, p.xhi}, {bandId(0, Gain), p.gl}, {bandId(1, Gain), p.gm}, {bandId(2, Gain), p.gh}}};
        for (int b = 0; b < 3; ++b) {
            f.values.push_back({bandId(b, DownThr), juce::jlimit(-60.0f, 0.0f, defDown[b] + p.offset)});
            f.values.push_back({bandId(b, UpThr), juce::jlimit(-80.0f, 0.0f, defUp[b] + p.offset)});
        }
        out.push_back(std::move(f));
    }
    return out;
}
