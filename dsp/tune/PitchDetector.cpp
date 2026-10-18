#include "dsp/tune/PitchDetector.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {
namespace {
constexpr float kMinHz = 70.0f, kMaxHz = 1000.0f;
constexpr float kPeakRatio = 0.9f;   // MPM "k": first peak within 90 % of the best
constexpr float kMinClarity = 0.5f;  // below this the frame counts as unvoiced
}  // namespace

void PitchDetector::prepare(double sampleRate) {
    sr_ = sampleRate;
    // Window long enough for two periods of the lowest note (~43 ms).
    n_ = 1024;
    while (n_ < static_cast<int>(sampleRate * 0.043)) n_ <<= 1;
    hop_ = std::max(128, n_ / 8);
    ring_.assign(static_cast<std::size_t>(n_), 0.0f);
    frame_.assign(static_cast<std::size_t>(n_), 0.0f);
    nsdf_.assign(static_cast<std::size_t>(n_), 0.0f);
    spec_.assign(static_cast<std::size_t>(2 * n_), {});
    fft_ = std::make_unique<Fft>(2 * n_);
    reset();
}

void PitchDetector::reset() noexcept {
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    writePos_ = sinceLast_ = 0;
    hz_ = clarity_ = 0;
}

bool PitchDetector::push(float x) noexcept {
    ring_[static_cast<std::size_t>(writePos_)] = x;
    writePos_ = (writePos_ + 1) % n_;
    if (++sinceLast_ < hop_) return false;
    sinceLast_ = 0;
    analyse();
    return true;
}

void PitchDetector::analyse() noexcept {
    // Oldest-to-newest copy of the window.
    double energy = 0;
    for (int i = 0; i < n_; ++i) {
        const float v = ring_[static_cast<std::size_t>((writePos_ + i) % n_)];
        frame_[static_cast<std::size_t>(i)] = v;
        energy += static_cast<double>(v) * v;
    }
    if (energy / n_ < 1e-7) {  // silence (about -70 dBFS)
        hz_ = 0;
        clarity_ = 0;
        return;
    }
    // Autocorrelation r(tau) through the FFT (zero-padded to 2N, no circular wrap).
    for (int i = 0; i < 2 * n_; ++i) spec_[static_cast<std::size_t>(i)] = {i < n_ ? frame_[static_cast<std::size_t>(i)] : 0.0f, 0.0f};
    fft_->transform(spec_.data());
    for (auto& c : spec_) c = {std::norm(c), 0.0f};
    fft_->inverse(spec_.data());

    // NSDF: n(tau) = 2 r(tau) / m(tau), m(tau) = sum of x[j]^2 + x[j+tau]^2 (updated incrementally).
    const int maxTau = std::min(n_ / 2, static_cast<int>(sr_ / kMinHz) + 2);
    const int minTau = std::max(2, static_cast<int>(sr_ / kMaxHz));
    double m = 2.0 * energy;
    for (int tau = 0; tau < maxTau; ++tau) {
        const float r = spec_[static_cast<std::size_t>(tau)].real();
        nsdf_[static_cast<std::size_t>(tau)] = m > 0 ? static_cast<float>(2.0 * r / m) : 0.0f;
        const float a = frame_[static_cast<std::size_t>(tau)], b = frame_[static_cast<std::size_t>(n_ - 1 - tau)];
        m -= static_cast<double>(a) * a + static_cast<double>(b) * b;
    }

    // Key maxima: highest point between each positive-going and negative-going zero crossing.
    float best = 0;
    int tau = 1;
    while (tau < maxTau && nsdf_[static_cast<std::size_t>(tau)] > 0) ++tau;  // skip the zero-lag lobe
    constexpr int kMaxPeaks = 64;
    int peaks[kMaxPeaks];
    int numPeaks = 0;
    while (tau < maxTau - 1 && numPeaks < kMaxPeaks) {
        while (tau < maxTau - 1 && nsdf_[static_cast<std::size_t>(tau)] <= 0) ++tau;
        int peak = tau;
        while (tau < maxTau - 1 && nsdf_[static_cast<std::size_t>(tau)] > 0) {
            if (nsdf_[static_cast<std::size_t>(tau)] > nsdf_[static_cast<std::size_t>(peak)]) peak = tau;
            ++tau;
        }
        if (peak >= minTau) {
            peaks[numPeaks++] = peak;
            best = std::max(best, nsdf_[static_cast<std::size_t>(peak)]);
        }
    }
    if (numPeaks == 0 || best < kMinClarity) {
        hz_ = 0;
        clarity_ = std::max(0.0f, best);
        return;
    }
    for (int i = 0; i < numPeaks; ++i) {
        const int p = peaks[i];
        const float v = nsdf_[static_cast<std::size_t>(p)];
        if (v >= kPeakRatio * best) {
            const float a = nsdf_[static_cast<std::size_t>(p - 1)], c = nsdf_[static_cast<std::size_t>(p + 1)];
            const float denom = a - 2.0f * v + c;
            const float shift = std::abs(denom) > 1e-9f ? 0.5f * (a - c) / denom : 0.0f;
            hz_ = static_cast<float>(sr_ / (static_cast<double>(p) + std::clamp(shift, -0.5f, 0.5f)));
            clarity_ = v;
            return;
        }
    }
}

}  // namespace pat::dsp
