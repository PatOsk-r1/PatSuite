// PatSquash audio engine: three-band upward + downward compressor (the "OTT" style of sound).
// Pure C++, no JUCE, no allocation in process(), zero latency.
//
// The signal is split into low / mid / high with Linkwitz-Riley crossovers (the bands add back
// to the original when nothing is compressed). Each band:
//   - downward: above its "down" threshold it is turned down (ratio)
//   - upward:   below its "up" threshold quiet detail is turned up (ratio, at most +24 dB, and
//               never below -70 dB so silence and hiss are not pumped up)
// DEPTH blends the squashed sound with the original, TIME scales attack and release, UPWARD and
// DOWNWARD scale the two kinds of compression for all bands.
#pragma once

#include <array>

namespace pat::dsp {

struct SquashBand {
    float downThresholdDb = -30.0f, downRatio = 6.0f;
    float upThresholdDb = -40.0f, upRatio = 3.0f;
    float gainDb = 0.0f;
};

struct SquashSettings {
    std::array<SquashBand, 3> band{};
    float lowCrossHz = 120.0f, highCrossHz = 2500.0f;
    float depth = 1.0f;     // 0..1 dry / squashed
    float time = 1.0f;      // 0.1..4 attack/release scale
    float upward = 1.0f;    // 0..1 amount of upward compression
    float downward = 1.0f;  // 0..1 amount of downward compression
    float inputDb = 0.0f, outputDb = 0.0f;
};

class SquashEngine {
public:
    static constexpr float kMaxUpDb = 24.0f, kFloorDb = -70.0f;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* l, float* r, int n, const SquashSettings& s) noexcept;

    // Static curve of one band: gain change (dB) for an input level (dB).
    static float gainDb(float levelDb, const SquashBand& b, float upward, float downward) noexcept;

    // For the display (last block): level of each band before processing and its gain change.
    float bandLevelDb(int b) const noexcept { return levelDb_[static_cast<std::size_t>(b)]; }
    float bandGainDb(int b) const noexcept { return shownGain_[static_cast<std::size_t>(b)]; }

private:
    struct Biquad {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z[2][2] = {};
        double tick(double x, int ch) noexcept {
            const double y = b0 * x + z[ch][0];
            z[ch][0] = b1 * x - a1 * y + z[ch][1];
            z[ch][1] = b2 * x - a2 * y;
            return y;
        }
    };
    static void design(Biquad& f, double fs, double hz, bool highpass) noexcept;
    static void designAllpass(Biquad& f, double fs, double hz) noexcept;
    void updateCrossovers(float lo, float hi) noexcept;

    double sr_ = 48000.0;
    float lastLo_ = -1, lastHi_ = -1;
    // LR4 = two Butterworth sections in a row.
    Biquad lp1_[2], hp1_[2], lp2_[2], hp2_[2], ap2_;  // ap2_: keeps the low band in phase with mid + high
    std::array<float, 3> env_{}, gr_{}, levelDb_{}, shownGain_{};
    float inG_ = 1, outG_ = 1;
};

}  // namespace pat::dsp
