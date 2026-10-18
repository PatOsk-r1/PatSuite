// Patan audio engine: moves the sound with a drawn curve. Pure C++, no JUCE, no allocation in
// process(), zero latency.
//
//   Pan:    the curve moves the sound between left and right (balance of the stereo signal)
//   Volume: the curve turns the level up and down (tremolo / gate)
//   Width:  the curve opens and closes the stereo image (mono .. extra wide)
//
// The cycle runs locked to the song (a note length) or freely in Hz. While the host plays, the
// position follows the song position, so the movement always lands on the same beat.
#pragma once

#include "dsp/pan/PanCurve.h"

namespace pat::dsp {

struct PanSettings {
    enum Mode { Pan = 0, Volume = 1, Width = 2 };
    enum Law { Balance = 0, Law3dB = 1, Law45dB = 2, Law6dB = 3 };
    int mode = Pan;
    bool sync = true;
    int division = 6;          // index into kDivisionBeats
    float rateHz = 1.0f;       // free rate
    float depth = 1.0f;        // 0..1
    float phaseDeg = 0.0f;     // 0..360 offset of the cycle
    float smoothMs = 5.0f;     // glide of the movement (0..200)
    float center = 0.0f;       // -1..1 where the movement is centred (Pan mode)
    int law = Balance;
    float stereoWidth = 1.0f;  // 0..2 width of the input before moving it
    float mix = 1.0f;          // 0..1
    float outputDb = 0.0f;
    bool invert = false;       // mirror the curve (left <-> right)
};

struct PanTransport {
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
};

class PanEngine {
public:
    static constexpr int kNumDivisions = 16;
    static const char* divisionName(int i) noexcept;
    static double divisionBeats(int i) noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* l, float* r, int n, const CurveTable& curve, const PanSettings& s, const PanTransport& t) noexcept;

    // Pan gains for a position -1..1 with the chosen law (left, right).
    static void panGains(float pos, int law, float& gl, float& gr) noexcept;

    double phase() const noexcept { return phase_; }         // 0..1 at the end of the last block
    float position() const noexcept { return value_; }       // smoothed curve output, -1..1
    float outputPeak(int ch) const noexcept { return ch == 0 ? peakL_ : peakR_; }

private:
    double sr_ = 48000.0;
    double phase_ = 0.0;
    float value_ = 0.0f;
    float peakL_ = 0, peakR_ = 0;
};

}  // namespace pat::dsp
