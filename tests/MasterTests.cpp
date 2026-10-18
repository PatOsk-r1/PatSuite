// PatMaster tests: the limiter never passes the ceiling, leaves quiet audio untouched (only
// delayed by its reported latency), and the loudness meter reads like BS.1770 says.
#include "dsp/master/LimiterEngine.h"
#include "dsp/master/Loudness.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;
constexpr double kPi = 3.14159265358979323846;

std::vector<float> sine(int n, double hz, float amp) {
    std::vector<float> x(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) x[static_cast<std::size_t>(i)] = amp * static_cast<float>(std::sin(2.0 * kPi * hz * i / kSr));
    return x;
}
}  // namespace

TEST(Limiter, NeverPassesTheCeiling) {
    for (bool truePeak : {false, true}) {
        LimiterEngine e;
        e.prepare(kSr);
        LimiterSettings s;
        s.gainDb = 12.0f;
        s.ceilingDb = -1.0f;
        s.truePeak = truePeak;
        std::mt19937 rng(7);
        std::uniform_real_distribution<float> d(-1.0f, 1.0f);
        const int n = 48000;
        std::vector<float> l(static_cast<std::size_t>(n)), r(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            const float drum = (i % 12000) < 400 ? 1.0f : 0.2f;
            l[static_cast<std::size_t>(i)] = d(rng) * drum;
            r[static_cast<std::size_t>(i)] = d(rng) * drum;
        }
        for (int i = 0; i < n; i += 512) e.process(l.data() + i, r.data() + i, std::min(512, n - i), s);
        const float ceil = std::pow(10.0f, -1.0f / 20.0f);
        float pk = 0;
        for (int i = 0; i < n; ++i) pk = std::max(pk, std::max(std::abs(l[static_cast<std::size_t>(i)]), std::abs(r[static_cast<std::size_t>(i)])));
        EXPECT_LE(pk, ceil + 1e-6f);
        if (truePeak) {  // also between samples (measured with the meter's 4x estimate)
            LoudnessMeter m;
            m.prepare(kSr);
            m.process(l.data(), r.data(), n);
            EXPECT_LE(m.truePeakDb(), -1.0f + 0.3f);
        }
    }
}

TEST(Limiter, QuietAudioIsOnlyDelayed) {
    LimiterEngine e;
    e.prepare(kSr);
    LimiterSettings s;
    const int n = 4800;
    auto l = sine(n, 440.0, 0.1f), r = l;
    const auto ref = l;
    e.process(l.data(), r.data(), n, s);
    const int lat = e.latency();
    EXPECT_EQ(lat, 72 + 3);
    for (int i = lat; i < n; ++i) ASSERT_NEAR(l[static_cast<std::size_t>(i)], ref[static_cast<std::size_t>(i - lat)], 1e-5f) << i;
}

TEST(Limiter, LoudSineIsHeldAtTheCeilingWithoutDistortionSpikes) {
    LimiterEngine e;
    e.prepare(kSr);
    LimiterSettings s;
    s.ceilingDb = -3.0f;
    const int n = 48000;
    auto l = sine(n, 100.0, 1.0f), r = l;
    e.process(l.data(), r.data(), n, s);
    float pk = 0;
    for (int i = n / 2; i < n; ++i) pk = std::max(pk, std::abs(l[static_cast<std::size_t>(i)]));
    EXPECT_NEAR(20.0f * std::log10(pk), -3.0f, 0.3f);
    EXPECT_LT(e.gainReductionDb(), -2.5f);
}

TEST(Loudness, SineReadsItsLevel) {
    LoudnessMeter m;
    m.prepare(kSr);
    // 1 kHz on both channels at -23 dBFS reads -23 LUFS (BS.1770 reference).
    const auto x = sine(static_cast<int>(kSr * 5), 1000.0, std::pow(10.0f, -23.0f / 20.0f));
    m.process(x.data(), x.data(), static_cast<int>(x.size()));
    EXPECT_NEAR(m.momentary(), -23.0f, 0.2f);
    EXPECT_NEAR(m.shortTerm(), -23.0f, 0.2f);
    EXPECT_NEAR(m.integrated(), -23.0f, 0.2f);
    EXPECT_NEAR(m.truePeakDb(), -23.0f, 0.1f);
    EXPECT_LT(m.range(), 0.5f);
}

TEST(Loudness, SilenceIsGatedOut) {
    LoudnessMeter m;
    m.prepare(kSr);
    const auto x = sine(static_cast<int>(kSr * 4), 1000.0, std::pow(10.0f, -18.0f / 20.0f));
    std::vector<float> silence(static_cast<std::size_t>(kSr * 4), 0.0f);
    m.process(x.data(), x.data(), static_cast<int>(x.size()));
    m.process(silence.data(), silence.data(), static_cast<int>(silence.size()));
    EXPECT_NEAR(m.integrated(), -18.0f, 0.3f);  // the silent part does not drag it down
    EXPECT_LE(m.momentary(), LoudnessMeter::kSilence + 0.1f);
}

TEST(Loudness, TruePeakFindsPeaksBetweenSamples) {
    // A sine at a quarter of the sample rate, phase 45 degrees: samples are at 0.707 of the peak.
    LoudnessMeter m;
    m.prepare(kSr);
    std::vector<float> x(4800);
    for (int i = 0; i < 4800; ++i) x[static_cast<std::size_t>(i)] = static_cast<float>(0.5 * std::sin(kPi / 2.0 * i + kPi / 4.0));
    m.process(x.data(), x.data(), 4800);
    EXPECT_NEAR(m.truePeakDb(), 20.0f * std::log10(0.5f), 0.5f);  // not the -3 dB lower sample peak
}

TEST(Limiter, CpuCost) {
    LimiterEngine e;
    e.prepare(kSr);
    LoudnessMeter m;
    m.prepare(kSr);
    LimiterSettings s;
    s.gainDb = 6.0f;
    auto l = sine(512, 300.0, 0.9f), r = l;
    const int blocks = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) {
        e.process(l.data(), r.data(), 512, s);
        m.process(l.data(), r.data(), 512);
    }
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / (blocks * 512 / kSr);
    std::printf("PatMaster CPU: %.2f %% of one core\n", pct);
    EXPECT_LT(pct, 8.0);
}
