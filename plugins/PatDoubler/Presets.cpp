// Factory presets of PatDoubler. Values in real units; parameters left out keep their default.
#include "plugins/PatDoubler/Processor.h"

using namespace doubler;

std::vector<pat::ui::FactoryPreset> DoublerProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // copies (1..4), variation, spread, width, thickness, then delay ms / detune cents / pan per copy
    struct P {
        const char* name;
        const char* category;
        int copies;
        float variation, spread, width, thickness;
        float delay[4], detune[4], pan[4];
    };
    const P list[] = {
        {"Init", "", 4, 0.3f, 1.0f, 1.0f, 0.5f, {14, 23, 31, 41}, {-9, 11, -17, 19}, {-0.8f, 0.8f, -0.45f, 0.45f}},
        {"Natural Double", "Vocals/Double", 1, 0.4f, 1.0f, 0.6f, 0.4f, {22, 23, 31, 41}, {7, 11, -17, 19}, {0.3f, 0.8f, -0.45f, 0.45f}},
        {"Tight Pair", "Vocals/Double", 2, 0.25f, 1.0f, 0.9f, 0.45f, {12, 17, 31, 41}, {-6, 7, -17, 19}, {-0.7f, 0.7f, -0.45f, 0.45f}},
        {"Wide Pair", "Vocals/Double", 2, 0.35f, 1.3f, 1.0f, 0.5f, {18, 27, 31, 41}, {-10, 12, -17, 19}, {-1, 1, -0.45f, 0.45f}},
        {"Hook Stack", "Vocals/Stack", 4, 0.4f, 1.2f, 1.0f, 0.65f, {14, 23, 31, 41}, {-9, 11, -17, 19}, {-0.8f, 0.8f, -0.45f, 0.45f}},
        {"Choir of Four", "Vocals/Stack", 4, 0.6f, 1.1f, 1.0f, 0.75f, {20, 33, 45, 58}, {-14, 16, -22, 24}, {-0.9f, 0.9f, -0.35f, 0.35f}},
        {"Gang Vocal", "Vocals/Stack", 4, 0.8f, 1.0f, 0.9f, 0.85f, {26, 38, 52, 66}, {-18, 20, -26, 28}, {-0.7f, 0.7f, -0.3f, 0.3f}},
        {"Rap Ad-lib Spread", "Vocals/Rap", 2, 0.2f, 1.4f, 1.0f, 0.4f, {10, 15, 31, 41}, {-5, 6, -17, 19}, {-1, 1, -0.45f, 0.45f}},
        {"Rap Hook Thick", "Vocals/Rap", 3, 0.3f, 1.2f, 1.0f, 0.6f, {12, 19, 27, 41}, {-8, 9, -13, 19}, {-0.85f, 0.85f, 0, 0.45f}},
        {"Subtle Thickener", "Vocals/Subtle", 2, 0.15f, 0.6f, 0.5f, 0.3f, {8, 11, 31, 41}, {-4, 4, -17, 19}, {-0.4f, 0.4f, -0.45f, 0.45f}},
        {"Center Fattener", "Vocals/Subtle", 2, 0.2f, 0.2f, 0.2f, 0.5f, {9, 13, 31, 41}, {-6, 6, -17, 19}, {-0.15f, 0.15f, -0.45f, 0.45f}},
        {"Slapback Twin", "Vocals/Effect", 1, 0.1f, 1.0f, 0.8f, 0.5f, {78, 23, 31, 41}, {0, 11, -17, 19}, {0.6f, 0.8f, -0.45f, 0.45f}},
        {"Chorus Shimmer", "Vocals/Effect", 4, 0.9f, 1.5f, 1.0f, 0.55f, {6, 9, 13, 17}, {-25, 25, -35, 35}, {-1, 1, -0.6f, 0.6f}},
        {"Detuned Robot Gang", "Vocals/Effect", 4, 0.0f, 1.0f, 1.0f, 0.8f, {5, 5, 5, 5}, {-50, 50, -35, 35}, {-1, 1, -0.5f, 0.5f}},
        {"Guitar Double", "Instruments", 1, 0.5f, 1.2f, 1.0f, 0.5f, {28, 23, 31, 41}, {6, 11, -17, 19}, {0.9f, 0.8f, -0.45f, 0.45f}},
        {"Synth Widener", "Instruments", 2, 0.3f, 1.5f, 1.0f, 0.4f, {11, 17, 31, 41}, {-12, 12, -17, 19}, {-1, 1, -0.45f, 0.45f}},
        {"Pad Ensemble", "Instruments", 4, 0.7f, 1.3f, 1.0f, 0.6f, {16, 27, 39, 53}, {-20, 22, -30, 32}, {-0.9f, 0.9f, -0.5f, 0.5f}},
        // Producers: Uruguayan names that describe the sound.
        {"Hinchada del Centenario", "Producers/Torbey Beats", 4, 0.85f, 1.2f, 1.0f, 0.9f, {24, 37, 51, 65}, {-20, 22, -28, 30}, {-0.9f, 0.9f, -0.4f, 0.4f}},
        {"Murga de Barrio", "Producers/Torbey Beats", 4, 0.7f, 1.0f, 0.95f, 0.8f, {20, 31, 44, 57}, {-16, 18, -24, 26}, {-0.8f, 0.8f, -0.3f, 0.3f}},
        {"Doble Pilsen", "Producers/Torbey Beats", 2, 0.3f, 1.3f, 1.0f, 0.55f, {13, 21, 31, 41}, {-9, 10, -17, 19}, {-1, 1, -0.45f, 0.45f}},
        {"Gemelos de Conaprole", "Producers/Torbey Beats", 2, 0.2f, 1.0f, 0.9f, 0.45f, {11, 16, 31, 41}, {-7, 7, -17, 19}, {-0.75f, 0.75f, -0.45f, 0.45f}},
        {"Coro de la Tienda Inglesa", "Producers/Torbey Beats", 4, 0.5f, 1.4f, 1.0f, 0.7f, {15, 25, 36, 48}, {-12, 14, -20, 22}, {-1, 1, -0.55f, 0.55f}},
        {"Alfajor Doble", "Producers/Pat Osk", 2, 0.35f, 1.1f, 1.0f, 0.6f, {15, 22, 31, 41}, {-11, 9, -17, 19}, {-0.85f, 0.85f, -0.45f, 0.45f}},
        {"Bo, Qué Eco", "Producers/Pat Osk", 1, 0.1f, 1.0f, 1.0f, 0.4f, {72, 23, 31, 41}, {-3, 11, -17, 19}, {-0.7f, 0.8f, -0.45f, 0.45f}},
        {"Grappamiel Mareado", "Producers/Pat Osk", 4, 1.0f, 1.5f, 1.0f, 0.6f, {9, 14, 19, 26}, {-35, 38, -45, 48}, {-1, 1, -0.6f, 0.6f}},
        {"Rueda de Mate", "Producers/Pat Osk", 3, 0.45f, 1.2f, 1.0f, 0.65f, {17, 28, 40, 41}, {-10, 12, -16, 19}, {-0.9f, 0.9f, 0.1f, 0.45f}},
        {"Bizcochos Surtidos", "Producers/Pat Osk", 4, 0.6f, 1.25f, 1.0f, 0.7f, {12, 21, 33, 47}, {-15, 13, -21, 25}, {-0.95f, 0.85f, -0.4f, 0.5f}},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list) {
        pat::ui::FactoryPreset f{u(p.name), u(p.category),
                                 {{kCopies, static_cast<float>(p.copies - 1)}, {kVariation, p.variation}, {kSpread, p.spread}, {kWidth, p.width}, {kThickness, p.thickness}}};
        for (int c = 0; c < 4; ++c) {
            f.values.push_back({delayId(c), p.delay[c]});
            f.values.push_back({detuneId(c), p.detune[c]});
            f.values.push_back({panId(c), p.pan[c]});
        }
        out.push_back(std::move(f));
    }
    return out;
}
