// Real-time TD-PSOLA pitch shifter for a monophonic voice.
//
// The input is cut into two-period grains centred on pitch marks placed exactly one period apart
// (fractional positions, read with interpolation, so non-integer periods stay phase-locked). Grains are overlap-added at a new spacing (period / ratio): the pitch
// changes while the spectral envelope (formants) stays where it was, so the voice keeps its
// character. Reading each grain faster or slower (`formantRatio`) moves the formants on their own.
// Unvoiced input (no pitch) is passed through with fixed 10 ms grains.
// Constant latency: latencySamples() (about 33 ms; the host compensates it). Allocates only in prepare().
#pragma once

#include <cstdint>
#include <vector>

namespace pat::dsp {

class PsolaShifter {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    int latencySamples() const noexcept { return latency_; }

    // periodSamples: current period of the input (0 = unvoiced). ratio: output / input pitch.
    float process(float x, float periodSamples, float ratio, float formantRatio) noexcept;

private:
    float in(std::int64_t t) const noexcept { return inBuf_[static_cast<std::size_t>(t & inMask_)]; }
    float inInterp(double t) const noexcept;
    void addGrain(double centreOut, double mark, float period, float gain, float formantRatio) noexcept;

    double sr_ = 48000;
    int latency_ = 1024;
    std::vector<float> inBuf_, outBuf_;
    std::int64_t inMask_ = 0, outMask_ = 0;
    std::int64_t inPos_ = 0;       // samples written so far
    double nextMark_ = 0;          // next analysis mark to place
    double marks_[64]{};           // recent analysis marks (input time)
    float markPeriod_[64]{};
    int markHead_ = 0, markCount_ = 0;
    double nextSynth_ = 0;         // next synthesis mark (output time)
    float lastPeriod_ = 0;
};

}  // namespace pat::dsp
