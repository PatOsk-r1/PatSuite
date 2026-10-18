// Real-time monophonic pitch detector (McLeod Pitch Method).
//
// The normalised square difference function (NSDF) of the last window is computed with an FFT
// autocorrelation every `hop` samples. The first peak close to the highest one gives the period
// (parabolic interpolation for sub-sample accuracy); the peak height is the "clarity", used as
// the confidence meter (1 = perfectly periodic, near 0 = noise or silence).
// Range 70-1000 Hz (voice). Allocates only in prepare().
#pragma once

#include "common/core/Fft.h"

#include <complex>
#include <memory>
#include <vector>

namespace pat::dsp {

class PitchDetector {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    // Feed one sample; returns true when a new estimate is ready (every hop()).
    bool push(float x) noexcept;
    float frequency() const noexcept { return hz_; }    // 0 when unvoiced
    float clarity() const noexcept { return clarity_; }  // 0..1
    int hop() const noexcept { return hop_; }
    int window() const noexcept { return n_; }

private:
    void analyse() noexcept;

    double sr_ = 48000;
    int n_ = 2048, hop_ = 256, writePos_ = 0, sinceLast_ = 0;
    std::vector<float> ring_, frame_, nsdf_;
    std::vector<std::complex<float>> spec_;
    std::unique_ptr<Fft> fft_;
    float hz_ = 0, clarity_ = 0;
};

}  // namespace pat::dsp
