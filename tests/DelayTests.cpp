// PatPatPat engine tests: echo timing in every mode, the tap display, freeze, stability at
// 100 % feedback, ducking and dry passthrough.
#include "dsp/delay/DelayEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;

struct Out {
    std::vector<float> l, r;
};

// A click on the chosen inputs, run for `seconds` through a fully wet, clean delay.
Out impulse(DelaySettings s, double seconds, bool clickL = true, bool clickR = true, double bpm = 120.0) {
    DelayEngine e;
    e.prepare(kSr);
    const int n = static_cast<int>(seconds * kSr);
    Out o;
    o.l.assign(static_cast<std::size_t>(n), 0.0f);
    o.r = o.l;
    o.l[0] = clickL ? 1.0f : 0.0f;
    o.r[0] = clickR ? 1.0f : 0.0f;
    for (int i = 0; i < n; i += 256) e.process(o.l.data() + i, o.r.data() + i, std::min(256, n - i), s, bpm);
    return o;
}

DelaySettings wet() {
    DelaySettings s;
    s.mix = 1.0f;  // echoes only (dry gain 0)
    s.lowCutHz = 20.0f;
    s.highCutHz = 20000.0f;
    s.sync = false;
    s.link = false;
    return s;
}

int peakIndex(const std::vector<float>& x, int from, int to) {
    int best = from;
    for (int i = from; i < to && i < static_cast<int>(x.size()); ++i)
        if (std::abs(x[static_cast<std::size_t>(i)]) > std::abs(x[static_cast<std::size_t>(best)])) best = i;
    return best;
}
float energy(const std::vector<float>& x, int from, int to) {
    float e = 0;
    for (int i = from; i < to && i < static_cast<int>(x.size()); ++i) e += x[static_cast<std::size_t>(i)] * x[static_cast<std::size_t>(i)];
    return e;
}
}  // namespace

TEST(DelayEngine, SyncTimesFollowTempo) {
    DelaySettings s;
    s.sync = true;
    s.divisionL = 10;  // 1/4
    EXPECT_NEAR(DelayEngine::timeSeconds(s, false, 120.0), 0.5, 1e-9);
    s.divisionL = 8;  // 1/8 dotted
    EXPECT_NEAR(DelayEngine::timeSeconds(s, false, 100.0), 0.45, 1e-9);
    s.link = true;
    s.divisionR = 0;
    EXPECT_NEAR(DelayEngine::timeSeconds(s, true, 100.0), 0.45, 1e-9);  // linked: right = left
    s.divisionL = 15;  // 1 bar at 40 bpm would be 6 s: clamped
    EXPECT_LE(DelayEngine::timeSeconds(s, false, 40.0), DelayEngine::kMaxSeconds);
}

TEST(DelayEngine, StereoEchoesOnTime) {
    auto s = wet();
    s.mode = DelaySettings::Stereo;
    s.timeLMs = 100.0f;
    s.timeRMs = 150.0f;
    s.feedback = 0.5f;
    const auto o = impulse(s, 0.5);
    EXPECT_NEAR(peakIndex(o.l, 10, 7000), 4800, 3);
    EXPECT_NEAR(peakIndex(o.r, 10, 9000), 7200, 3);
    EXPECT_NEAR(peakIndex(o.l, 6000, 11000), 9600, 4);  // second repeat

    // Each repeat is `feedback` times the previous one. A soft, quiet burst (not a click) so the
    // loop filters and saturation do not change its level.
    DelayEngine e;
    e.prepare(kSr);
    std::vector<float> l(24000, 0.0f), r(24000, 0.0f);
    for (int i = 0; i < 96; ++i) l[static_cast<std::size_t>(i)] = 0.1f * std::sin(3.14159f * i / 96.0f) * std::sin(i * 0.2f);
    e.process(l.data(), r.data(), 24000, s, 120);
    const float e1 = energy(l, 4700, 5000), e2 = energy(l, 9500, 9800);
    EXPECT_NEAR(std::sqrt(e2 / e1), 0.5f, 0.03f);
}

TEST(DelayEngine, PingPongAlternatesSides) {
    auto s = wet();
    s.mode = DelaySettings::PingPong;
    s.timeLMs = 100.0f;
    s.timeRMs = 100.0f;
    s.feedback = 0.7f;
    const auto o = impulse(s, 0.5);
    // 1st echo left at 100 ms (nothing right), 2nd right at 200 ms (nothing left), 3rd left.
    EXPECT_NEAR(peakIndex(o.l, 10, 7000), 4800, 3);
    EXPECT_LT(energy(o.r, 0, 7000), 1e-6f);
    EXPECT_NEAR(peakIndex(o.r, 7000, 12000), 9600, 4);
    EXPECT_LT(energy(o.l, 7000, 12000), 1e-6f);
    EXPECT_NEAR(peakIndex(o.l, 12000, 17000), 14400, 5);
}

TEST(DelayEngine, CrossKeepsTheStereoInput) {
    auto s = wet();
    s.mode = DelaySettings::Cross;
    s.timeLMs = 100.0f;
    s.timeRMs = 100.0f;
    s.feedback = 0.5f;
    const auto o = impulse(s, 0.3, true, false);  // click only on the left
    EXPECT_NEAR(peakIndex(o.l, 10, 7000), 4800, 3);
    EXPECT_LT(energy(o.r, 0, 7000), 1e-6f);
    EXPECT_NEAR(peakIndex(o.r, 7000, 12000), 9600, 4);
}

TEST(DelayEngine, MonoIsTheSameOnBothSides) {
    auto s = wet();
    s.mode = DelaySettings::Mono;
    s.timeLMs = 80.0f;
    s.feedback = 0.4f;
    const auto o = impulse(s, 0.3);
    for (std::size_t i = 0; i < o.l.size(); ++i) ASSERT_FLOAT_EQ(o.l[i], o.r[i]);
    EXPECT_NEAR(peakIndex(o.l, 10, 6000), 3840, 3);
}

TEST(DelayEngine, TapDisplayMatchesTheModes) {
    DelaySettings s;
    s.sync = false;
    s.link = false;
    s.timeLMs = 100.0f;
    s.timeRMs = 150.0f;
    s.feedback = 0.5f;
    DelayTap t[32];
    s.mode = DelaySettings::PingPong;
    int n = DelayEngine::taps(s, 120, t, 32);
    ASSERT_GE(n, 3);
    EXPECT_NEAR(t[0].seconds, 0.10, 1e-9);
    EXPECT_GT(t[0].left, 0.9f);
    EXPECT_NEAR(t[1].seconds, 0.25, 1e-9);
    EXPECT_GT(t[1].right, 0.4f);
    EXPECT_EQ(t[1].left, 0.0f);
    s.mode = DelaySettings::Stereo;
    n = DelayEngine::taps(s, 120, t, 32);
    EXPECT_NEAR(t[0].seconds, 0.10, 1e-9);
    EXPECT_NEAR(t[1].seconds, 0.15, 1e-9);
    for (int i = 1; i < n; ++i) EXPECT_GE(t[i].seconds, t[i - 1].seconds);
}

TEST(DelayEngine, FullFeedbackStaysBounded) {
    auto s = wet();
    s.feedback = 1.0f;
    s.drive = 0.0f;
    s.timeLMs = 50.0f;
    s.timeRMs = 50.0f;
    DelayEngine e;
    e.prepare(kSr);
    std::vector<float> l(256), r(256);
    float peak = 0;
    for (int b = 0; b < 2000; ++b) {  // about 10 s of loud noise
        for (int i = 0; i < 256; ++i) l[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)] = (i * 7919 % 200) / 100.0f - 1.0f;
        e.process(l.data(), r.data(), 256, s, 120);
        for (int i = 0; i < 256; ++i) peak = std::max(peak, std::abs(l[static_cast<std::size_t>(i)]));
    }
    EXPECT_TRUE(std::isfinite(peak));
    EXPECT_LT(peak, 4.0f);
}

TEST(DelayEngine, FreezeHoldsTheEchoes) {
    auto s = wet();
    s.mode = DelaySettings::Stereo;
    s.timeLMs = 100.0f;
    s.timeRMs = 100.0f;
    s.feedback = 0.3f;
    DelayEngine e;
    e.prepare(kSr);
    std::vector<float> l(4800, 0.0f), r(4800, 0.0f);
    for (int i = 0; i < 2400; ++i) l[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)] = 0.5f * std::sin(i * 0.05f);
    e.process(l.data(), r.data(), 4800, s, 120);  // 100 ms of tone in the line
    s.freeze = true;
    float early = 0, late = 0;
    for (int b = 0; b < 100; ++b) {  // 10 s frozen, with new input that must be ignored
        std::fill(l.begin(), l.end(), 0.8f);
        std::fill(r.begin(), r.end(), 0.8f);
        // Fully wet: the dry input does not reach the output, only the frozen loop.
        e.process(l.data(), r.data(), 4800, s, 120);
        const float en = energy(l, 0, 4800);
        if (b == 1) early = en;
        if (b == 99) late = en;
    }
    EXPECT_GT(early, 1.0f);
    EXPECT_NEAR(late / early, 1.0f, 0.15f);
}

TEST(DelayEngine, DuckingLowersEchoesWhilePlaying) {
    auto s = wet();
    s.mode = DelaySettings::Mono;
    s.timeLMs = 50.0f;
    s.feedback = 0.6f;
    s.mix = 0.5f;
    const auto run = [&](float duck) {
        s.duck = duck;
        DelayEngine e;
        e.prepare(kSr);
        std::vector<float> l(48000), r(48000);
        for (int i = 0; i < 48000; ++i) l[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)] = 0.5f * std::sin(i * 0.03f);
        e.process(l.data(), r.data(), 48000, s, 120);
        return e.duckGain();
    };
    EXPECT_FLOAT_EQ(run(0.0f), 1.0f);
    EXPECT_LT(run(1.0f), 0.2f);
}

TEST(DelayEngine, MixZeroIsDry) {
    DelaySettings s;
    s.mix = 0.0f;
    DelayEngine e;
    e.prepare(kSr);
    std::vector<float> l(1000), r(1000);
    for (int i = 0; i < 1000; ++i) l[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)] = std::sin(i * 0.1f);
    const auto ref = l;
    e.process(l.data(), r.data(), 1000, s, 120);
    for (int i = 0; i < 1000; ++i) ASSERT_FLOAT_EQ(l[static_cast<std::size_t>(i)], ref[static_cast<std::size_t>(i)]);
}

TEST(DelayEngine, CpuCost) {
    DelaySettings s;
    s.character = DelaySettings::Tape;
    s.diffusion = 0.5f;
    s.modDepth = 0.3f;
    s.duck = 0.5f;
    DelayEngine e;
    e.prepare(kSr);
    std::vector<float> l(512, 0.3f), r(512, 0.2f);
    const int blocks = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) e.process(l.data(), r.data(), 512, s, 120);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / (blocks * 512 / kSr);
    std::printf("PatPatPat CPU: %.2f %% of one core\n", pct);
    EXPECT_LT(pct, 10.0);
}
