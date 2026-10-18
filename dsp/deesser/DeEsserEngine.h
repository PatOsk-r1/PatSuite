// PatDeEsser audio engine. Pure C++, no JUCE, no allocation in process(), zero latency.
//
//   detector:  band-pass at `freqHz` with `widthOct` octaves -> stereo-linked peak envelope
//              (0.5 ms attack, `releaseMs` release), in dB
//   gain:      soft-knee (6 dB) compressor curve: over threshold, reduce by (1 - 1/ratio)
//   split:     only the sibilance band is turned down:  out = (x - band) + g * band
//              (the state-variable filter splits x exactly, so g = 1 gives back x bit for bit)
//   wide:      the whole signal is turned down while the "s" lasts:  out = g * x
//   listen:    outputs only the detected band, to find the sibilance by ear
#pragma once

#include <cmath>

namespace pat::dsp {

struct DeEsserSettings {
    float freqHz = 6500.0f;    // centre of the sibilance band
    float widthOct = 1.17f;    // band width in octaves (1.17 = about 4-9 kHz around 6 kHz)
    float thresholdDb = -24.0f;
    float ratio = 4.0f;
    float releaseMs = 60.0f;
    bool split = true;
    bool listen = false;
    float outputDb = 0.0f;
};

// Topology-preserving state-variable band-pass, normalised to unity gain at the centre.
class BandSplitter {
public:
    void setup(float sampleRate, float freqHz, float widthOct) noexcept;
    void reset() noexcept { ic1_ = ic2_ = 0; }
    // Returns the band component of x (unity peak gain at the centre frequency).
    float band(float x) noexcept {
        const float v3 = x - ic2_;
        const float v1 = a1_ * ic1_ + a2_ * v3;
        const float v2 = ic2_ + a2_ * ic1_ + a3_ * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;
        return k_ * v1;
    }

private:
    float a1_ = 0, a2_ = 0, a3_ = 0, k_ = 1, ic1_ = 0, ic2_ = 0;
};

class DeEsserEngine {
public:
    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    void process(float* l, float* r, int n, const DeEsserSettings& s) noexcept;  // r may be null (mono)

    // Gain reduction for a detector level, in dB (<= 0). Public for the UI curve and the tests.
    static float gainReductionDb(float levelDb, float thresholdDb, float ratio) noexcept;

    // Meters of the last block.
    float detectorDb() const noexcept { return detDb_; }     // loudest sibilance level
    float reductionDb() const noexcept { return grDb_; }     // deepest reduction (<= 0)
    float inputPeak() const noexcept { return inPeak_; }
    float outputPeak() const noexcept { return outPeak_; }

private:
    float sr_ = 48000.0f;
    BandSplitter detL_, detR_, sigL_, sigR_;
    float lastFreq_ = -1, lastWidth_ = -1;
    float env_ = 0, attack_ = 0, outGain_ = 1;
    float detDb_ = -120, grDb_ = 0, inPeak_ = 0, outPeak_ = 0;
};

}  // namespace pat::dsp
