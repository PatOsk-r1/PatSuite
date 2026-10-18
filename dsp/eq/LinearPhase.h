// PatQpro linear-phase mode.
//
// The whole EQ (every static band) becomes four FIR filters (left->left, right->left,
// left->right, right->right: the cross terms exist only when mid/side bands are used), built
// from the ideal analog curve with zero phase, windowed, and run with a uniformly partitioned
// FFT convolution (512-sample partitions). Left and right travel together as one complex signal,
// so a stereo block costs one forward and one inverse FFT.
//
// Kernels are built off the audio thread (build()) and handed over lock-free; a new kernel is
// crossfaded in over one block. Retired kernels come back through a queue for the builder to free.
// Latency = 512 + length / 2 samples.
#pragma once

#include "common/core/Fft.h"
#include "common/core/SpscFifo.h"
#include "dsp/eq/EqFilters.h"

#include <atomic>
#include <complex>
#include <memory>
#include <vector>

namespace pat::dsp {

struct LinearKernel {
    int length = 0, partitions = 0;
    bool cross = false;  // mid/side bands present: cross terms are used
    // [path][partition * kFftSize + bin], paths: 0 LL, 1 RL (R in -> L out), 2 LR, 3 RR
    std::vector<std::complex<float>> h[4];
};

class LinearPhase {
public:
    static constexpr int kBlock = 512;
    static constexpr int kFftSize = 2 * kBlock;
    static int lengthFor(int resolution) noexcept { return 2048 << (resolution < 0 ? 0 : (resolution > 3 ? 3 : resolution)); }
    static int latency(int resolution) noexcept { return kBlock + lengthFor(resolution) / 2; }

    // Builder thread: the FIR kernel of the static EQ. gainScale multiplies every band gain.
    static std::unique_ptr<LinearKernel> build(const EqBand* bands, int numBands, float gainScale, double fs, int resolution);

    LinearPhase();
    ~LinearPhase();
    void prepare(int resolution);  // allocates for the largest kernel; resets the delay lines
    void reset() noexcept;

    // Builder thread: hand over a new kernel (takes ownership) and free retired ones.
    void publish(std::unique_ptr<LinearKernel> k);
    void collect();

    // Audio thread: in place, any block size. Output is delayed by latency(resolution).
    void process(float* l, float* r, int n) noexcept;
    bool hasKernel() const noexcept { return current_ != nullptr; }

private:
    void runBlock() noexcept;
    void convolve(const LinearKernel& k, std::complex<float>* out) noexcept;

    Fft fft_{kFftSize};
    int resolution_ = 1, latencyOut_ = 0;
    std::atomic<LinearKernel*> pending_{nullptr};
    LinearKernel* current_ = nullptr;
    SpscFifo<LinearKernel*> retired_{64};
    // Input block collection / output delivery.
    std::vector<float> inL_, inR_, outL_, outR_;
    int fill_ = 0;
    std::vector<float> histL_, histR_;  // last kFftSize input samples (overlap-save)
    // Frequency-domain delay line of input spectra (left and right separated).
    std::vector<std::complex<float>> fdlL_, fdlR_;
    int fdlPos_ = 0, maxPartitions_ = 0;
    std::vector<std::complex<float>> work_, accA_, accB_;
};

}  // namespace pat::dsp
