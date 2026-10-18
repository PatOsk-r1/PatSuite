// Factory presets of PatChain. Values in real units; parameters left out keep their default.
#include "plugins/PatChain/Processor.h"

using namespace sidechain;

std::vector<pat::ui::FactoryPreset> SidechainProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // trigger (0 auto / 1 tempo / 2 sidechain), pump, rate (0 1/4, 1 1/8, 2 1/16; stored +2, see
    // divisionName), threshold, attack, release, knee, ratio, sidechain low cut, sidechain high cut,
    // shape (0 Classic, 1 Smooth, 2 Punchy, 3 Long, 4 Cut, 5 Bounce, 6 Swell, 7 Breathe)
    struct P {
        const char* name;
        const char* category;
        float trig, pump, rate, thr, att, rel, knee, ratio, scLow, scHigh;
        float shape = 0;
    };
    const P list[] = {
        {"Init", "", 0, 0.7f, 0, -24, 2, 180, 6, 8, 20, 150},
        {"Classic EDM Pump", "Tempo/Pump", 1, 0.85f, 0, -24, 2, 220, 6, 8, 20, 150},
        {"House Breath", "Tempo/Pump", 1, 0.55f, 0, -24, 5, 260, 6, 8, 20, 150},
        {"Eighths Bounce", "Tempo/Pump", 1, 0.7f, 1, -24, 2, 120, 6, 8, 20, 150},
        {"Sixteenth Stutter", "Tempo/Pump", 1, 0.9f, 2, -24, 1, 60, 6, 8, 20, 150},
        {"Future Bass Swell", "Tempo/Pump", 1, 1.0f, 0, -24, 8, 380, 6, 8, 20, 150},
        {"Lo-fi Wobble", "Tempo/Pump", 1, 0.35f, 0, -24, 10, 300, 6, 8, 20, 150},
        {"Kick Duck Tight", "Sidechain/Kick", 2, 0.8f, 0, -30, 0.5f, 90, 3, 12, 30, 140},
        {"Kick Duck Smooth", "Sidechain/Kick", 2, 0.7f, 0, -28, 3, 220, 8, 6, 30, 160},
        {"808 Clears the Bass", "Sidechain/Kick", 2, 0.9f, 0, -32, 1, 150, 4, 10, 25, 120},
        {"Kick Over Pads", "Sidechain/Kick", 2, 0.75f, 0, -26, 5, 350, 9, 6, 30, 160},
        {"Voice Over Music", "Sidechain/Voice", 2, 0.6f, 0, -36, 15, 500, 10, 4, 150, 4000},
        {"Podcast Ducker", "Sidechain/Voice", 2, 0.75f, 0, -40, 20, 700, 12, 6, 120, 5000},
        {"Snare Gap", "Sidechain/Drums", 2, 0.6f, 0, -26, 1, 110, 4, 8, 150, 2000},
        {"Hat Chop", "Sidechain/Drums", 2, 0.5f, 0, -30, 0.3f, 40, 2, 10, 500, 20000},
        {"Auto Glue", "Auto/Compression", 0, 0.4f, 0, -18, 10, 200, 8, 3, 20, 20000},
        {"Auto Low-End Pump", "Auto/Compression", 0, 0.75f, 0, -24, 2, 180, 6, 8, 20, 150},
        {"Auto Heavy Breath", "Auto/Compression", 0, 0.95f, 0, -30, 1, 280, 4, 14, 20, 200},
        // Producers: Uruguayan names that describe the movement.
        {"Bondi de CUTCSA", "Producers/Torbey Beats", 1, 0.8f, 1, -24, 2, 160, 6, 8, 20, 150},
        {"Latido de Tambor", "Producers/Torbey Beats", 2, 0.85f, 0, -30, 1, 140, 4, 10, 30, 150},
        {"Pilsen Bien Helada", "Producers/Torbey Beats", 1, 0.65f, 0, -24, 4, 240, 6, 8, 20, 150},
        {"Ola de Punta del Este", "Producers/Torbey Beats", 1, 1.0f, 0, -24, 12, 420, 6, 8, 20, 150},
        {"Candombe del Barrio Sur", "Producers/Torbey Beats", 1, 0.75f, 2, -24, 1, 70, 6, 8, 20, 150},
        {"Feria de Tristán Narvaja", "Producers/Pat Osk", 2, 0.7f, 0, -28, 3, 200, 8, 6, 30, 160},
        {"Mate Que Respira", "Producers/Pat Osk", 1, 0.5f, 0, -24, 8, 320, 6, 8, 20, 150},
        {"Pique de Torta Frita", "Producers/Pat Osk", 2, 0.95f, 0, -34, 0.5f, 100, 3, 14, 25, 130},
        {"Ta Pronto, Bo", "Producers/Pat Osk", 1, 0.9f, 2, -24, 1, 50, 6, 8, 20, 150},
        {"Chivito al Pan", "Producers/Pat Osk", 2, 0.8f, 0, -28, 2, 260, 6, 9, 30, 150},
        // One-knob tempo shapes (no routing needed).
        {"Smooth Quarter", "Tempo/Shapes", 1, 0.85f, 0, -24, 3, 180, 6, 8, 20, 150, 1},
        {"Punchy Quarter", "Tempo/Shapes", 1, 0.9f, 0, -24, 3, 180, 6, 8, 20, 150, 2},
        {"Long Half Note", "Tempo/Shapes", 1, 0.8f, -1, -24, 3, 180, 6, 8, 20, 150, 3},
        {"Gate Cut Eighths", "Tempo/Shapes", 1, 1.0f, 1, -24, 3, 180, 6, 8, 20, 150, 4},
        {"Double Bounce", "Tempo/Shapes", 1, 0.85f, 0, -24, 3, 180, 6, 8, 20, 150, 5},
        {"Reverse Swell", "Tempo/Shapes", 1, 0.9f, 0, -24, 3, 180, 6, 8, 20, 150, 6},
        {"Breathing Pad", "Tempo/Shapes", 1, 0.7f, 0, -24, 3, 180, 6, 8, 20, 150, 7},
        {"Trance Gate 1/16", "Tempo/Shapes", 1, 1.0f, 2, -24, 3, 180, 6, 8, 20, 150, 4},
        {"Mandioca Frita", "Producers/Torbey Beats", 1, 0.95f, 0, -24, 3, 180, 6, 8, 20, 150, 2},
        {"Respira la Rambla", "Producers/Pat Osk", 1, 0.75f, 0, -24, 3, 180, 6, 8, 20, 150, 7},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list)
        out.push_back({u(p.name), u(p.category),
                       {{kTrigger, p.trig}, {kPump, p.pump}, {kRate, p.rate + 2}, {kThreshold, p.thr}, {kAttack, p.att}, {kRelease, p.rel},
                        {kKnee, p.knee}, {kRatio, p.ratio}, {kScLow, p.scLow}, {kScHigh, p.scHigh}, {kShape, p.shape}}});
    return out;
}
