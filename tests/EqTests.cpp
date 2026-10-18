// PatQpro engine tests: filter shapes, the three processing modes, mid/side placement, dynamic
// bands, solo, the 2x oversampler, the analyzer and EQ match.
#include "dsp/eq/EqAnalysis.h"
#include "dsp/eq/EqEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;

double db(double v) { return 20.0 * std::log10(std::max(v, 1e-12)); }

EqBand band(int type, float freq, float gain, float q = 1.0f, int slope = 1) {
    EqBand b;
    b.used = true;
    b.type = type;
    b.freq = freq;
    b.gain = gain;
    b.q = q;
    b.slope = slope;
    return b;
}

double digitalDb(const EqBand& b, double hz, double fs = kSr) {
    Biquad s[EqDesign::kMaxSections];
    const int n = EqDesign::digital(b, b.gain, fs, s);
    return db(std::abs(EqDesign::digitalResponse(s, n, hz, fs)));
}

// Gain (dB) of a sine at hz through the engine, measured after it settles.
double engineGainDb(EqEngine& e, const std::vector<EqBand>& bands, const EqGlobal& g, double hz, bool stereoDiff = false) {
    std::vector<EqBand> all(EqEngine::kBands);
    for (std::size_t i = 0; i < bands.size(); ++i) all[i] = bands[i];
    const int n = static_cast<int>(kSr * 0.6);
    std::vector<float> l(static_cast<std::size_t>(n)), r(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        l[static_cast<std::size_t>(i)] = 0.25f * static_cast<float>(std::sin(2.0 * kPi * hz * i / kSr));
        r[static_cast<std::size_t>(i)] = stereoDiff ? -l[static_cast<std::size_t>(i)] : l[static_cast<std::size_t>(i)];
    }
    for (int i = 0; i < n; i += 480) e.process(l.data() + i, r.data() + i, nullptr, nullptr, std::min(480, n - i), all.data(), g);
    float peak = 0;
    for (int i = n / 2; i < n; ++i) peak = std::max(peak, std::abs(l[static_cast<std::size_t>(i)]));
    return db(peak / 0.25);
}
}  // namespace

TEST(EqFilters, BellHasItsGainAtTheFrequency) {
    const auto b = band(EqBand::Bell, 1000, 6.0f, 1.0f);
    EXPECT_NEAR(digitalDb(b, 1000), 6.0, 0.05);
    EXPECT_NEAR(digitalDb(b, 50), 0.0, 0.1);
    EXPECT_NEAR(digitalDb(b, 15000), 0.0, 0.2);
}

TEST(EqFilters, ShelvesHalfGainAtTheFrequency) {
    for (int slope : {0, 1, 3, 6}) {
        const auto lo = band(EqBand::LowShelf, 500, -12.0f, 0.7071f, slope);
        EXPECT_NEAR(digitalDb(lo, 20), -12.0, 0.3) << slope;
        EXPECT_NEAR(digitalDb(lo, 500), -6.0, 0.3) << slope;
        EXPECT_NEAR(digitalDb(lo, 15000), 0.0, 0.3) << slope;
        const auto hi = band(EqBand::HighShelf, 2000, 9.0f, 0.7071f, slope);
        EXPECT_NEAR(digitalDb(hi, 2000), 4.5, 0.3) << slope;
        EXPECT_NEAR(digitalDb(hi, 30), 0.0, 0.2) << slope;
    }
    // Steeper slope: sharper transition.
    EXPECT_LT(digitalDb(band(EqBand::LowShelf, 500, -12.0f, 0.7071f, 6), 250), digitalDb(band(EqBand::LowShelf, 500, -12.0f, 0.7071f, 1), 250));
}

TEST(EqFilters, CutsHaveTheirSlope) {
    const int slopes[] = {6, 12, 18, 24, 30, 36, 48, 72, 96};
    for (int i = 0; i < 9; ++i) {
        const auto hp = band(EqBand::LowCut, 1000, 0, 0.7071f, i);
        EXPECT_NEAR(digitalDb(hp, 1000), -3.01, 0.1) << slopes[i];
        EXPECT_NEAR(digitalDb(hp, 250), -2.0 * slopes[i], 0.6 + slopes[i] * 0.01) << slopes[i];  // two octaves down
        EXPECT_NEAR(digitalDb(hp, 10000), 0.0, 0.1) << slopes[i];
        const auto lp = band(EqBand::HighCut, 1000, 0, 0.7071f, i);
        EXPECT_NEAR(digitalDb(lp, 1000), -3.01, 0.15) << slopes[i];
        EXPECT_NEAR(digitalDb(lp, 50), 0.0, 0.1) << slopes[i];
    }
    // Q above 0.707 adds a resonant bump.
    EXPECT_GT(digitalDb(band(EqBand::LowCut, 1000, 0, 4.0f, 3), 1000), 6.0);
}

TEST(EqFilters, NotchBandPassAndTilts) {
    EXPECT_LT(digitalDb(band(EqBand::Notch, 1000, 0, 4.0f), 1000), -40.0);
    EXPECT_NEAR(digitalDb(band(EqBand::Notch, 1000, 0, 4.0f), 200), 0.0, 0.1);
    EXPECT_NEAR(digitalDb(band(EqBand::BandPass, 1000, 0, 2.0f), 1000), 0.0, 0.05);
    EXPECT_LT(digitalDb(band(EqBand::BandPass, 1000, 0, 2.0f), 100), -18.0);
    const auto tilt = band(EqBand::TiltShelf, 1000, 6.0f, 0.7071f);
    EXPECT_NEAR(digitalDb(tilt, 20), -3.0, 0.2);
    EXPECT_NEAR(digitalDb(tilt, 15000), 3.0, 0.3);
    EXPECT_NEAR(digitalDb(tilt, 1000), 0.0, 0.2);
    const auto flat = band(EqBand::FlatTilt, 1000, 10.0f);
    EXPECT_NEAR(digitalDb(flat, 1000), 0.0, 0.1);
    const double slope1 = digitalDb(flat, 4000) - digitalDb(flat, 1000), slope2 = digitalDb(flat, 250) - digitalDb(flat, 62.5);
    EXPECT_GT(slope1, 1.0);  // a steady rise
    EXPECT_NEAR(slope1, slope2, 0.8);
}

TEST(EqEngine, ZeroLatencyBell) {
    EqEngine e;
    e.prepare(kSr, 1);
    EqGlobal g;
    EXPECT_NEAR(engineGainDb(e, {band(EqBand::Bell, 1000, 6.0f)}, g, 1000), 6.0, 0.1);
    EXPECT_NEAR(engineGainDb(e, {band(EqBand::Bell, 1000, 6.0f)}, g, 100), 0.0, 0.1);
    g.gainScale = 0.5f;
    EXPECT_NEAR(engineGainDb(e, {band(EqBand::Bell, 1000, 6.0f)}, g, 1000), 3.0, 0.1);
    g.gainScale = 1.0f;
    g.outputDb = -6.0f;
    EXPECT_NEAR(engineGainDb(e, {}, g, 1000), -6.0, 0.1);
    EXPECT_EQ(EqEngine::latency(EqGlobal::ZeroLatency, 1, kSr), 0);
}

TEST(EqEngine, NaturalModeKeepsTheTopEnd) {
    // A wide bell at 16 kHz at 44.1 kHz: the bilinear transform squeezes it near Nyquist,
    // natural mode (2x) stays much closer to the analog curve.
    const double fs = 44100.0;
    const auto b = band(EqBand::Bell, 16000, 9.0f, 0.5f);
    const double ideal = db(EqDesign::idealMagnitude(b, 9.0f, 20000.0));
    const double zl = digitalDb(b, 20000.0, fs);
    const double nat = digitalDb(b, 20000.0, 2.0 * fs);
    EXPECT_LT(std::abs(nat - ideal), std::abs(zl - ideal) * 0.5);
    EqEngine e;
    e.prepare(kSr, 1);
    EqGlobal g;
    g.mode = EqGlobal::Natural;
    EXPECT_NEAR(engineGainDb(e, {band(EqBand::Bell, 1000, 6.0f)}, g, 1000), 6.0, 0.15);
    EXPECT_EQ(EqEngine::latency(EqGlobal::Natural, 1, 44100.0), 3);
    EXPECT_EQ(EqEngine::latency(EqGlobal::Natural, 1, 96000.0), 0);
}

TEST(Oversampler2x, PassesAudioAndHasItsLatency) {
    Oversampler2x os;
    const int n = 4800;
    std::vector<float> out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        float a, b;
        os.up(static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / kSr)), a, b);
        out[static_cast<std::size_t>(i)] = os.down(a, b);
    }
    float peak = 0;
    for (int i = n / 2; i < n; ++i) peak = std::max(peak, std::abs(out[static_cast<std::size_t>(i)]));
    EXPECT_NEAR(peak, 1.0f, 0.01f);
    // Low-frequency delay: compare against a delayed copy.
    double bestErr = 1e9;
    int best = 0;
    for (int d = 0; d < 8; ++d) {
        double err = 0;
        for (int i = n / 2; i < n; ++i) {
            const double x = std::sin(2.0 * kPi * 1000.0 * (i - d) / kSr);
            err += (out[static_cast<std::size_t>(i)] - x) * (out[static_cast<std::size_t>(i)] - x);
        }
        if (err < bestErr) {
            bestErr = err;
            best = d;
        }
    }
    EXPECT_EQ(best, static_cast<int>(Oversampler2x::kLatency));
}

TEST(EqEngine, LinearPhaseMatchesAndIsSymmetric) {
    EqEngine e;
    e.prepare(kSr, 1);
    EqGlobal g;
    g.mode = EqGlobal::Linear;
    std::vector<EqBand> bands(EqEngine::kBands);
    bands[0] = band(EqBand::Bell, 1000, 6.0f);
    e.linear().publish(LinearPhase::build(bands.data(), EqEngine::kBands, 1.0f, kSr, 1));
    EXPECT_NEAR(engineGainDb(e, {bands[0]}, g, 1000), 6.0, 0.15);
    EXPECT_NEAR(engineGainDb(e, {bands[0]}, g, 100), 0.0, 0.15);

    // Impulse response: symmetric around the latency (no phase shift).
    EqEngine e2;
    e2.prepare(kSr, 1);
    e2.linear().publish(LinearPhase::build(bands.data(), EqEngine::kBands, 1.0f, kSr, 1));
    const int lat = EqEngine::latency(EqGlobal::Linear, 1, kSr);
    EXPECT_EQ(lat, 512 + 2048);
    const int n = 2 * lat + 1024;
    std::vector<float> l(static_cast<std::size_t>(n), 0.0f), r(static_cast<std::size_t>(n), 0.0f);
    // Prime with silence so the new kernel is in place, then the impulse.
    std::vector<float> zl(2048, 0.0f), zr(2048, 0.0f);
    e2.process(zl.data(), zr.data(), nullptr, nullptr, 2048, bands.data(), g);
    l[0] = r[0] = 1.0f;
    for (int i = 0; i < n; i += 256) e2.process(l.data() + i, r.data() + i, nullptr, nullptr, std::min(256, n - i), bands.data(), g);
    int peak = 0;
    for (int i = 0; i < n; ++i)
        if (std::abs(l[static_cast<std::size_t>(i)]) > std::abs(l[static_cast<std::size_t>(peak)])) peak = i;
    EXPECT_EQ(peak, lat);
    for (int k = 1; k < 300; ++k) EXPECT_NEAR(l[static_cast<std::size_t>(lat - k)], l[static_cast<std::size_t>(lat + k)], 2e-4f) << k;
}

TEST(EqEngine, MidSidePlacement) {
    EqEngine e;
    e.prepare(kSr, 1);
    EqGlobal g;
    auto side = band(EqBand::Bell, 1000, -12.0f);
    side.place = EqBand::Side;
    EXPECT_NEAR(engineGainDb(e, {side}, g, 1000, false), 0.0, 0.1);    // mono: no side signal
    EXPECT_NEAR(engineGainDb(e, {side}, g, 1000, true), -12.0, 0.15);  // pure side
    auto mid = band(EqBand::Bell, 1000, 6.0f);
    mid.place = EqBand::Mid;
    EXPECT_NEAR(engineGainDb(e, {mid}, g, 1000, false), 6.0, 0.15);
    EXPECT_NEAR(engineGainDb(e, {mid}, g, 1000, true), 0.0, 0.1);
    auto left = band(EqBand::Bell, 1000, 6.0f);
    left.place = EqBand::Right;  // left channel (measured) untouched
    EXPECT_NEAR(engineGainDb(e, {left}, g, 1000), 0.0, 0.1);
    // Linear phase with a mid/side band.
    EqEngine lp;
    lp.prepare(kSr, 0);
    g.mode = EqGlobal::Linear;
    std::vector<EqBand> all(EqEngine::kBands);
    all[0] = side;
    lp.linear().publish(LinearPhase::build(all.data(), EqEngine::kBands, 1.0f, kSr, 0));
    EXPECT_NEAR(engineGainDb(lp, {side}, g, 1000, true), -12.0, 0.3);
    EXPECT_NEAR(engineGainDb(lp, {side}, g, 1000, false), 0.0, 0.2);
}

TEST(EqEngine, DynamicBandCutsWhenLoud) {
    EqEngine e;
    e.prepare(kSr, 1);
    EqGlobal g;
    auto d = band(EqBand::Bell, 1000, 0.0f, 1.0f);
    d.dynamic = true;
    d.dynRange = -12.0f;
    d.dynThreshold = -30.0f;
    // Sine at 0.25 (-12 dB) is 18 dB over the threshold: the full range applies.
    EXPECT_NEAR(engineGainDb(e, {d}, g, 1000), -12.0, 0.6);
    EXPECT_NEAR(e.dynamicGainDb(0), -12.0f, 0.5f);
    d.dynThreshold = 0.0f;  // never reached: no change
    EqEngine e2;
    e2.prepare(kSr, 1);
    EXPECT_NEAR(engineGainDb(e2, {d}, g, 1000), 0.0, 0.2);
}

TEST(EqEngine, SoloPlaysOnlyTheBandRegion) {
    EqEngine e;
    e.prepare(kSr, 1);
    EqGlobal g;
    g.soloBand = 0;
    const auto b = band(EqBand::Bell, 1000, 3.0f, 2.0f);
    EXPECT_NEAR(engineGainDb(e, {b}, g, 1000), 0.0, 0.2);
    EXPECT_LT(engineGainDb(e, {b}, g, 100), -15.0);
}

TEST(EqEngine, AverageGainForAutoGain) {
    std::vector<EqBand> bands(EqEngine::kBands);
    EXPECT_NEAR(EqEngine::averageGainDb(bands.data(), EqEngine::kBands, 1.0f), 0.0f, 1e-4f);
    bands[0] = band(EqBand::HighShelf, 20, 6.0f);  // everything up 6 dB
    EXPECT_NEAR(EqEngine::averageGainDb(bands.data(), EqEngine::kBands, 1.0f), 6.0f, 0.3f);
}

TEST(EqAnalysis, AnalyzerReadsASine) {
    SpectrumAnalyzer a(8192);
    a.setFftSize(4096);
    std::vector<float> x(8192);
    for (int i = 0; i < 8192; ++i) x[static_cast<std::size_t>(i)] = 0.5f * static_cast<float>(std::sin(2.0 * kPi * 1000.0 * i / kSr));
    a.push(x.data(), 8192);
    float out[SpectrumAnalyzer::kBins];
    a.compute(kSr, out);
    const int k = static_cast<int>(std::round(SpectrumAnalyzer::binForHz(1000.0f)));
    EXPECT_NEAR(out[k], -6.0f, 1.0f);
    EXPECT_LT(out[static_cast<int>(SpectrumAnalyzer::binForHz(100.0f))], -60.0f);
}

TEST(EqAnalysis, MatchFindsTheDifference) {
    // Reference = flat + a 6 dB bump at 2 kHz; current = flat.
    const auto bump = band(EqBand::Bell, 2000, 6.0f, 1.5f);
    float ref[SpectrumAnalyzer::kBins], cur[SpectrumAnalyzer::kBins];
    for (int i = 0; i < SpectrumAnalyzer::kBins; ++i) {
        cur[i] = -20.0f;
        ref[i] = -20.0f + static_cast<float>(db(EqDesign::idealMagnitude(bump, 6.0f, SpectrumAnalyzer::binHz(i))));
    }
    const auto bands = EqMatch::fit(ref, cur, 8, 1.0f);
    ASSERT_FALSE(bands.empty());
    EXPECT_EQ(bands[0].type, EqBand::Bell);
    EXPECT_NEAR(std::log2(bands[0].freq / 2000.0f), 0.0f, 0.15f);
    EXPECT_NEAR(bands[0].gain, 5.5f, 1.2f);  // centred on the 100 Hz .. 10 kHz average
}

TEST(EqAnalysis, CollisionNeedsBothLoud) {
    float a[SpectrumAnalyzer::kBins], b[SpectrumAnalyzer::kBins], c[SpectrumAnalyzer::kBins];
    for (int i = 0; i < SpectrumAnalyzer::kBins; ++i) {
        a[i] = i < 128 ? -10.0f : -90.0f;
        b[i] = -12.0f;
    }
    Collision::compute(a, b, c);
    EXPECT_GT(c[50], 0.8f);
    EXPECT_LT(c[200], 0.01f);
}

TEST(EqEngine, CpuCost) {
    EqEngine e;
    e.prepare(kSr, 1);
    std::vector<EqBand> bands(EqEngine::kBands);
    for (int i = 0; i < 8; ++i) bands[static_cast<std::size_t>(i)] = band(EqBand::Bell, 100.0f * (i + 1), 3.0f, 1.0f);
    bands[8] = band(EqBand::LowCut, 30, 0, 0.7071f, 3);
    bands[9] = band(EqBand::HighShelf, 8000, 2.0f);
    bands[10] = band(EqBand::Bell, 3000, 0.0f);
    bands[10].dynamic = true;
    bands[10].dynRange = -6.0f;
    std::vector<float> l(512, 0.1f), r(512, 0.1f);
    for (int mode = 0; mode < 3; ++mode) {
        EqGlobal g;
        g.mode = mode;
        if (mode == 2) e.linear().publish(LinearPhase::build(bands.data(), EqEngine::kBands, 1.0f, kSr, 1));
        const int blocks = 1000;
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) e.process(l.data(), r.data(), nullptr, nullptr, 512, bands.data(), g);
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        const double pct = 100.0 * sec / (blocks * 512 / kSr);
        std::printf("PatQpro CPU (mode %d, 11 bands): %.2f %% of one core\n", mode, pct);
        EXPECT_LT(pct, 15.0);
    }
}
