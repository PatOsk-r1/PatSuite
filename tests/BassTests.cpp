// PatBass engine tests: pitch, sub octave, glide, 808 punch, release, clean lows under heavy
// distortion, polyphony, stability and CPU.
#include "dsp/bass/BassEngine.h"

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

Out render(BassEngine& e, int n) {
    Out o;
    o.l.assign(static_cast<std::size_t>(n), 0.0f);
    o.r = o.l;
    for (int i = 0; i < n; i += 256) e.render(o.l.data() + i, o.r.data() + i, std::min(256, n - i), 120.0, 0.0, false);
    return o;
}

// Frequency from rising zero crossings over a range of samples.
double freq(const std::vector<float>& x, int from, int to) {
    int first = -1, last = -1, count = 0;
    for (int i = from + 1; i < to; ++i)
        if (x[static_cast<std::size_t>(i) - 1] < 0.0f && x[static_cast<std::size_t>(i)] >= 0.0f) {
            if (first < 0) first = i;
            else ++count;
            last = i;
        }
    return count > 0 ? count * kSr / (last - first) : 0.0;
}

BassSettings pureSine() {
    BassSettings s;
    s.aShape = 0.0f;  // sine
    s.subOn = false;
    s.cutoff = 20000.0f;
    s.filterEnv = 0.0f;
    s.keyTrack = 0.0f;
    s.filterType = 1;
    s.reso = 0.0f;
    s.width = 0.0f;
    return s;
}
}  // namespace

TEST(Bass, PlaysTheRightPitch) {
    BassEngine e;
    e.prepare(kSr);
    e.setSettings(pureSine());
    e.noteOn(45, 1.0f);  // A2 = 110 Hz
    const auto o = render(e, 24000);
    EXPECT_NEAR(freq(o.l, 4800, 24000), 110.0, 0.5);
}

TEST(Bass, SubIsOneOrTwoOctavesDown) {
    for (int oct : {1, 2}) {
        BassEngine e;
        e.prepare(kSr);
        auto s = pureSine();
        s.aLevel = 0.0f;
        s.subOn = true;
        s.subOct = oct;
        e.setSettings(s);
        e.noteOn(57, 1.0f);  // 220 Hz
        const auto o = render(e, 48000);
        EXPECT_NEAR(freq(o.l, 9600, 48000), 220.0 / (oct == 1 ? 2.0 : 4.0), 0.5) << oct;
    }
}

TEST(Bass, GlideSlidesBetweenNotes) {
    BassEngine e;
    e.prepare(kSr);
    auto s = pureSine();
    s.glideMs = 200.0f;
    e.setSettings(s);
    e.noteOn(45, 1.0f);
    auto o = render(e, 9600);
    e.noteOn(57, 1.0f);  // legato: slides up an octave
    o = render(e, 48000);
    const double early = freq(o.l, 0, 2400), late = freq(o.l, 30000, 48000);
    EXPECT_GT(early, 112.0);
    EXPECT_LT(early, 200.0);  // still on the way
    EXPECT_NEAR(late, 220.0, 1.0);
}

TEST(Bass, PunchStartsHighAndDrops) {
    BassEngine e;
    e.prepare(kSr);
    auto s = pureSine();
    s.punchSemis = 24.0f;
    s.punchMs = 30.0f;
    e.setSettings(s);
    e.noteOn(33, 1.0f);  // 55 Hz
    const auto o = render(e, 48000);
    EXPECT_GT(freq(o.l, 0, 800), 100.0);
    EXPECT_NEAR(freq(o.l, 24000, 48000), 55.0, 0.5);
}

TEST(Bass, ReleaseEndsInSilence) {
    BassEngine e;
    e.prepare(kSr);
    auto s = pureSine();
    s.aR = 0.1f;
    e.setSettings(s);
    e.noteOn(45, 1.0f);
    render(e, 4800);
    e.noteOff(45);
    const auto o = render(e, 48000);
    float tail = 0;
    for (int i = 40000; i < 48000; ++i) tail = std::max(tail, std::abs(o.l[static_cast<std::size_t>(i)]));
    EXPECT_LT(tail, 1e-3f);
    EXPECT_EQ(e.activeVoices(), 0);
}

TEST(Bass, SubStaysCleanUnderHeavyDistortion) {
    BassEngine e;
    e.prepare(kSr);
    auto s = pureSine();
    s.aLevel = 0.0f;
    s.subOn = true;
    s.drive = 1.0f;
    s.distType = 1;  // hard clip
    e.setSettings(s);
    e.noteOn(45, 1.0f);  // sub at 55 Hz
    const auto o = render(e, 48000);
    // A pure sine: its peak and RMS keep the sine ratio (sqrt 2) - clipping would flatten it.
    float pk = 0;
    double rms = 0;
    for (int i = 24000; i < 48000; ++i) {
        pk = std::max(pk, std::abs(o.l[static_cast<std::size_t>(i)]));
        rms += o.l[static_cast<std::size_t>(i)] * o.l[static_cast<std::size_t>(i)];
    }
    rms = std::sqrt(rms / 24000.0);
    EXPECT_NEAR(pk / rms, std::sqrt(2.0), 0.03);
}

TEST(Bass, PolyPlaysChords) {
    BassEngine e;
    e.prepare(kSr);
    auto s = pureSine();
    s.mono = false;
    e.setSettings(s);
    e.noteOn(45, 1.0f);
    e.noteOn(49, 1.0f);
    e.noteOn(52, 1.0f);
    render(e, 2400);
    EXPECT_EQ(e.activeVoices(), 3);
}

TEST(Bass, EveryShapeAndFilterStaysFinite) {
    BassEngine e;
    e.prepare(kSr);
    BassSettings s;
    s.unison = 7;
    s.bOn = true;
    s.aFm = 1.0f;
    s.reso = 1.0f;
    s.drive = 1.0f;
    s.glue = 1.0f;
    s.click = 1.0f;
    s.noise = 0.5f;
    s.lfoCut = 1.0f;
    s.lfoPitch = 0.5f;
    for (int ft = 0; ft < BassEngine::kNumFilterTypes; ++ft)
        for (int dt = 0; dt < BassEngine::kNumDistTypes; ++dt) {
            s.filterType = ft;
            s.distType = dt;
            e.setSettings(s);
            e.noteOn(36 + ft, 1.0f);
            const auto o = render(e, 4800);
            for (float v : o.l) ASSERT_TRUE(std::isfinite(v)) << ft << " " << dt;
            float pk = 0;
            for (float v : o.l) pk = std::max(pk, std::abs(v));
            EXPECT_LT(pk, 8.0f) << ft << " " << dt;
            e.noteOff(36 + ft);
        }
}

TEST(Bass, CpuCost) {
    BassEngine e;
    e.prepare(kSr);
    BassSettings s;
    s.unison = 5;
    s.bOn = true;
    s.drive = 0.5f;
    s.glue = 0.5f;
    s.lfoCut = 0.5f;
    e.setSettings(s);
    e.noteOn(36, 1.0f);
    std::vector<float> l(512), r(512);
    const int blocks = 2000;
    const auto t0 = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b) e.render(l.data(), r.data(), 512, 140.0, b * 0.02, true);
    const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const double pct = 100.0 * sec / (blocks * 512 / kSr);
    std::printf("PatBass CPU (1 note, 5 unison x 2 osc, drive, glue): %.2f %% of one core\n", pct);
    EXPECT_LT(pct, 15.0);
}

// Velocity must still be heard with heavy drive and glue (they used to even it out), in mono and
// in poly, and a soft note must sound softer than a hard one.
TEST(Bass, VelocityIsHeardThroughDriveAndGlue) {
    for (bool mono : {true, false}) {
        auto peakAt = [mono](float velocity) {
            BassEngine e;
            e.prepare(kSr);
            BassSettings s;
            s.mono = mono;
            s.drive = 0.8f;
            s.glue = 1.0f;
            s.velocity = 0.5f;
            e.setSettings(s);
            e.noteOn(40, velocity);
            const auto o = render(e, 24000);
            float p = 0.0f;
            for (int i = 4800; i < 24000; ++i) p = std::max(p, std::abs(o.l[static_cast<std::size_t>(i)]));
            return 20.0f * std::log10(p + 1e-9f);
        };
        const float full = peakAt(1.0f), fl = peakAt(100.0f / 127.0f), soft = peakAt(0.4f);
        std::printf("mono %d: vel 127 %.1f dB, vel 100 %.1f dB, vel 51 %.1f dB\n", mono ? 1 : 0, full, fl, soft);
        EXPECT_GT(full - soft, 6.0f);
        EXPECT_GT(full - fl, 2.0f);
    }
}

TEST(Bass, ZeroVelocityAmountPlaysEveryNoteEqual) {
    BassEngine e;
    e.prepare(kSr);
    BassSettings s;
    s.velocity = 0.0f;
    e.setSettings(s);
    e.noteOn(40, 0.2f);
    const auto o = render(e, 9600);
    float p = 0.0f;
    for (float x : o.l) p = std::max(p, std::abs(x));
    EXPECT_GT(p, 0.1f);
}
