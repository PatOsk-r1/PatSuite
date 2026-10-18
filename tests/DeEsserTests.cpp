// PatDeEsser engine tests: what each control does, measured on synthetic signals.
#include "dsp/deesser/DeEsserEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace pat::dsp;

namespace {

constexpr double kSr = 48000.0;
constexpr float kPi = 3.14159265f;

std::vector<float> sine(float hz, float amp, int n) {
    std::vector<float> x(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) x[static_cast<std::size_t>(i)] = amp * std::sin(2.0f * kPi * hz * static_cast<float>(i) / static_cast<float>(kSr));
    return x;
}

// Runs the engine in 512-sample blocks on a mono signal (both channels the same).
std::vector<float> run(DeEsserEngine& e, std::vector<float> x, const DeEsserSettings& s) {
    std::vector<float> r = x;
    for (std::size_t i = 0; i < x.size(); i += 512) {
        const int n = static_cast<int>(std::min<std::size_t>(512, x.size() - i));
        e.process(x.data() + i, r.data() + i, n, s);
    }
    return x;
}

// RMS level in dB of the second half (steady state).
double levelDb(const std::vector<float>& x) {
    double s = 0;
    for (std::size_t i = x.size() / 2; i < x.size(); ++i) s += static_cast<double>(x[i]) * x[i];
    return 10.0 * std::log10(s / static_cast<double>(x.size() / 2) + 1e-30);
}

// Level of one frequency component (single-bin DFT over the second half).
double toneDb(const std::vector<float>& x, float hz) {
    double re = 0, im = 0;
    const std::size_t start = x.size() / 2;
    for (std::size_t i = start; i < x.size(); ++i) {
        const double ph = 2.0 * 3.14159265358979 * hz * static_cast<double>(i) / kSr;
        re += x[i] * std::cos(ph);
        im += x[i] * std::sin(ph);
    }
    const double amp = 2.0 * std::sqrt(re * re + im * im) / static_cast<double>(x.size() - start);
    return 20.0 * std::log10(amp + 1e-30);
}

DeEsserEngine prepared() {
    DeEsserEngine e;
    e.prepare(kSr);
    return e;
}

}  // namespace

TEST(DeEsser, BandSplitterIsUnityAtTheCentreAndQuietFarAway) {
    BandSplitter b;
    b.setup(48000.0f, 6500.0f, 1.17f);
    const auto measure = [&](float hz) {
        b.reset();
        auto x = sine(hz, 1.0f, 48000);
        for (auto& v : x) v = b.band(v);
        return levelDb(x) + 3.0103;  // RMS of a unit sine is -3 dB
    };
    EXPECT_NEAR(measure(6500.0f), 0.0, 0.3);
    EXPECT_LT(measure(500.0f), -18.0);
    EXPECT_LT(measure(100.0f), -30.0);
}

TEST(DeEsser, GainCurveFollowsThresholdAndRatio) {
    EXPECT_EQ(DeEsserEngine::gainReductionDb(-40.0f, -24.0f, 4.0f), 0.0f);           // below
    EXPECT_NEAR(DeEsserEngine::gainReductionDb(-4.0f, -24.0f, 4.0f), -15.0f, 1e-4);  // 20 dB over, 4:1
    EXPECT_NEAR(DeEsserEngine::gainReductionDb(-14.0f, -24.0f, 2.0f), -5.0f, 1e-4);  // 10 dB over, 2:1
    // Soft knee: continuous and between the two straight lines.
    const float atThreshold = DeEsserEngine::gainReductionDb(-24.0f, -24.0f, 4.0f);
    EXPECT_LT(atThreshold, 0.0f);
    EXPECT_GT(atThreshold, -1.5f);
}

TEST(DeEsser, SplitModeTamesSibilanceAndLeavesTheVoiceAlone) {
    DeEsserSettings s;
    s.thresholdDb = -30.0f;
    s.ratio = 4.0f;
    // A 6.5 kHz "s" at -10 dBFS peak plus a 500 Hz "voice".
    auto x = sine(6500.0f, 0.316f, 48000);
    const auto voice = sine(500.0f, 0.5f, 48000);
    for (std::size_t i = 0; i < x.size(); ++i) x[i] += voice[i];
    auto e = prepared();
    const auto y = run(e, x, s);
    const double sBefore = toneDb(x, 6500.0f), sAfter = toneDb(y, 6500.0f);
    EXPECT_LT(sAfter - sBefore, -10.0);                     // clearly reduced (about 15 dB at 4:1)
    EXPECT_NEAR(toneDb(y, 500.0f), toneDb(x, 500.0f), 1.0);  // the voice keeps its level
    EXPECT_LT(e.reductionDb(), -8.0f);
}

TEST(DeEsser, WideModeTurnsEverythingDownDuringTheS) {
    DeEsserSettings s;
    s.thresholdDb = -30.0f;
    s.ratio = 4.0f;
    s.split = false;
    auto x = sine(6500.0f, 0.316f, 48000);
    const auto voice = sine(500.0f, 0.5f, 48000);
    for (std::size_t i = 0; i < x.size(); ++i) x[i] += voice[i];
    auto e = prepared();
    const auto y = run(e, x, s);
    EXPECT_LT(toneDb(y, 500.0f) - toneDb(x, 500.0f), -8.0);  // the voice goes down too
}

TEST(DeEsser, NothingHappensBelowThreshold) {
    DeEsserSettings s;  // threshold -24 dB
    const auto x = sine(500.0f, 0.8f, 48000);  // loud, but not sibilant
    for (bool split : {true, false}) {
        s.split = split;
        auto e = prepared();
        const auto y = run(e, x, s);
        EXPECT_NEAR(levelDb(y), levelDb(x), 0.1) << split;
    }
}

TEST(DeEsser, SplitIsTransparentWhenNotReducing) {
    // Ratio 1: the split must give back the input exactly (no colouring of the voice).
    DeEsserSettings s;
    s.ratio = 1.0f;
    std::mt19937 rng(5);
    std::uniform_real_distribution<float> u(-0.9f, 0.9f);
    std::vector<float> x(48000);
    for (auto& v : x) v = u(rng);
    auto e = prepared();
    const auto y = run(e, x, s);
    for (std::size_t i = 0; i < x.size(); ++i) ASSERT_NEAR(y[i], x[i], 1e-5f);
}

TEST(DeEsser, ListenPlaysOnlyTheBand) {
    DeEsserSettings s;
    s.listen = true;
    auto x = sine(6500.0f, 0.3f, 48000);
    const auto low = sine(300.0f, 0.5f, 48000);
    for (std::size_t i = 0; i < x.size(); ++i) x[i] += low[i];
    auto e = prepared();
    const auto y = run(e, x, s);
    EXPECT_NEAR(toneDb(y, 6500.0f), toneDb(x, 6500.0f), 1.0);
    EXPECT_LT(toneDb(y, 300.0f), toneDb(x, 300.0f) - 20.0);
}

TEST(DeEsser, ZeroLatency) {
    DeEsserSettings s;
    s.ratio = 1.0f;
    std::vector<float> x(1024, 0.0f);
    x[0] = 1.0f;
    auto e = prepared();
    const auto y = run(e, x, s);
    EXPECT_NEAR(y[0], 1.0f, 1e-5f);
}

TEST(DeEsser, StaysFiniteWithExtremeSettings) {
    std::mt19937 rng(9);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    for (float freq : {2000.0f, 14000.0f})
        for (float width : {0.3f, 2.5f})
            for (float ratio : {1.0f, 20.0f}) {
                DeEsserSettings s;
                s.freqHz = freq;
                s.widthOct = width;
                s.ratio = ratio;
                s.thresholdDb = -60.0f;
                s.releaseMs = 5.0f;
                std::vector<float> x(24000);
                for (auto& v : x) v = u(rng);
                auto e = prepared();
                for (float v : run(e, x, s)) ASSERT_TRUE(std::isfinite(v));
            }
}

TEST(DeEsser, CpuCostIsTiny) {
    // 10 s of stereo noise at 48 kHz, split mode, in 256-sample blocks.
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    std::vector<float> l(480000), r(480000);
    for (std::size_t i = 0; i < l.size(); ++i) l[i] = r[i] = u(rng);
    auto e = prepared();
    DeEsserSettings s;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < l.size(); i += 256) e.process(l.data() + i, r.data() + i, 256, s);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[ bench ] PatDeEsser engine: %.3f%% of one core\n", secs / 10.0 * 100.0);
    EXPECT_LT(secs, 0.5);  // far below 5% of one core
}
