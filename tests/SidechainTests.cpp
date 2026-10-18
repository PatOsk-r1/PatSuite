// PatSidechain engine tests.
#include "dsp/sidechain/SidechainEngine.h"

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

struct Run {
    std::vector<float> l, r;
    std::vector<float> gainTrace;  // graph gain points
};

// Main = constant 1.0 (so output == gain), optional sidechain signal.
Run run(SidechainEngine& e, int n, const SidechainSettings& s, const std::vector<float>* sc = nullptr, Transport t = {}) {
    Run out;
    out.l.assign(static_cast<std::size_t>(n), 1.0f);
    out.r = out.l;
    GraphPoint pts[64];
    for (int i = 0; i < n; i += 512) {
        const int m = std::min(512, n - i);
        const float* scp = sc ? sc->data() + i : nullptr;
        const int k = e.process(out.l.data() + i, out.r.data() + i, scp, scp, m, s, t, pts, 64);
        for (int p = 0; p < k; ++p) out.gainTrace.push_back(pts[p].gain);
        if (t.playing) t.ppq += m * t.bpm / 60.0 / kSr;
    }
    return out;
}

std::vector<float> kicks(int n, float everySeconds) {
    std::vector<float> x(static_cast<std::size_t>(n), 0.0f);
    const int period = static_cast<int>(everySeconds * kSr);
    for (int start = 0; start < n; start += period)
        for (int i = 0; i < 6000 && start + i < n; ++i)
            x[static_cast<std::size_t>(start + i)] = 0.8f * std::sin(2.0f * kPi * 55.0f * i / static_cast<float>(kSr)) * std::exp(-i / 1500.0f);
    return x;
}

SidechainEngine prepared() {
    SidechainEngine e;
    e.prepare(kSr);
    return e;
}
}  // namespace

TEST(Sidechain, TempoModeDucksOnEveryQuarterNote) {
    SidechainSettings s;
    s.trigger = SidechainSettings::Tempo;
    s.pump = 1.0f;
    auto e = prepared();
    const auto out = run(e, 96000, s);  // 2 s at 120 bpm = 4 quarter notes
    // Dips at 0, 0.5, 1.0, 1.5 s; full level in between (release 180 ms).
    for (double t : {0.0, 0.5, 1.0, 1.5}) EXPECT_LT(out.l[static_cast<std::size_t>((t + 0.003) * kSr)], 0.3f) << t;
    for (double t : {0.35, 0.85, 1.35}) EXPECT_GT(out.l[static_cast<std::size_t>(t * kSr)], 0.95f) << t;
    EXPECT_EQ(e.triggers(), 3);  // beats 2, 3, 4 (the first one starts at phase 0)
}

TEST(Sidechain, RateAndSongPositionAreRespected) {
    SidechainSettings s;
    s.trigger = SidechainSettings::Tempo;
    s.pump = 1.0f;
    s.rate = 1;  // 1/8 at 120 bpm = every 0.25 s
    Transport t;
    t.playing = true;
    t.ppq = 0.25;  // the song starts a quarter of a beat before an eighth note boundary
    auto e = prepared();
    const auto out = run(e, 48000, s, nullptr, t);
    EXPECT_LT(out.l[static_cast<std::size_t>(0.125 * kSr + 144)], 0.3f);  // first eighth after 0.125 s
    EXPECT_LT(out.l[static_cast<std::size_t>(0.375 * kSr + 144)], 0.3f);
    EXPECT_GT(out.l[static_cast<std::size_t>(0.33 * kSr)], 0.5f);
}

TEST(Sidechain, SidechainModeFollowsTheKick) {
    SidechainSettings s;
    s.trigger = SidechainSettings::Sidechain;
    s.pump = 1.0f;
    const auto sc = kicks(96000, 0.5f);
    auto e = prepared();
    const auto out = run(e, 96000, s, &sc);
    EXPECT_LT(out.l[static_cast<std::size_t>(0.02 * kSr)], 0.3f);   // ducked right after a kick
    EXPECT_GT(out.l[static_cast<std::size_t>(0.45 * kSr)], 0.9f);   // recovered before the next
    EXPECT_GE(e.triggers(), 3);
    // Pump 0: nothing moves.
    s.pump = 0.0f;
    auto e2 = prepared();
    const auto flat = run(e2, 48000, s, &sc);
    for (float v : flat.l) ASSERT_NEAR(v, 1.0f, 1e-4f);
}

TEST(Sidechain, FilterIgnoresHighSidechainContent) {
    SidechainSettings s;
    s.trigger = SidechainSettings::Sidechain;
    s.pump = 1.0f;
    std::vector<float> hats(48000);
    for (std::size_t i = 0; i < hats.size(); ++i) hats[i] = 0.8f * std::sin(2.0f * kPi * 6000.0f * static_cast<float>(i) / static_cast<float>(kSr));
    auto e = prepared();
    const auto out = run(e, 48000, s, &hats);
    EXPECT_GT(*std::min_element(out.l.begin() + 4800, out.l.end()), 0.8f);  // hi-hats do not pump the mix
}

TEST(Sidechain, AutoUsesSidechainOnlyWhenItSounds) {
    SidechainSettings s;  // Auto
    auto e = prepared();
    run(e, 9600, s);
    EXPECT_FALSE(e.usingSidechain());
    const auto sc = kicks(48000, 0.5f);
    run(e, 48000, s, &sc);
    EXPECT_TRUE(e.usingSidechain());
}

TEST(Sidechain, ListenPlaysTheFilteredSidechain) {
    SidechainSettings s;
    s.listenSidechain = true;
    const auto sc = kicks(48000, 0.5f);
    auto e = prepared();
    const auto out = run(e, 48000, s, &sc);
    float peak = 0;
    for (float v : out.l) peak = std::max(peak, std::abs(v));
    EXPECT_GT(peak, 0.3f);
    EXPECT_LT(peak, 1.2f);
}

TEST(Sidechain, CompressorCurve) {
    EXPECT_EQ(SidechainEngine::compressorGainDb(-40.0f, -24.0f, 8.0f, 6.0f), 0.0f);
    EXPECT_NEAR(SidechainEngine::compressorGainDb(-4.0f, -24.0f, 8.0f, 6.0f), -17.5f, 1e-3);
    EXPECT_NEAR(SidechainEngine::compressorGainDb(-14.0f, -24.0f, 2.0f, 0.0f), -5.0f, 1e-3);  // hard knee
}

TEST(Sidechain, StaysFiniteAndCheap) {
    std::mt19937 rng(2);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    std::vector<float> l(480000), r(480000), sc(480000);
    for (std::size_t i = 0; i < l.size(); ++i) l[i] = r[i] = sc[i] = u(rng);
    SidechainSettings s;
    s.attackMs = 0.1f;
    s.releaseMs = 10.0f;
    s.ratio = 20.0f;
    s.thresholdDb = -60.0f;
    auto e = prepared();
    GraphPoint pts[8];
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < l.size(); i += 256) e.process(l.data() + i, r.data() + i, sc.data() + i, sc.data() + i, 256, s, {}, pts, 8);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[ bench ] PatSidechain engine: %.3f%% of one core\n", secs / 10.0 * 100.0);
    for (float v : l) ASSERT_TRUE(std::isfinite(v));
    EXPECT_LT(secs, 0.5);
}

TEST(Sidechain, HoldsTheDuckWithoutFollowingEachKickCycle) {
    // A long 50 Hz kick: once ducked, the gain must stay steady instead of wobbling with every
    // cycle of the kick (that wobble is what distorts the bass).
    SidechainEngine e;
    e.prepare(kSr);
    SidechainSettings s;
    s.trigger = SidechainSettings::Sidechain;
    s.pump = 1.0f;
    const int n = static_cast<int>(0.3 * kSr);
    std::vector<float> sc(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) sc[static_cast<std::size_t>(i)] = 0.8f * std::sin(2.0f * kPi * 50.0f * static_cast<float>(i) / static_cast<float>(kSr));
    const auto out = run(e, n, s, &sc);
    float lo = 1.0f, hi = 0.0f;
    for (int i = static_cast<int>(0.1 * kSr); i < static_cast<int>(0.25 * kSr); ++i) {
        lo = std::min(lo, out.l[static_cast<std::size_t>(i)]);
        hi = std::max(hi, out.l[static_cast<std::size_t>(i)]);
    }
    EXPECT_LT(hi, 0.3f);         // ducked
    EXPECT_LT(hi - lo, 0.02f);   // and steady
}
