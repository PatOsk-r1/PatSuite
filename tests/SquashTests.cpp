// PatSquash engine tests: the band split adds back to the original, upward and downward
// compression do what the curve says, silence is not pumped up, depth blends.
#include "dsp/squash/SquashEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;

// Steady-state output level (dB) of a sine of the given level through the engine.
float throughDb(SquashEngine& e, const SquashSettings& s, double hz, float levelDb) {
    const float amp = std::pow(10.0f, levelDb / 20.0f);
    const int n = static_cast<int>(kSr * 1.5);
    std::vector<float> l(static_cast<std::size_t>(n)), r;
    for (int i = 0; i < n; ++i) l[static_cast<std::size_t>(i)] = amp * static_cast<float>(std::sin(2.0 * 3.14159265358979 * hz * i / kSr));
    r = l;
    for (int i = 0; i < n; i += 512) e.process(l.data() + i, r.data() + i, std::min(512, n - i), s);
    float pk = 0;
    for (int i = n - static_cast<int>(kSr * 0.3); i < n; ++i) pk = std::max(pk, std::abs(l[static_cast<std::size_t>(i)]));
    return 20.0f * std::log10(pk / amp);
}
}  // namespace

TEST(Squash, BandsAddBackToTheOriginal) {
    SquashSettings s;
    s.depth = 0.0f;
    for (double hz : {50.0, 120.0, 700.0, 2500.0, 9000.0}) {
        SquashEngine e;
        e.prepare(kSr);
        EXPECT_NEAR(throughDb(e, s, hz, -12.0f), 0.0f, 0.1f) << hz;
    }
}

TEST(Squash, CurveDownAndUp) {
    SquashBand b;  // down -30 dB 6:1, up -40 dB 3:1
    EXPECT_NEAR(SquashEngine::gainDb(-6.0f, b, 1.0f, 1.0f), -20.0f, 1e-3f);
    EXPECT_NEAR(SquashEngine::gainDb(-35.0f, b, 1.0f, 1.0f), 0.0f, 1e-3f);
    EXPECT_NEAR(SquashEngine::gainDb(-50.0f, b, 1.0f, 1.0f), 6.667f, 1e-2f);
    EXPECT_NEAR(SquashEngine::gainDb(-50.0f, b, 0.5f, 1.0f), 3.333f, 1e-2f);
    EXPECT_EQ(SquashEngine::gainDb(-90.0f, b, 1.0f, 1.0f), 0.0f);  // below the floor: untouched
    EXPECT_LE(SquashEngine::gainDb(-68.0f, b, 1.0f, 1.0f), SquashEngine::kMaxUpDb);
}

TEST(Squash, LoudIsTurnedDownQuietIsTurnedUp) {
    SquashSettings s;
    SquashEngine e;
    e.prepare(kSr);
    // -20 dB in the mid band; a little of the tone also sits in the (untouched) high band.
    EXPECT_NEAR(throughDb(e, s, 1000.0, -6.0f), -19.0f, 2.0f);
    SquashEngine e2;
    e2.prepare(kSr);
    EXPECT_NEAR(throughDb(e2, s, 1000.0, -50.0f), 6.67f, 1.0f);
    SquashEngine e3;
    e3.prepare(kSr);
    EXPECT_NEAR(throughDb(e3, s, 1000.0, -95.0f), 0.0f, 0.3f);  // hiss stays hiss
}

TEST(Squash, DepthBlends) {
    SquashSettings s;
    s.depth = 0.5f;
    SquashEngine e;
    e.prepare(kSr);
    const float half = throughDb(e, s, 1000.0, -6.0f);
    EXPECT_LT(half, -3.0f);
    EXPECT_GT(half, -20.0f);
}

TEST(Squash, BandGainAndOutput) {
    SquashSettings s;
    s.upward = s.downward = 0.0f;  // no compression: only the gains
    s.band[2].gainDb = 6.0f;
    s.outputDb = -3.0f;
    SquashEngine e;
    e.prepare(kSr);
    EXPECT_NEAR(throughDb(e, s, 9000.0, -20.0f), 3.0f, 0.3f);
    SquashEngine e2;
    e2.prepare(kSr);
    EXPECT_NEAR(throughDb(e2, s, 60.0, -20.0f), -3.0f, 0.3f);
}

TEST(Squash, CpuCost) {
    SquashEngine e;
    e.prepare(kSr);
    SquashSettings s;
    std::vector<float> l(512, 0.3f), r(512, 0.2f);
    const int blocks = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) e.process(l.data(), r.data(), 512, s);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / (blocks * 512 / kSr);
    std::printf("PatSquash CPU: %.2f %% of one core\n", pct);
    EXPECT_LT(pct, 5.0);
}
