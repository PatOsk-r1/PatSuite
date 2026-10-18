// PatQpro analysis tools (pure C++, used off the audio thread):
//   SpectrumAnalyzer: dB spectrum on a fixed log axis (20 Hz .. 20 kHz) from the newest samples.
//   EqMatch:          turns the difference between a reference spectrum and yours into a set of
//                     EQ bands (bells, plus shelves at the extremes).
//   Collision:        how much two spectra fight for the same frequencies (0..1 per point).
#pragma once

#include "common/core/Fft.h"
#include "dsp/eq/EqFilters.h"

#include <complex>
#include <memory>
#include <vector>

namespace pat::dsp {

class SpectrumAnalyzer {
public:
    static constexpr int kBins = 256;
    static float binHz(int i) noexcept;  // log axis 20 Hz .. 20 kHz
    static float binForHz(float hz) noexcept;

    explicit SpectrumAnalyzer(int maxFftSize = 8192);
    void setFftSize(int n) noexcept;  // power of two, 1024 .. maxFftSize
    int fftSize() const noexcept { return size_; }
    void push(const float* x, int n) noexcept;
    // Spectrum of the newest fftSize() samples, in dB (a full-scale sine reads about 0 dB).
    void compute(double sampleRate, float* outDb) noexcept;

private:
    int max_, size_;
    std::vector<std::unique_ptr<Fft>> ffts_;
    std::vector<float> ring_, window_, mags_, frame_;
    std::vector<std::complex<float>> scratch_;
    int write_ = 0;
};

struct EqMatch {
    // Smooth a spectrum (on the analyzer axis) over `octaves` (e.g. 1/3).
    static void smooth(const float* in, float* out, float octaves);
    // Bands that move `currentDb` toward `referenceDb` (both on the analyzer axis). The overall
    // level difference is ignored. amount 0..1 scales the result.
    static std::vector<EqBand> fit(const float* referenceDb, const float* currentDb, int maxBands, float amount);
};

struct Collision {
    // Per analyzer point: 0 = no clash .. 1 = both signals loud and close in level.
    static void compute(const float* aDb, const float* bDb, float* out, float floorDb = -60.0f);
};

}  // namespace pat::dsp
