// PatPatPat audio engine: stereo / ping-pong delay. Pure C++, no JUCE, no allocation in
// process() (the delay memory is allocated in prepare()).
//
//   Mono:      left + right summed into one echo line, heard on both sides
//   Stereo:    each side has its own line and time
//   Ping-Pong: the sum starts on the left and every repeat jumps to the other side
//   Cross:     like Ping-Pong but keeps the stereo input (each side's echoes bounce across)
//
// In the feedback loop: low/high cut, saturation (also keeps 100 % feedback under control) and
// the character (Clean, Tape: wow and flutter, Analog: dark and warm, Lo-Fi: crushed). Around it:
// diffusion (smears the attack), ducking (echoes get out of the way while you play), freeze
// (holds the current echoes forever), stereo width of the echoes and the dry/wet mix.
#pragma once

#include <vector>

namespace pat::dsp {

struct DelaySettings {
    enum Mode { Mono = 0, Stereo = 1, PingPong = 2, Cross = 3 };
    enum Character { Clean = 0, Tape = 1, Analog = 2, LoFi = 3 };
    int mode = PingPong;
    int character = Clean;
    bool sync = true;
    int divisionL = 8, divisionR = 8;  // index into the note lengths (see divisionBeats)
    float timeLMs = 375.0f, timeRMs = 375.0f;
    bool link = true;  // right time follows the left one
    float feedback = 0.45f;  // 0..1
    float lowCutHz = 80.0f, highCutHz = 9000.0f;
    float drive = 0.0f;      // 0..1
    float modRateHz = 0.6f, modDepth = 0.0f;  // depth 0..1 (up to +-3 ms)
    float duck = 0.0f;       // 0..1
    bool freeze = false;
    float width = 1.0f;      // 0..2 stereo width of the echoes
    float diffusion = 0.0f;  // 0..1
    float mix = 0.35f;       // 0..1
    float outputDb = 0.0f;
};

struct DelayTap {
    double seconds = 0;
    float left = 0, right = 0;  // level of this repeat on each side (input = 1)
};

class DelayEngine {
public:
    static constexpr double kMaxSeconds = 4.0;
    static constexpr int kNumDivisions = 16;
    static const char* divisionName(int i) noexcept;
    static double divisionBeats(int i) noexcept;
    // Delay time in seconds of one side, from the settings and tempo (clamped to kMaxSeconds).
    static double timeSeconds(const DelaySettings& s, bool right, double bpm) noexcept;
    // The first repeats of a single click on both inputs, for the display. Returns the count.
    static int taps(const DelaySettings& s, double bpm, DelayTap* out, int maxTaps) noexcept;

    void prepare(double sampleRate);
    void reset() noexcept;
    void process(float* l, float* r, int n, const DelaySettings& s, double bpm) noexcept;

    float inputLevel() const noexcept { return inEnv_; }  // for the display (transients)
    float outputPeak(int ch) const noexcept { return ch == 0 ? peakL_ : peakR_; }
    float duckGain() const noexcept { return duckGain_; }

private:
    struct Line {
        std::vector<float> buf;
        int write = 0;
        void push(float x) noexcept;
        float read(double delaySamples) const noexcept;  // cubic interpolation
    };
    struct OnePole {
        float z = 0;
        float lp(float x, float a) noexcept { return z += a * (x - z); }
    };
    struct Allpass {
        float buf[2048] = {};
        int idx = 0, len = 1;
        float run(float x, float g) noexcept;
    };

    float shape(float x, float k) const noexcept;
    float loopTone(float x, int ch, const DelaySettings& s, float lpA, float hpA) noexcept;

    double sr_ = 48000.0;
    Line line_[2];
    OnePole lowpass_[2], lowpass2_[2], highpass_[2];
    Allpass diff_[2][4];
    double delay_[2] = {0, 0};   // smoothed delay in samples
    double lfo_ = 0, wow_ = 0;   // phases 0..1
    float fbState_[2] = {0, 0};  // last echo output of each line
    float crushHold_[2] = {0, 0};
    double crushPhase_ = 0;
    float inEnv_ = 0, duckEnv_ = 0, duckGain_ = 1;
    float peakL_ = 0, peakR_ = 0;
    bool first_ = true;
};

}  // namespace pat::dsp
