// Patan engine tests: curve editing and text form, triple buffer, pan laws and the movement
// locked to the song.
#include "common/core/TripleBuffer.h"
#include "dsp/pan/PanEngine.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace pat::dsp;

namespace {
constexpr double kSr = 48000.0;

CurveTable table(const PanCurve& c) {
    CurveTable t;
    c.render(t);
    return t;
}
}  // namespace

TEST(PanCurve, ShapesStayInRangeAndWrap) {
    for (int s = 0; s < PanCurve::NumShapes; ++s) {
        PanCurve c;
        c.setPoints(PanCurve::shape(s));
        for (int i = 0; i <= 200; ++i) {
            const float v = c.valueAt(i / 200.0f);
            EXPECT_GE(v, -1.0f) << PanCurve::shapeName(s);
            EXPECT_LE(v, 1.0f) << PanCurve::shapeName(s);
        }
        EXPECT_FLOAT_EQ(table(c).v[CurveTable::kSize], table(c).v[0]);
    }
}

TEST(PanCurve, SineGoesLeftToRightAndBack) {
    PanCurve c;  // default sweep
    EXPECT_NEAR(c.valueAt(0.0f), -1.0f, 1e-4f);
    EXPECT_NEAR(c.valueAt(0.25f), 0.0f, 1e-3f);
    EXPECT_NEAR(c.valueAt(0.5f), 1.0f, 1e-4f);
    EXPECT_NEAR(c.valueAt(0.75f), 0.0f, 1e-3f);
}

TEST(PanCurve, HoldMakesSteps) {
    PanCurve c;
    c.setPoints(PanCurve::shape(PanCurve::Square));
    EXPECT_FLOAT_EQ(c.valueAt(0.1f), -1.0f);
    EXPECT_FLOAT_EQ(c.valueAt(0.49f), -1.0f);
    EXPECT_FLOAT_EQ(c.valueAt(0.51f), 1.0f);
}

TEST(PanCurve, EditingKeepsOrderAndFirstPoint) {
    PanCurve c;
    c.setPoints(PanCurve::shape(PanCurve::Triangle));
    const int i = c.add(0.25f, 0.8f);
    ASSERT_EQ(i, 1);
    EXPECT_EQ(c.points().size(), 3u);
    c.move(i, 0.9f, 0.0f);  // cannot pass the next point (0.5)
    EXPECT_LT(c.points()[1].x, 0.5f);
    c.move(0, 0.3f, 0.5f);  // the first point stays at x = 0
    EXPECT_FLOAT_EQ(c.points()[0].x, 0.0f);
    EXPECT_FLOAT_EQ(c.points()[0].y, 0.5f);
    c.remove(0);  // not allowed
    EXPECT_EQ(c.points().size(), 3u);
    c.remove(1);
    EXPECT_EQ(c.points().size(), 2u);
}

TEST(PanCurve, BendChangesTheSegmentNotTheEnds) {
    PanCurve c;
    c.setPoints(PanCurve::shape(PanCurve::Triangle));
    const float straight = c.valueAt(0.25f);
    c.setBend(0, 0.7f);
    EXPECT_LT(c.valueAt(0.25f), straight - 0.2f);  // rises late
    EXPECT_NEAR(c.valueAt(0.0f), -1.0f, 1e-4f);
    EXPECT_NEAR(c.valueAt(0.4999f), 1.0f, 0.02f);
}

TEST(PanCurve, TextRoundTrip) {
    PanCurve a;
    a.setPoints(PanCurve::shape(PanCurve::Steps8));
    a.setBend(2, -0.4f);
    a.setType(3, CurvePoint::Smooth);
    PanCurve b;
    ASSERT_TRUE(b.fromString(a.toString()));
    ASSERT_EQ(a.points().size(), b.points().size());
    for (float x = 0; x < 1.0f; x += 0.01f) EXPECT_NEAR(a.valueAt(x), b.valueAt(x), 2e-3f);
    EXPECT_FALSE(b.fromString("not a curve"));
    EXPECT_EQ(b.points().size(), a.points().size());  // unchanged after bad text
}

TEST(TripleBuffer, ReaderGetsNewestCompleteValue) {
    pat::TripleBuffer<int> tb(0);
    EXPECT_EQ(tb.read(), 0);
    tb.writeSlot() = 1;
    tb.publish();
    tb.writeSlot() = 2;
    tb.publish();
    EXPECT_EQ(tb.read(), 2);
    EXPECT_EQ(tb.read(), 2);  // nothing new: same value
    tb.writeSlot() = 3;
    tb.publish();
    EXPECT_EQ(tb.read(), 3);
}

TEST(PanEngine, LawsGiveFullSidesAndRightCentre) {
    float gl, gr;
    PanEngine::panGains(0.0f, PanSettings::Balance, gl, gr);
    EXPECT_FLOAT_EQ(gl, 1.0f);
    EXPECT_FLOAT_EQ(gr, 1.0f);
    PanEngine::panGains(-1.0f, PanSettings::Balance, gl, gr);
    EXPECT_FLOAT_EQ(gl, 1.0f);
    EXPECT_FLOAT_EQ(gr, 0.0f);
    const float centreDb[] = {0.0f, -3.01f, -4.5f, -6.02f};
    for (int law = 1; law < 4; ++law) {
        PanEngine::panGains(0.0f, law, gl, gr);
        EXPECT_NEAR(20.0f * std::log10(gl), centreDb[law], 0.1f) << law;
        PanEngine::panGains(1.0f, law, gl, gr);
        EXPECT_NEAR(gl, 0.0f, 1e-4f);
        EXPECT_NEAR(gr, 1.0f, 1e-4f);
    }
}

TEST(PanEngine, SweepsLeftAndRightInTime) {
    PanEngine e;
    e.prepare(kSr);
    PanCurve c;  // -1 at phase 0, +1 at phase 0.5
    const auto t = table(c);
    PanSettings s;
    s.sync = false;
    s.rateHz = 1.0f;
    s.smoothMs = 0.0f;
    const int n = static_cast<int>(kSr);
    std::vector<float> l(static_cast<std::size_t>(n), 1.0f), r = l;
    for (int i = 0; i < n; i += 480) e.process(l.data() + i, r.data() + i, std::min(480, n - i), t, s, {});
    EXPECT_NEAR(r[10], 0.0f, 0.01f);          // start: hard left, right side silent
    EXPECT_NEAR(l[n / 2], 0.0f, 0.01f);       // half a second: hard right
    EXPECT_NEAR(l[n / 4], 1.0f, 0.01f);       // quarter: centre
    EXPECT_NEAR(r[n / 4], 1.0f, 0.02f);
}

TEST(PanEngine, FollowsSongPosition) {
    PanEngine e;
    e.prepare(kSr);
    PanCurve c;
    c.setPoints(PanCurve::shape(PanCurve::Square));  // left in the first half, right in the second
    const auto t = table(c);
    PanSettings s;
    s.division = 6;  // 1/4 note
    s.smoothMs = 0.0f;
    PanTransport tr;
    tr.playing = true;
    tr.ppq = 10.75;  // three quarters into a beat: second half -> right
    float l[64], r[64];
    std::fill(l, l + 64, 1.0f);
    std::fill(r, r + 64, 1.0f);
    e.process(l, r, 64, t, s, tr);
    EXPECT_NEAR(l[0], 0.0f, 1e-4f);
    EXPECT_NEAR(r[0], 1.0f, 1e-4f);
    tr.ppq = 3.2;  // first half -> left
    std::fill(l, l + 64, 1.0f);
    std::fill(r, r + 64, 1.0f);
    e.process(l, r, 64, t, s, tr);
    EXPECT_NEAR(l[0], 1.0f, 1e-4f);
    EXPECT_NEAR(r[0], 0.0f, 1e-4f);
}

TEST(PanEngine, VolumeAndWidthModes) {
    PanEngine e;
    e.prepare(kSr);
    PanCurve c;
    c.setPoints({{0, -1, 0, CurvePoint::Hold}});  // always at the bottom
    const auto t = table(c);
    PanSettings s;
    s.smoothMs = 0.0f;
    s.mode = PanSettings::Volume;
    s.depth = 0.5f;
    float l[16], r[16];
    std::fill(l, l + 16, 1.0f);
    std::fill(r, r + 16, 1.0f);
    e.process(l, r, 16, t, s, {});
    EXPECT_NEAR(l[8], 0.5f, 1e-4f);
    s.mode = PanSettings::Width;
    s.depth = 1.0f;
    std::fill(l, l + 16, 1.0f);
    std::fill(r, r + 16, -1.0f);  // pure side signal
    e.process(l, r, 16, t, s, {});
    EXPECT_NEAR(l[8], 0.0f, 1e-4f);  // width 0 = mono: the side signal disappears
}

TEST(PanEngine, MixZeroIsUntouched) {
    PanEngine e;
    e.prepare(kSr);
    const auto t = table(PanCurve{});
    PanSettings s;
    s.mix = 0.0f;
    float l[32], r[32];
    for (int i = 0; i < 32; ++i) l[i] = r[i] = 0.25f * static_cast<float>(i % 5);
    e.process(l, r, 32, t, s, {});
    for (int i = 0; i < 32; ++i) EXPECT_FLOAT_EQ(l[i], 0.25f * static_cast<float>(i % 5));
}

TEST(PanEngine, CpuCost) {
    PanEngine e;
    e.prepare(kSr);
    const auto t = table(PanCurve{});
    PanSettings s;
    std::vector<float> l(512, 0.3f), r(512, 0.2f);
    const int blocks = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) e.process(l.data(), r.data(), 512, t, s, {});
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / (blocks * 512 / kSr);
    std::printf("Patan CPU: %.2f %% of one core\n", pct);
    EXPECT_LT(pct, 5.0);
}
