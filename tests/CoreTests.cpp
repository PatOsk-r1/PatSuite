// Tests for the shared, JUCE-free building blocks.
#include "common/core/PerfTier.h"
#include "common/core/SpscFifo.h"
#include "common/core/TuningTable.h"

#include <gtest/gtest.h>

#include <cmath>
#include <thread>
#include <vector>

using namespace pat;

// ------------------------------------------------------------------------------------- SpscFifo

TEST(SpscFifo, PushPopKeepsOrderAndDropsWhenFull) {
    SpscFifo<int> f(8);
    ASSERT_EQ(f.capacity(), 8u);
    std::vector<int> in(10);
    for (int i = 0; i < 10; ++i) in[static_cast<std::size_t>(i)] = i;
    EXPECT_EQ(f.push(in.data(), 10), 8u);  // full: the last two are dropped, never blocks
    std::vector<int> out(10, -1);
    EXPECT_EQ(f.pop(out.data(), 10), 8u);
    for (int i = 0; i < 8; ++i) EXPECT_EQ(out[static_cast<std::size_t>(i)], i);
    EXPECT_EQ(f.available(), 0u);
}

TEST(SpscFifo, SkipToNewestKeepsOnlyRecentItems) {
    SpscFifo<int> f(16);
    std::vector<int> in(12);
    for (int i = 0; i < 12; ++i) in[static_cast<std::size_t>(i)] = i;
    f.push(in.data(), 12);
    f.skipToNewest(4);
    std::vector<int> out(4);
    ASSERT_EQ(f.pop(out.data(), 4), 4u);
    EXPECT_EQ(out[0], 8);
    EXPECT_EQ(out[3], 11);
}

TEST(SpscFifo, ConcurrentProducerAndConsumerSeeAnOrderedStream) {
    SpscFifo<int> f(1024);
    constexpr int total = 200000;
    std::thread producer([&] {
        int next = 0;
        while (next < total) {
            int chunk[64];
            int n = 0;
            for (; n < 64 && next + n < total; ++n) chunk[n] = next + n;
            next += static_cast<int>(f.push(chunk, static_cast<std::size_t>(n)));
        }
    });
    int expected = 0;
    while (expected < total) {
        int buf[100];
        const auto n = f.pop(buf, 100);
        for (std::size_t i = 0; i < n; ++i) ASSERT_EQ(buf[i], expected++);
    }
    producer.join();
}

// ------------------------------------------------------------------------------------- PerfTier

TEST(PerfTier, RulePicksTheExpectedLevel) {
    EXPECT_EQ(chooseTier({2, 16384, 4096}), PerfTier::Low);    // dual core
    EXPECT_EQ(chooseTier({4, 4096, 2048}), PerfTier::Low);     // 4 GB RAM
    EXPECT_EQ(chooseTier({8, 16384, 0}), PerfTier::Low);       // integrated graphics
    EXPECT_EQ(chooseTier({4, 8100, 4096}), PerfTier::Medium);  // this project's PC: i3-10100F, 8 GB, GTX 1050 Ti
    EXPECT_EQ(chooseTier({8, 16300, 8192}), PerfTier::High);   // 16 GB reports a little under 16384
    EXPECT_EQ(chooseTier({6, 32768, 8192}), PerfTier::Medium);
}

TEST(PerfTier, ProfilesScaleUp) {
    const auto lo = profileFor(PerfTier::Low), mid = profileFor(PerfTier::Medium), hi = profileFor(PerfTier::High);
    EXPECT_LT(lo.fps, mid.fps);
    EXPECT_LT(mid.fps, hi.fps);
    EXPECT_LE(lo.fftSize, 1024);
    EXPECT_EQ(mid.fftSize, 2048);
    EXPECT_EQ(hi.fftSize, 4096);
    EXPECT_TRUE(lo.repaintOnlyOnChange);
    EXPECT_TRUE(hi.glow);
    EXPECT_FALSE(lo.glow);
}

// ------------------------------------------------------------------------------------- TuningTable

TEST(TuningTable, TwelveToneSnapsToSemitones) {
    const auto t = TuningTable::equal(12, 440.0);  // reference A4
    EXPECT_NEAR(t.nearest(445.0), 440.0, 1e-9);
    EXPECT_NEAR(t.nearest(460.0), 440.0 * std::exp2(1.0 / 12.0), 1e-9);  // A#4
    EXPECT_NEAR(t.nearest(220.5), 220.0, 1e-9);                             // other octaves
    EXPECT_NEAR(t.centsToNearest(445.0), -19.56, 0.05);
}

TEST(TuningTable, QuarterTonesAreReachable) {
    const auto t = TuningTable::equal(24, 440.0);
    const double quarterUp = 440.0 * std::exp2(0.5 / 12.0);  // 50 cents above A4
    EXPECT_NEAR(t.nearest(quarterUp * 1.002), quarterUp, 1e-6);
    // 12-TET would push the same note to a semitone instead.
    EXPECT_NEAR(TuningTable::equal(12, 440.0).nearest(quarterUp * 1.002), 440.0 * std::exp2(1.0 / 12.0), 1e-6);
}

TEST(TuningTable, ArbitraryDegreesAndBoundaries) {
    // A maqam-like set with a neutral third at 350 cents (degrees given out of order).
    const TuningTable t({0, 150, 350, 500, 700, 850, 1000}, 261.6255653);
    const double c4 = 261.6255653;
    EXPECT_NEAR(TuningTable::hzToCents(t.nearest(c4 * std::exp2(340.0 / 1200.0)), c4), 350.0, 1e-6);
    // Just below the next octave's root: snaps up to 1200, not down to 1000.
    EXPECT_NEAR(TuningTable::hzToCents(t.nearest(c4 * std::exp2(1180.0 / 1200.0)), c4), 1200.0, 1e-6);
    // Below the reference (negative cents) works too.
    EXPECT_NEAR(TuningTable::hzToCents(t.nearest(c4 * std::exp2(-160.0 / 1200.0)), c4), -200.0, 1e-6);
}

TEST(TuningTable, NonOctavePeriod) {
    // Bohlen-Pierce style: 13 equal steps of a 1902-cent tritave.
    std::vector<double> d;
    for (int i = 0; i < 13; ++i) d.push_back(1901.955 * i / 13.0);
    const TuningTable t(d, 100.0, 1901.955);
    const double step = 1901.955 / 13.0;
    EXPECT_NEAR(TuningTable::hzToCents(t.nearest(TuningTable::centsToHz(1901.955 + step * 2.2, 100.0)), 100.0), 1901.955 + step * 2, 1e-6);
}
