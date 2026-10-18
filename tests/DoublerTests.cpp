// PatDoubler engine tests.
#include "dsp/doubler/DoublerEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;

DoublerEngine prepared() {
    DoublerEngine e;
    e.prepare(kSr);
    return e;
}

void run(DoublerEngine& e, std::vector<float>& l, std::vector<float>& r, const DoublerSettings& s) {
    for (std::size_t i = 0; i < l.size(); i += 512) {
        const int n = static_cast<int>(std::min<std::size_t>(512, l.size() - i));
        e.process(l.data() + i, r.data() + i, n, s, nullptr, 0);
    }
}

// One clean copy: no detune, no drift, centred.
DoublerSettings oneCopy() {
    DoublerSettings s;
    s.copies = 1;
    s.thickness = 1.0f;
    s.variation = 0.0f;
    s.detuneCents = {0, 0, 0, 0};
    s.pan = {0, 0, 0, 0};
    return s;
}
}  // namespace

TEST(Doubler, CopyArrivesAfterItsDelay) {
    auto s = oneCopy();
    s.delayMs[0] = 20.0f;
    std::vector<float> l(9600, 0.0f), r(9600, 0.0f);
    l[0] = r[0] = 1.0f;
    auto e = prepared();
    run(e, l, r, s);
    EXPECT_NEAR(l[0], 1.0f, 1e-6f);  // dry: zero latency
    std::size_t peak = 100;
    for (std::size_t i = 100; i < l.size(); ++i)
        if (std::abs(l[i]) > std::abs(l[peak])) peak = i;
    // 20 ms delay + 16 ms fixed (drift headroom + half the shifter window) = 36 ms = 1728 samples.
    EXPECT_NEAR(static_cast<double>(peak), 1729.0, 4.0);
    EXPECT_GT(std::abs(l[peak]), 0.3f);
}

TEST(Doubler, DetuneShiftsThePitch) {
    auto s = oneCopy();
    s.detuneCents[0] = 50.0f;
    s.dryGain = 0.0f;  // listen to the copy alone
    std::vector<float> l(96000), r;
    for (std::size_t i = 0; i < l.size(); ++i) l[i] = static_cast<float>(std::sin(2.0 * 3.14159265358979 * 440.0 * static_cast<double>(i) / kSr));
    r = l;
    auto e = prepared();
    run(e, l, r, s);
    int crossings = 0;
    for (std::size_t i = 48001; i < l.size(); ++i)
        if (l[i - 1] < 0 && l[i] >= 0) ++crossings;  // one second of steady state
    EXPECT_NEAR(crossings, 440.0 * std::exp2(50.0 / 1200.0), 3.0);  // 452.9 Hz
}

TEST(Doubler, WidthZeroIsMono) {
    DoublerSettings s;
    s.width = 0.0f;
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    std::vector<float> l(48000), r;
    for (auto& v : l) v = u(rng);
    r = l;
    auto e = prepared();
    run(e, l, r, s);
    for (std::size_t i = 0; i < l.size(); ++i) ASSERT_NEAR(l[i], r[i], 1e-5f);
}

TEST(Doubler, WideCopiesMakeTheSidesDifferent) {
    DoublerSettings s;  // default: width 0.8, copies panned left/right
    std::mt19937 rng(4);
    std::uniform_real_distribution<float> u(-0.5f, 0.5f);
    std::vector<float> l(48000), r;
    for (auto& v : l) v = u(rng);
    r = l;
    auto e = prepared();
    run(e, l, r, s);
    double side = 0, mid = 0;
    for (std::size_t i = 24000; i < l.size(); ++i) {
        side += (l[i] - r[i]) * (l[i] - r[i]);
        mid += (l[i] + r[i]) * (l[i] + r[i]);
    }
    EXPECT_GT(side / mid, 0.02);
}

TEST(Doubler, ThicknessZeroIsDryOnly) {
    DoublerSettings s;
    s.thickness = 0.0f;
    std::vector<float> l(4800), r;
    for (std::size_t i = 0; i < l.size(); ++i) l[i] = std::sin(static_cast<float>(i) * 0.05f);
    r = l;
    const auto ref = l;
    auto e = prepared();
    run(e, l, r, s);
    for (std::size_t i = 0; i < l.size(); ++i) ASSERT_NEAR(l[i], ref[i], 1e-5f);
}

TEST(Doubler, StaysFiniteAndCheap) {
    DoublerSettings s;
    s.variation = 1.0f;
    s.spread = 1.5f;
    s.thickness = 1.0f;
    s.detuneCents = {-50, 50, -50, 50};
    s.delayMs = {1, 80, 1, 80};
    std::mt19937 rng(5);
    std::uniform_real_distribution<float> u(-1.0f, 1.0f);
    std::vector<float> l(480000), r(480000);
    for (std::size_t i = 0; i < l.size(); ++i) l[i] = r[i] = u(rng);
    auto e = prepared();
    StereoSample scope[128];
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < l.size(); i += 256) e.process(l.data() + i, r.data() + i, 256, s, scope, 128);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("[ bench ] PatDoubler engine: %.3f%% of one core\n", secs / 10.0 * 100.0);
    for (float v : l) ASSERT_TRUE(std::isfinite(v));
    EXPECT_LT(secs, 0.5);
}
