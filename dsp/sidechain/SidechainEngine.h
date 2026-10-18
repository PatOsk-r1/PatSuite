// PatChain audio engine. Pure C++, no JUCE, no allocation in process(), zero latency.
//
// Two ways to make the music "pump":
//   Tempo:     a ducking shape repeats every 1/4, 1/8 or 1/16 note, locked to the song.
//   Sidechain: a compressor listens to the sidechain input (usually the kick) through a band
//              filter and turns the main signal down when it hits.
//   Auto picks Sidechain while a sidechain signal is present, Tempo otherwise.
// "Pump" is how deep the ducking goes in both modes (0 = off, 1 = full).
#pragma once

#include <cmath>

namespace pat::dsp {

struct SidechainSettings {
    enum Trigger { Auto = 0, Tempo = 1, Sidechain = 2 };
    int trigger = Auto;
    float pump = 0.7f;        // 0..1
    int rate = 2;             // index into the note lengths (see divisionName), default 1/4
    int shape = 0;            // tempo curve (see shapeName)
    float thresholdDb = -24.0f;
    float attackMs = 2.0f;
    float releaseMs = 180.0f;
    float kneeDb = 6.0f;
    float ratio = 8.0f;
    float scLowCutHz = 20.0f;
    float scHighCutHz = 150.0f;
    bool listenSidechain = false;
    float outputDb = 0.0f;
};

struct Transport {
    double bpm = 120.0;
    double ppq = 0.0;    // song position in quarter notes at the start of the block
    bool playing = false;
};

// One point of the scrolling graph: the trigger (kick) level and the applied gain.
struct GraphPoint {
    float trigger = 0;  // 0..1
    float gain = 1;     // 0..1
};

class SidechainEngine {
public:
    static constexpr int kGraphStep = 128;  // samples per graph point

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    // main: in place (r may be null). sc: sidechain input, may be null (no sidechain bus).
    // Graph points are written to `graph` (up to maxPoints) and their count returned.
    int process(float* l, float* r, const float* scL, const float* scR, int n, const SidechainSettings& s, const Transport& t,
                GraphPoint* graph, int maxPoints) noexcept;

    static float compressorGainDb(float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept;
    static constexpr int kNumDivisions = 10, kNumShapes = 8;
    static double divisionBeats(int rate) noexcept;
    static const char* divisionName(int rate) noexcept;
    static const char* shapeName(int shape) noexcept;
    // Ducking amount (0 = full level, 1 = fully ducked) of a tempo curve: `fraction` is the position
    // inside the note length (0..1), `seconds` the time since the hit.
    static float shapeAmount(int shape, double fraction, double seconds, double lengthSeconds, const SidechainSettings& s) noexcept;

    float minGain() const noexcept { return minGain_; }        // lowest gain of the last block
    float triggerDb() const noexcept { return triggerDb_; }    // sidechain envelope level
    int triggers() const noexcept { return triggers_; }        // running count of hits / beats
    bool usingSidechain() const noexcept { return usingSc_; }  // which trigger Auto chose
    bool sidechainSignal() const noexcept { return scActivity_ > 0.0f; }  // something arrives on the sidechain

private:
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void set(float sr, float f, bool highpass) noexcept;
        float tick(float x) noexcept {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    float sr_ = 48000;
    Biquad hp_, lp_;
    float lastLow_ = -1, lastHigh_ = -1;
    float env_ = 0, grDb_ = 0, compGain_ = 1;  // grDb_: smoothed gain reduction (dB, <= 0)
    bool above_ = false;
    double freeBeats_ = 0, lastBeatPhase_ = 0;
    float scActivity_ = 0;  // seconds of recent sidechain signal (Auto mode)
    float outGain_ = 1;
    float minGain_ = 1, triggerDb_ = -120;
    int triggers_ = 0;
    bool usingSc_ = false;
    int graphCount_ = 0;
    float graphTrig_ = 0, graphGain_ = 1;
};

}  // namespace pat::dsp
