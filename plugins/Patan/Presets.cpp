// Factory presets of Patan: parameter values plus the drawn curve (extra "curve").
// Division index: 0 4 bars, 1 2 bars, 2 1 bar, 3 1/2, 4 1/2 dot, 5 1/2 tri, 6 1/4, 7 1/4 dot,
// 8 1/4 tri, 9 1/8, 10 1/8 dot, 11 1/8 tri, 12 1/16, 13 1/16 dot, 14 1/16 tri, 15 1/32.
#include "plugins/Patan/Processor.h"

using namespace patan;
using pat::dsp::CurvePoint;
using pat::dsp::PanCurve;

namespace {
std::string shapeText(int shape, unsigned seed = 1) {
    PanCurve c;
    c.setPoints(PanCurve::shape(shape, seed));
    return c.toString();
}
std::string pointsText(std::vector<CurvePoint> pts) {
    PanCurve c;
    c.setPoints(std::move(pts));
    return c.toString();
}
}  // namespace

std::vector<pat::ui::FactoryPreset> PatanProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    using P = CurvePoint;
    // Hand-drawn curves used by several presets.
    const std::string zigzag = pointsText({{0, -1, 0, P::Bend}, {0.125f, 1, 0, P::Bend}, {0.25f, -0.5f, 0, P::Bend}, {0.375f, 0.5f, 0, P::Bend},
                                           {0.5f, -1, 0, P::Bend}, {0.75f, 1, 0, P::Bend}});
    const std::string triplet = pointsText({{0, -1, 0, P::Hold}, {0.333f, 0, 0, P::Hold}, {0.666f, 1, 0, P::Hold}});
    const std::string swing = pointsText({{0, -1, 0, P::Smooth}, {0.62f, 1, 0, P::Smooth}});
    const std::string pulseGate = pointsText({{0, 1, -0.5f, P::Bend}, {0.25f, -1, 0, P::Hold}, {0.5f, 1, -0.5f, P::Bend}, {0.75f, -1, 0, P::Hold}});
    const std::string fadeIn = pointsText({{0, -1, 0.5f, P::Bend}, {0.95f, 1, 0, P::Hold}});
    const std::string offbeat = pointsText({{0, -1, 0, P::Hold}, {0.5f, 1, 0, P::Smooth}, {0.75f, -1, 0, P::Smooth}});
    const std::string heartbeat =
        pointsText({{0, 1, 0, P::Bend}, {0.08f, -0.6f, 0, P::Bend}, {0.16f, 0.9f, 0, P::Bend}, {0.26f, -1, 0, P::Hold}});
    const std::string corner = pointsText({{0, -1, 0, P::Hold}, {0.25f, -1, 0, P::Smooth}, {0.5f, 1, 0, P::Hold}, {0.75f, 1, 0, P::Smooth}});
    const std::string breathe = pointsText({{0, -1, 0.3f, P::Bend}, {0.6f, 1, -0.4f, P::Bend}});

    // name, category, curve, mode (0 pan / 1 volume / 2 width), sync, division, rate Hz, depth,
    // phase, smooth ms, center, law, width
    struct Pre {
        const char* name;
        const char* category;
        std::string curve;
        float mode, sync, division, rate, depth, phase, smooth, center, law, width;
    };
    const Pre list[] = {
        {"Init", "", shapeText(PanCurve::Sine), 0, 1, 2, 0.5f, 1, 0, 5, 0, 0, 1},
        {"Slow Sweep 4 Bars", "Pan/Smooth", shapeText(PanCurve::Sine), 0, 1, 0, 0.5f, 1, 0, 10, 0, 1, 1},
        {"Gentle Drift", "Pan/Smooth", shapeText(PanCurve::Sine), 0, 1, 1, 0.5f, 0.45f, 0, 30, 0, 1, 1},
        {"Quarter Swing", "Pan/Smooth", swing, 0, 1, 6, 0.5f, 0.8f, 0, 8, 0, 1, 1},
        {"Triangle Ride", "Pan/Smooth", shapeText(PanCurve::Triangle), 0, 1, 3, 0.5f, 0.9f, 0, 5, 0, 1, 1},
        {"Hard L-R 1/8", "Pan/Rhythmic", shapeText(PanCurve::Square), 0, 1, 9, 0.5f, 1, 0, 3, 0, 0, 1},
        {"Ping Pong 1/16", "Pan/Rhythmic", shapeText(PanCurve::Square), 0, 1, 12, 0.5f, 0.9f, 0, 2, 0, 0, 1},
        {"Triplet Walk", "Pan/Rhythmic", triplet, 0, 1, 2, 0.5f, 1, 0, 6, 0, 1, 1},
        {"Zigzag Bar", "Pan/Rhythmic", zigzag, 0, 1, 2, 0.5f, 1, 0, 4, 0, 1, 1},
        {"Steps Around", "Pan/Rhythmic", shapeText(PanCurve::Steps4), 0, 1, 3, 0.5f, 1, 0, 8, 0, 1, 1},
        {"Random Scatter", "Pan/Rhythmic", shapeText(PanCurve::Random, 7), 0, 1, 2, 0.5f, 0.85f, 0, 10, 0, 1, 1},
        {"Corner Hopper", "Pan/Rhythmic", corner, 0, 1, 3, 0.5f, 1, 0, 5, 0, 1, 1},
        {"Leaning Right Wobble", "Pan/Placement", shapeText(PanCurve::Sine), 0, 1, 6, 0.5f, 0.3f, 0, 10, 0.4f, 1, 1},
        {"Hi-Hat Shuffle", "Pan/Placement", shapeText(PanCurve::Square), 0, 1, 12, 0.5f, 0.45f, 0, 4, 0, 1, 1},
        {"Free 0.2 Hz Orbit", "Pan/Free", shapeText(PanCurve::Sine), 0, 0, 2, 0.2f, 1, 0, 10, 0, 1, 1},
        {"Free 6 Hz Rotor", "Pan/Free", shapeText(PanCurve::Sine), 0, 0, 2, 6.0f, 0.7f, 0, 1, 0, 1, 1},
        {"Classic Tremolo", "Volume/Tremolo", shapeText(PanCurve::Sine), 1, 1, 9, 0.5f, 0.6f, 0, 2, 0, 0, 1},
        {"Vintage Amp Tremolo", "Volume/Tremolo", shapeText(PanCurve::Sine), 1, 0, 2, 5.5f, 0.5f, 0, 1, 0, 0, 1},
        {"Trance Gate 1/16", "Volume/Gate", shapeText(PanCurve::Gate), 1, 1, 12, 0.5f, 1, 0, 1.5f, 0, 0, 1},
        {"Pulse Chop 1/8", "Volume/Gate", pulseGate, 1, 1, 2, 0.5f, 1, 0, 1, 0, 0, 1},
        {"Fake Sidechain", "Volume/Gate", fadeIn, 1, 1, 6, 0.5f, 0.85f, 0, 2, 0, 0, 1},
        {"Offbeat Bounce", "Volume/Gate", offbeat, 1, 1, 6, 0.5f, 0.9f, 0, 3, 0, 0, 1},
        {"Heartbeat", "Volume/Gate", heartbeat, 1, 1, 6, 0.5f, 0.8f, 0, 2, 0, 0, 1},
        {"Breathing Width", "Width", breathe, 2, 1, 1, 0.5f, 1, 0, 20, 0, 0, 1},
        {"Mono to Wide Bar", "Width", shapeText(PanCurve::SawUp), 2, 1, 2, 0.5f, 1, 0, 10, 0, 0, 1},
        {"Width Flutter", "Width", shapeText(PanCurve::Sine), 2, 1, 9, 0.5f, 0.6f, 0, 5, 0, 0, 1},
        // Producers: Uruguayan names that describe the movement.
        {"Mate Que Gira", "Producers/Torbey Beats", shapeText(PanCurve::Sine), 0, 1, 2, 0.5f, 0.9f, 0, 12, 0, 1, 1},
        {"Bondi Por 18 de Julio", "Producers/Torbey Beats", corner, 0, 1, 2, 0.5f, 1, 90, 6, 0, 1, 1},
        {"Candombe L-R", "Producers/Torbey Beats", triplet, 0, 1, 6, 0.5f, 1, 0, 3, 0, 0, 1},
        {"Olas de la Rambla", "Producers/Torbey Beats", swing, 0, 1, 1, 0.5f, 1, 0, 25, 0, 1, 1.3f},
        {"Pilsen Mareada", "Producers/Torbey Beats", shapeText(PanCurve::Random, 3), 0, 1, 3, 0.5f, 0.9f, 0, 40, 0, 1, 1},
        {"Picada de Hi-Hats", "Producers/Torbey Beats", shapeText(PanCurve::Square), 0, 1, 15, 0.5f, 0.7f, 0, 1, 0, 0, 1},
        {"Ventilador de Ta-Ta", "Producers/Pat Osk", shapeText(PanCurve::Sine), 0, 0, 2, 4.0f, 1, 0, 2, 0, 1, 1},
        {"Pampero en Rocha", "Producers/Pat Osk", breathe, 2, 1, 0, 0.5f, 1, 0, 30, 0, 0, 1.4f},
        {"Ta, Ta, Ta Gate", "Producers/Pat Osk", shapeText(PanCurve::Gate), 1, 1, 11, 0.5f, 1, 0, 1, 0, 0, 1},
        {"Torta Frita Rebotando", "Producers/Pat Osk", shapeText(PanCurve::Bounce), 0, 1, 6, 0.5f, 1, 0, 4, 0, 1, 1},
        {"Bizcochos en Zigzag", "Producers/Pat Osk", zigzag, 0, 1, 3, 0.5f, 1, 45, 6, 0, 1, 1},
        {"Latido Celeste", "Producers/Pat Osk", heartbeat, 1, 1, 6, 0.5f, 0.75f, 0, 2, 0, 0, 1},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list)
        out.push_back({u(p.name),
                       u(p.category),
                       {{kMode, p.mode},
                        {kSync, p.sync},
                        {kDivision, p.division},
                        {kRate, p.rate},
                        {kDepth, p.depth},
                        {kPhase, p.phase},
                        {kSmooth, p.smooth},
                        {kCenter, p.center},
                        {kLaw, p.law},
                        {kWidth, p.width}},
                       {{"curve", juce::String(p.curve)}}});
    return out;
}
