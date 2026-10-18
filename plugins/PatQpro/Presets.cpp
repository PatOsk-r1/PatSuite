// Factory presets of PatQpro: a list of bands per preset (unused bands stay off).
#include "plugins/PatQpro/Processor.h"

using namespace qpro;
using pat::dsp::EqBand;

namespace {
// type, freq, gain, q, slope index (0 = 6 dB/oct .. 8 = 96, 9 = brickwall), placement, dynamic range, threshold
struct B {
    int type;
    float freq, gain, q;
    int slope = 1, place = EqBand::Stereo;
    float range = 0.0f, thr = -30.0f;  // range != 0 makes the band dynamic
};
constexpr int Bell = EqBand::Bell, LowShelf = EqBand::LowShelf, HighShelf = EqBand::HighShelf, LowCut = EqBand::LowCut, HighCut = EqBand::HighCut,
              Notch = EqBand::Notch, BandPass = EqBand::BandPass, Tilt = EqBand::TiltShelf, FlatTilt = EqBand::FlatTilt;
constexpr int Mid = EqBand::Mid, Side = EqBand::Side;

pat::ui::FactoryPreset make(const char* name, const char* category, std::initializer_list<B> bands, int mode = 0, bool autoGain = false) {
    pat::ui::FactoryPreset p{juce::String::fromUTF8(name), juce::String::fromUTF8(category), {}};
    int i = 0;
    for (const auto& b : bands) {
        p.values.push_back({bandId(i, Used), 1.0f});
        p.values.push_back({bandId(i, Type), static_cast<float>(b.type)});
        p.values.push_back({bandId(i, Freq), b.freq});
        p.values.push_back({bandId(i, Gain), b.gain});
        p.values.push_back({bandId(i, Q), b.q});
        p.values.push_back({bandId(i, Slope), static_cast<float>(b.slope)});
        p.values.push_back({bandId(i, Place), static_cast<float>(b.place)});
        if (b.range != 0.0f) {
            p.values.push_back({bandId(i, Dyn), 1.0f});
            p.values.push_back({bandId(i, Range), b.range});
            p.values.push_back({bandId(i, Threshold), b.thr});
        }
        ++i;
    }
    p.values.push_back({kMode, static_cast<float>(mode)});
    p.values.push_back({kAutoGain, autoGain ? 1.0f : 0.0f});
    return p;
}
}  // namespace

std::vector<pat::ui::FactoryPreset> QproProcessor::factoryPresets() {
    return {
        make("Init", "", {}),
        // Vocals
        make("Vocal Presence", "Vocals", {{LowCut, 90, 0, 0.71f, 3}, {Bell, 300, -2.5f, 1.2f}, {Bell, 3200, 2.5f, 0.9f}, {HighShelf, 10000, 3, 0.71f}}),
        make("Vocal Air & Body", "Vocals", {{LowCut, 80, 0, 0.71f, 2}, {Bell, 180, 1.5f, 0.8f}, {Bell, 2500, -1.5f, 2.0f}, {HighShelf, 12000, 4, 0.6f}}),
        make("Rap Vocal Upfront", "Vocals", {{LowCut, 100, 0, 0.71f, 3}, {Bell, 250, -3, 1.0f}, {Bell, 1800, 2, 1.0f}, {Bell, 5000, 2.5f, 1.2f}, {HighShelf, 11000, 2, 0.71f}}),
        make("Dynamic De-Harsh", "Vocals", {{LowCut, 80, 0, 0.71f, 2}, {Bell, 3500, 0, 2.5f, 1, 0, -6, -28}, {Bell, 7000, 0, 3.0f, 1, 0, -5, -32}}),
        make("Dynamic De-Mud", "Vocals", {{Bell, 300, 0, 1.4f, 1, 0, -5, -26}, {HighShelf, 9000, 2, 0.71f}}),
        make("Podcast Voice", "Vocals", {{LowCut, 70, 0, 0.71f, 3}, {Bell, 150, 2, 0.8f}, {Bell, 400, -3, 1.2f}, {Bell, 4000, 2, 1.0f}, {HighCut, 15000, 0, 0.71f, 1}}),
        make("Backing Vocals Tuck", "Vocals", {{LowCut, 180, 0, 0.71f, 3}, {Bell, 2500, -3, 0.8f}, {HighShelf, 9000, 2, 0.71f}}),
        // Drums
        make("Kick Punch", "Drums", {{LowCut, 30, 0, 0.71f, 3}, {Bell, 60, 3, 1.2f}, {Bell, 350, -4, 1.4f}, {Bell, 3500, 3, 1.5f}}),
        make("808 Clean Sub", "Drums", {{LowCut, 25, 0, 0.71f, 5}, {Bell, 50, 2, 1.0f}, {Bell, 250, -3, 1.0f}, {HighCut, 6000, 0, 0.71f, 3}}),
        make("Snare Crack", "Drums", {{LowCut, 80, 0, 0.71f, 2}, {Bell, 200, 2, 1.4f}, {Bell, 900, -2, 2.0f}, {Bell, 5000, 3, 1.0f}}),
        make("Hi-Hat Silk", "Drums", {{LowCut, 400, 0, 0.71f, 3}, {Bell, 6500, -2, 2.0f}, {HighShelf, 12000, 3, 0.71f}}),
        make("Drum Bus Smile", "Drums", {{LowShelf, 80, 2, 0.71f}, {Bell, 400, -2, 0.9f}, {HighShelf, 9000, 2.5f, 0.71f}}),
        // Instruments
        make("Bass Definition", "Instruments", {{LowCut, 30, 0, 0.71f, 3}, {Bell, 80, 2, 1.0f}, {Bell, 250, -2.5f, 1.0f}, {Bell, 900, 2, 1.2f}}),
        make("Acoustic Guitar Sparkle", "Instruments", {{LowCut, 90, 0, 0.71f, 2}, {Bell, 200, -3, 1.2f}, {Bell, 3000, 1.5f, 1.0f}, {HighShelf, 10000, 3, 0.71f}}),
        make("Electric Guitar Cut-Through", "Instruments", {{LowCut, 100, 0, 0.71f, 3}, {Bell, 500, -2, 1.0f}, {Bell, 2200, 2.5f, 1.4f}, {HighCut, 9000, 0, 0.71f, 1}}),
        make("Piano Warmth", "Instruments", {{LowCut, 40, 0, 0.71f, 1}, {Bell, 250, 1.5f, 0.8f}, {Bell, 3000, -1.5f, 1.4f}, {HighShelf, 9000, 1.5f, 0.71f}}),
        make("Pad Clear Space", "Instruments", {{LowCut, 200, 0, 0.71f, 3}, {Bell, 2500, -3, 0.7f}, {HighCut, 12000, 0, 0.71f, 1}}),
        make("Synth Lead Bite", "Instruments", {{LowCut, 150, 0, 0.71f, 3}, {Bell, 1200, 2, 1.0f}, {Bell, 4000, 2, 1.5f}}),
        // Mix and master
        make("Master Gentle Smile", "Mastering", {{LowShelf, 60, 1, 0.71f}, {Bell, 2500, -0.7f, 0.7f}, {HighShelf, 12000, 1.5f, 0.71f}}, 2, true),
        make("Master Low-End Tidy", "Mastering", {{LowCut, 25, 0, 0.71f, 5}, {Bell, 250, -1, 1.0f, 1, 0, -2, -24}}, 2),
        make("Master Tilt Brighter", "Mastering", {{FlatTilt, 1000, 3, 1.0f}}, 2, true),
        make("Mid/Side Wide Air", "Mastering", {{LowCut, 120, 0, 0.71f, 2, Side}, {HighShelf, 8000, 3, 0.71f, 1, Side}, {Bell, 3000, -1, 0.8f, 1, Mid}}, 2),
        make("Mono Bass Below 100", "Mastering", {{LowCut, 100, 0, 0.71f, 4, Side}}, 2),
        make("Mix Bus Clarity", "Mix", {{LowCut, 25, 0, 0.71f, 3}, {Bell, 300, -1.5f, 0.8f}, {Bell, 4500, 1, 0.8f}}),
        // Effects
        make("Telephone", "Effect", {{LowCut, 400, 0, 0.71f, 3}, {HighCut, 3200, 0, 0.71f, 3}, {Bell, 1500, 4, 1.0f}}),
        make("AM Radio", "Effect", {{BandPass, 1200, 0, 0.6f, 3}, {Bell, 2000, 3, 1.5f}}),
        make("Lo-Fi Dusty", "Effect", {{LowCut, 150, 0, 0.71f, 2}, {HighCut, 5000, 0, 1.2f, 3}, {Bell, 800, 2, 0.7f}}),
        make("Underwater", "Effect", {{HighCut, 600, 0, 2.0f, 4}, {Bell, 200, 3, 1.0f}}),
        make("Notch Hum 50 Hz", "Repair", {{Notch, 50, 0, 8.0f}, {Notch, 100, 0, 8.0f}, {Notch, 150, 0, 8.0f}}),
        make("Notch Hum 60 Hz", "Repair", {{Notch, 60, 0, 8.0f}, {Notch, 120, 0, 8.0f}, {Notch, 180, 0, 8.0f}}),
        make("Brickwall Rumble Cut", "Repair", {{LowCut, 35, 0, 0.71f, 9}}, 2),
        // Producers: Uruguayan names that describe the sound.
        make("Sopa de Letras Conaprole", "Producers/Torbey Beats", {{LowCut, 35, 0, 0.71f, 3}, {Bell, 70, 3, 1.0f}, {Bell, 400, -3, 1.0f}, {HighShelf, 9000, 2, 0.71f}}),
        make("808 del Prado", "Producers/Torbey Beats", {{LowCut, 28, 0, 0.71f, 5}, {Bell, 55, 4, 1.2f}, {Bell, 220, -4, 1.2f}, {Bell, 1200, 2, 1.5f}}),
        make("Voz de Locutor de Carve", "Producers/Torbey Beats", {{LowCut, 70, 0, 0.71f, 3}, {Bell, 140, 3, 0.8f}, {Bell, 2800, 2, 1.0f}, {HighShelf, 11000, 2, 0.71f}}),
        make("Hi-Hat de Lata de Pilsen", "Producers/Torbey Beats", {{LowCut, 600, 0, 0.71f, 4}, {Bell, 8000, 3, 1.5f}, {Bell, 3500, -3, 2.0f}}),
        make("Radio del Taxi", "Producers/Torbey Beats", {{LowCut, 500, 0, 0.71f, 3}, {HighCut, 3500, 0, 1.5f, 3}, {Bell, 1800, 5, 1.2f}}),
        make("Brillo de Mate Nuevo", "Producers/Pat Osk", {{LowCut, 60, 0, 0.71f, 2}, {Bell, 350, -2, 1.0f}, {HighShelf, 7000, 4, 0.6f}}),
        make("Graves de la Llamada", "Producers/Pat Osk", {{LowShelf, 90, 4, 0.71f}, {Bell, 300, -3, 1.0f}, {Bell, 2500, 1.5f, 1.0f}}),
        make("Aire de Rocha", "Producers/Pat Osk", {{HighShelf, 10000, 4, 0.6f, 1, Side}, {LowCut, 150, 0, 0.71f, 2, Side}}, 2),
        make("Sin Barro, Bo", "Producers/Pat Osk", {{Bell, 250, 0, 1.2f, 1, 0, -6, -28}, {Bell, 500, -2, 1.5f}, {HighShelf, 9000, 2, 0.71f}}),
        make("Torta Frita Crocante", "Producers/Pat Osk", {{LowCut, 80, 0, 0.71f, 3}, {Bell, 3000, 3, 1.4f}, {Bell, 6000, 2, 2.0f}, {Bell, 400, -2, 1.0f}}),
    };
}
