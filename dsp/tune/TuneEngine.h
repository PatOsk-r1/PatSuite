// PatTune audio engine: real-time pitch correction to any tuning table.
//
//   detect:   McLeod pitch detector on the mono input (confidence = clarity)
//   target:   nearest allowed pitch in the scale (TuningTable: any cents, quarter tones included)
//   glide:    the correction moves towards the target with the retune speed (0 ms = instant,
//             the robotic effect); humanize slows it down on small deviations of held notes so
//             natural vibrato survives
//   shift:    PSOLA by the correction ratio; formants can be shifted separately
//   mix:      dry (delayed by the same latency, so it stays aligned) and corrected signals
// When Auto is off, or the confidence is below the gate, the voice passes through untouched.
// No allocation in process(). Constant latency: latencySamples().
#pragma once

#include "common/core/TuningTable.h"
#include "dsp/tune/PitchDetector.h"
#include "dsp/tune/PsolaShifter.h"

#include <cstdint>
#include <vector>

namespace pat::dsp {

struct TuneSettings {
    int scale = 1;  // index in the scale library (1 = Major)
    int key = 0;    // 0 = C
    std::uint32_t customMask = 0;
    double a4Hz = 440.0;
    bool autoOn = true;
    float retuneMs = 40.0f;   // 0..400
    float humanize = 0.3f;    // 0..1
    float formantSt = 0.0f;   // -12..12 semitones
    float mix = 1.0f;         // 0..1
    float gate = 0.6f;        // minimum confidence to correct
};

// One analysis step, for the piano roll.
struct TunePoint {
    float detectedHz = 0;  // 0 = no pitch
    float targetHz = 0;    // scale note it is pulled to
    float outputHz = 0;    // pitch after correction
    float clarity = 0;
};

class TuneEngine {
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    int latencySamples() const noexcept { return shifter_.latencySamples(); }
    // In place (r may be null). Writes one TunePoint per detector hop (up to maxPoints).
    int process(float* l, float* r, int n, const TuneSettings& s, TunePoint* points, int maxPoints) noexcept;

    const TuningTable& table() const noexcept { return table_; }
    TunePoint last() const noexcept { return last_; }

private:
    void updateTable(const TuneSettings& s) noexcept;

    double sr_ = 48000;
    PitchDetector detector_;
    PsolaShifter shifter_;
    TuningTable table_;
    int tableScale_ = -1, tableKey_ = -1;
    std::uint32_t tableMask_ = 0xffffffffu;
    double tableA4_ = 0;
    std::vector<float> dryL_, dryR_;
    int dryPos_ = 0;
    float correctionCents_ = 0, lastDetectedCents_ = 0;
    float period_ = 0;
    TunePoint last_;
};

}  // namespace pat::dsp
