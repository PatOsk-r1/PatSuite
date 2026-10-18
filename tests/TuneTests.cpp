// PatTune tests: detection, shifting, scales and the whole engine.
#include "dsp/tune/PitchDetector.h"
#include "dsp/tune/PsolaShifter.h"
#include "dsp/tune/Scales.h"
#include "dsp/tune/TuneEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace pat;
using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;

// A vowel-like harmonic tone (falling harmonics).
std::vector<float> voice(double hz, int n, float amp = 0.4f) {
    std::vector<float> x(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        double v = 0;
        for (int h = 1; h <= 8; ++h) v += std::sin(2.0 * 3.14159265358979 * hz * h * i / kSr) / h;
        x[static_cast<std::size_t>(i)] = static_cast<float>(amp * v * 0.6);
    }
    return x;
}

// Median detected pitch of a signal (skipping the first `skip` samples).
float detectedPitch(const std::vector<float>& x, std::size_t skip = 4800) {
    PitchDetector d;
    d.prepare(kSr);
    std::vector<float> found;
    for (std::size_t i = 0; i < x.size(); ++i)
        if (d.push(x[i]) && i > skip && d.frequency() > 0) found.push_back(d.frequency());
    if (found.empty()) return 0;
    std::sort(found.begin(), found.end());
    return found[found.size() / 2];
}

std::vector<float> runEngine(TuneEngine& e, std::vector<float> x, const TuneSettings& s) {
    for (std::size_t i = 0; i < x.size(); i += 512) {
        const int m = static_cast<int>(std::min<std::size_t>(512, x.size() - i));
        e.process(x.data() + i, nullptr, m, s, nullptr, 0);
    }
    return x;
}

double cents(double a, double b) { return 1200.0 * std::log2(a / b); }
}  // namespace

TEST(Tune, DetectorFindsVoicePitches) {
    for (double hz : {82.4, 110.0, 220.0, 440.0, 880.0}) {
        EXPECT_NEAR(detectedPitch(voice(hz, 48000)), hz, hz * 0.003) << hz;
    }
}

TEST(Tune, DetectorReportsConfidence) {
    PitchDetector d;
    d.prepare(kSr);
    const auto tone = voice(220.0, 24000);
    for (float v : tone) d.push(v);
    EXPECT_GT(d.clarity(), 0.9f);
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-0.4f, 0.4f);
    for (int i = 0; i < 24000; ++i) d.push(u(rng));
    EXPECT_LT(d.clarity(), 0.6f);
    for (int i = 0; i < 24000; ++i) d.push(0.0f);
    EXPECT_EQ(d.frequency(), 0.0f);
}

TEST(Tune, PsolaShiftsPitchAndKeepsLevel) {
    PsolaShifter p;
    p.prepare(kSr);
    const auto in = voice(220.0, 72000);
    std::vector<float> out(in.size());
    const float ratio = std::exp2(1.0f / 12.0f);  // one semitone up
    for (std::size_t i = 0; i < in.size(); ++i) out[i] = p.process(in[i], static_cast<float>(kSr / 220.0), ratio, 1.0f);
    EXPECT_NEAR(detectedPitch(out, 9600), 220.0 * ratio, 2.5);
    double ein = 0, eout = 0;
    for (std::size_t i = 24000; i < in.size(); ++i) {
        ein += in[i] * in[i];
        eout += out[i] * out[i];
    }
    EXPECT_NEAR(10.0 * std::log10(eout / ein), 0.0, 3.0);
}

TEST(Tune, ScalesIncludeArabicQuarterTonesAndJapanese) {
    // Maqam Bayati on D: second degree is a half-flat E (D + 150 cents).
    int bayati = -1, insen = -1;
    for (int i = 0; i < numScales(); ++i) {
        if (std::string(scale(i).name) == "Maqam Bayati") bayati = i;
        if (std::string(scale(i).name) == "In Sen / Insen") insen = i;
    }
    ASSERT_GE(bayati, 0);
    ASSERT_GE(insen, 0);
    const auto t = buildTable(bayati, 2, 0, 440.0);
    const double d = 440.0 * std::exp2((2.0 - 9.0) / 12.0);  // D4
    EXPECT_NEAR(cents(t.nearest(d * std::exp2(140.0 / 1200.0)), d), 150.0, 1e-6);
    EXPECT_EQ(buildTable(insen, 0, 0, 440.0).size(), 5);
    // Custom scale from a quarter-tone mask.
    const auto c = buildTable(customScaleIndex(), 0, (1u << 0) | (1u << 7) | (1u << 14), 440.0);
    EXPECT_EQ(c.size(), 3);
    EXPECT_NEAR(c.degree(1), 350.0, 1e-9);
}

TEST(Tune, EngineSnapsToTheScale) {
    TuneSettings s;
    s.scale = 0;  // chromatic
    s.retuneMs = 0.0f;
    s.humanize = 0.0f;
    TuneEngine e;
    e.prepare(kSr);
    const auto out = runEngine(e, voice(225.0, 96000), s);  // 39 cents above A3
    EXPECT_NEAR(cents(detectedPitch(out, 24000), 220.0), 0.0, 8.0);
}

TEST(Tune, QuarterTonesAreKeptWithA24ToneScale) {
    const double quarter = 220.0 * std::exp2(50.0 / 1200.0);  // A3 + 50 cents
    const double sung = 220.0 * std::exp2(62.0 / 1200.0);     // a little above it
    TuneSettings s;
    s.retuneMs = 0.0f;
    s.humanize = 0.0f;
    s.scale = customScaleIndex();
    s.customMask = 0xFFFFFFu;  // all 24 quarter tones
    TuneEngine e;
    e.prepare(kSr);
    const auto out24 = runEngine(e, voice(sung, 96000), s);
    EXPECT_NEAR(cents(detectedPitch(out24, 24000), quarter), 0.0, 8.0);
    s.scale = 0;  // chromatic: goes to the semitone instead
    TuneEngine e12;
    e12.prepare(kSr);
    const auto out12 = runEngine(e12, voice(sung, 96000), s);
    EXPECT_NEAR(cents(detectedPitch(out12, 24000), 220.0 * std::exp2(100.0 / 1200.0)), 0.0, 8.0);
}

TEST(Tune, AutoOffIsAPureDelayOfTheReportedLatency) {
    TuneSettings s;
    s.autoOn = false;
    TuneEngine e;
    e.prepare(kSr);
    std::vector<float> x(9600, 0.0f);
    x[0] = 1.0f;
    const auto out = runEngine(e, x, s);
    const auto peak = static_cast<int>(std::max_element(out.begin(), out.end()) - out.begin());
    EXPECT_EQ(peak, e.latencySamples());
    EXPECT_NEAR(out[static_cast<std::size_t>(peak)], 1.0f, 1e-6f);
    std::printf("[ info  ] PatTune latency: %d samples (%.1f ms at 48 kHz)\n", e.latencySamples(), e.latencySamples() / 48.0);
}

TEST(Tune, StaysFiniteAndCheap) {
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> u(-0.8f, 0.8f);
    std::vector<float> l(480000), r(480000);
    for (std::size_t i = 0; i < l.size(); ++i) l[i] = r[i] = (i / 24000) % 2 ? u(rng) : 0.5f * std::sin(static_cast<float>(i) * 0.03f);
    TuneSettings s;
    s.formantSt = 5.0f;
    s.retuneMs = 0.0f;
    TuneEngine e;
    e.prepare(kSr);
    TunePoint pts[64];
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < l.size(); i += 256) e.process(l.data() + i, r.data() + i, 256, s, pts, 64);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[ bench ] PatTune engine: %.2f%% of one core\n", secs / 10.0 * 100.0);
    for (float v : l) ASSERT_TRUE(std::isfinite(v));
    EXPECT_LT(secs, 1.5);
}
