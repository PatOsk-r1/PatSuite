// PatMaster limiter: brickwall look-ahead limiter for the master. Pure C++, no allocation in
// process() (buffers sized in prepare()).
//
//   gain -> optional soft clip -> limiter (true peak detection, 1.5 ms look-ahead) -> ceiling
//
// The limiter never lets a peak over the ceiling through: the gain needed for each peak is held
// for the look-ahead time and smoothed with a box filter of the same length, so it is already
// down when the peak arrives (no distortion from fast attack). Release is fixed or automatic
// (fast for short peaks, slower when it limits all the time, to avoid pumping).
// Latency: look-ahead + 4 samples (true peak detection), reported to the host.
#pragma once

#include "dsp/master/Loudness.h"

#include <vector>

namespace pat::dsp {

struct LimiterSettings {
    float gainDb = 0.0f;        // drive into the limiter, 0..+24
    float ceilingDb = -1.0f;    // output ceiling, -12..0 dBFS
    float releaseMs = 100.0f;   // 1..1000
    bool autoRelease = true;
    float link = 1.0f;          // 0 = each side alone, 1 = both sides move together
    bool truePeak = true;       // catch peaks between samples too
    float clip = 0.0f;          // 0..1 soft clip before the limiter (louder, a bit of grit)
};

class LimiterEngine {
public:
    static constexpr double kLookaheadMs = 1.5;

    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* l, float* r, int n, const LimiterSettings& s) noexcept;

    int latency() const noexcept { return lookahead_ + 3; }
    float gainReductionDb() const noexcept { return grDb_; }  // deepest of the last block (<= 0)

private:
    float softClip(float x, float amount) const noexcept;

    double sr_ = 48000.0;
    int lookahead_ = 72;
    // Audio delay line (look-ahead + true-peak delay), per channel.
    std::vector<float> delay_[2];
    int delayPos_ = 0, delayLen_ = 0;
    // Required gain history (sliding minimum) and box filter.
    std::vector<float> req_[2], box_[2];
    int reqPos_ = 0;
    double boxSum_[2] = {0, 0};
    float env_[2] = {1, 1};
    float grAvg_ = 0;  // average limiting (dB), for automatic release
    LoudnessMeter::TruePeak tp_[2];
    float inG_ = 1, grDb_ = 0;
};

}  // namespace pat::dsp
