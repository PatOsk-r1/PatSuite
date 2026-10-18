// PatQpro audio engine: up to 24 bands. Pure C++, no JUCE, no allocation in process().
//
// Processing modes:
//   Zero latency: digital biquads at the host rate (like an analog console, no delay).
//   Natural:      the bands run at twice the sample rate, so the top end keeps its analog shape;
//                 3 samples of latency (off above 80 kHz, where it is not needed).
//   Linear phase: every static band becomes one FIR (see LinearPhase); no phase shift at all.
//                 Dynamic bands still run as natural filters right after it.
//
// Every band can sit on both channels, only left/right, or only mid/side. Dynamic bands move
// their gain (up to "range") as the level around their frequency passes the threshold; the
// detector listens to the input or to the sidechain. Solo plays only the band's region.
#pragma once

#include "dsp/eq/EqFilters.h"
#include "dsp/eq/LinearPhase.h"
#include "dsp/eq/Oversampler2x.h"

#include <array>
#include <atomic>
#include <vector>

namespace pat::dsp {

struct EqGlobal {
    enum Mode { ZeroLatency = 0, Natural = 1, Linear = 2 };
    int mode = ZeroLatency;
    int resolution = 1;      // linear phase length: 0 low .. 3 maximum
    float gainScale = 1.0f;  // -2..2 multiplies every band gain
    float outputDb = 0.0f;
    float autoGainDb = 0.0f;  // compensation computed from the curve (0 when auto gain is off)
    float pan = 0.0f;         // output balance -1..1
    bool invert = false;      // output polarity
    float attackMs = 10.0f, releaseMs = 120.0f;  // dynamic bands
    int soloBand = -1;
};

class EqEngine {
public:
    static constexpr int kBands = 24;

    void prepare(double sampleRate, int resolution);
    void reset() noexcept;
    void process(float* l, float* r, const float* scL, const float* scR, int n, const EqBand* bands, const EqGlobal& g) noexcept;

    static int latency(int mode, int resolution, double sampleRate) noexcept;
    float dynamicGainDb(int band) const noexcept { return dynGain_[static_cast<std::size_t>(band)].load(std::memory_order_relaxed); }
    LinearPhase& linear() noexcept { return linear_; }
    // Static gain change (dB) of the whole curve averaged over the audible range (per octave).
    static float averageGainDb(const EqBand* bands, int numBands, float gainScale) noexcept;

private:
    static constexpr int kChunk = 256;  // detector / processing step
    struct Band {
        Biquad coef[EqDesign::kMaxSections];
        double z[EqDesign::kMaxSections][2][2] = {};
        int sections = 0, type = -1, slope = -1, place = -1;
        float freq = 0, gain = 0, q = 0;    // values the coefficients were made with
        float sFreq = 0, sGain = 0, sQ = 0;  // smoothed toward the parameters
        double fs = 0;
        bool on = false;
        // Dynamic detector.
        Biquad det;
        double dz[2] = {0, 0};
        float detFreq = 0, env = 0, dyn = 0;
    };

    void runChain(float* l, float* r, int n, double fs, const EqBand* bands, float scale, int which) noexcept;
    void updateBand(Band& st, const EqBand& b, float gainDb, double fs, int subLen) noexcept;
    void detect(const float* det, int n, const EqBand* bands, const EqGlobal& g, bool scDet, const float* scDetBuf) noexcept;
    void solo(float* l, float* r, int n, const EqBand& b) noexcept;

    double sr_ = 48000.0;
    std::array<Band, kBands> bands_{};
    std::array<std::atomic<float>, kBands> dynGain_{};
    Oversampler2x os_[2];
    std::vector<float> upL_, upR_, det_, scDet_, monoR_;
    LinearPhase linear_;
    int lastMode_ = -1;
    Biquad soloCoef_;
    double soloZ_[2][2] = {};
    int soloLast_ = -1;
    float outGain_ = 1.0f, panL_ = 1.0f, panR_ = 1.0f;
};

}  // namespace pat::dsp
