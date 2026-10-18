// Loudness meter (ITU-R BS.1770-4 / EBU R128): K-weighting, momentary (400 ms), short-term (3 s),
// integrated (gated) loudness, loudness range (LRA) and the highest true peak. Pure C++, no
// allocation after prepare(); everything runs on the audio thread and the results are plain
// values the UI reads (copied into atomics by the processor).
#pragma once

#include <array>
#include <vector>

namespace pat::dsp {

class LoudnessMeter {
public:
    static constexpr float kSilence = -70.0f;

    void prepare(double sampleRate);
    void reset() noexcept;
    void process(const float* l, const float* r, int n) noexcept;

    float momentary() const noexcept { return momentary_; }    // LUFS
    float shortTerm() const noexcept { return shortTerm_; }    // LUFS
    float integrated() const noexcept { return integrated_; }  // LUFS (kSilence until measured)
    float range() const noexcept { return range_; }            // LU
    float truePeakDb() const noexcept { return truePeak_; }    // dBTP, highest so far

    // True peak of a block (4x oversampled), used by the limiter too.
    struct TruePeak {
        std::array<float, 8> hist{};
        int pos = 0;
        float push(float x) noexcept;  // returns the true peak around the sample pushed 4 steps ago
    };

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
    static constexpr int kBins = 1000;  // histogram: -70 .. +30 LUFS in 0.1 LU steps
    static int bin(double lufs) noexcept;
    static double fromBin(int b) noexcept;
    void finishBlock() noexcept;

    double sr_ = 48000.0;
    Biquad shelf_, highpass_;
    int blockLen_ = 4800, blockFill_ = 0;
    double blockSum_ = 0;
    std::array<double, 30> blocks_{};  // last 30 x 100 ms energies (3 s)
    int blockCount_ = 0, blockPos_ = 0;
    std::vector<double> histEnergy_;   // integrated: summed energy and count per loudness bin
    std::vector<int> histCount_, stCount_;
    float momentary_ = kSilence, shortTerm_ = kSilence, integrated_ = kSilence, range_ = 0.0f, truePeak_ = -120.0f;
    TruePeak tpL_, tpR_;
};

}  // namespace pat::dsp
