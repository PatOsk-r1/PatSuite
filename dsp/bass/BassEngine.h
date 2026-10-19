// PatBass synth engine: a synth made only for basses. Pure C++, no JUCE, no allocation after
// prepare().
//
//   per voice: OSC A (sine -> triangle -> saw -> square morph, pulse width, FM growl, unison)
//              OSC B (same shapes, octave / semitone / fine offset: reese, octaves, fifths)
//              SUB (pure sine or triangle one or two octaves down, kept out of the distortion)
//              NOISE, CLICK (attack transient), PITCH PUNCH (808 drop), GLIDE (portamento)
//              FILTER (24 / 12 dB low pass, band pass, high pass, VOWEL formant for talking basses)
//              with its own envelope, key tracking and drive; AMP envelope
//   global:    LFO (synced to the song or free) for wobbles: cutoff, pitch, shape, volume, vowel;
//              DISTORTION (soft, hard, fold, crush, tube) at 2x oversampling, only above the
//              "clean lows" split so the sub stays round; GLUE compressor; stereo width above
//              the split (the lows stay mono); volume.
#pragma once

#include "dsp/eq/Oversampler2x.h"

#include <array>
#include <cstdint>

namespace pat::dsp {

struct BassSettings {
    // OSC A
    float aShape = 0.66f, aPw = 0.5f, aFm = 0.0f;
    int aFmRatio = 1;  // index into kFmRatios
    int unison = 1;
    float detune = 0.2f, spread = 0.6f, aLevel = 0.8f;
    // OSC B
    bool bOn = false;
    float bShape = 0.66f;
    int bOct = 0, bSemi = 0;
    float bFine = 0.0f, bLevel = 0.6f;
    // SUB / NOISE / PUNCH
    bool subOn = true;
    int subOct = 1;    // 1 or 2 octaves down
    int subShape = 0;  // 0 sine, 1 triangle
    float subLevel = 0.6f, noise = 0.0f;
    float punchSemis = 0.0f, punchMs = 60.0f, click = 0.0f;
    // VOICE
    bool mono = true, legato = true;
    float glideMs = 0.0f, bendRange = 2.0f, velocity = 0.5f;
    // FILTER
    int filterType = 0;  // 0 LP24, 1 LP12, 2 BP, 3 HP, 4 Vowel
    float cutoff = 1200.0f, reso = 0.2f, filterDrive = 0.0f, filterEnv = 0.3f, keyTrack = 0.3f, vowel = 0.0f;
    float fA = 0.002f, fD = 0.3f, fS = 0.0f, fR = 0.2f;
    float aA = 0.002f, aD = 0.5f, aS = 1.0f, aR = 0.15f;
    // LFO
    int lfoShape = 0;  // 0 sine, 1 triangle, 2 saw down, 3 square, 4 random steps
    bool lfoSync = true, lfoRetrig = true;
    int lfoDiv = 3;    // index into kLfoDivisions
    float lfoHz = 2.0f;
    float lfoCut = 0.0f, lfoPitch = 0.0f, lfoShapeAmt = 0.0f, lfoVol = 0.0f, lfoVowel = 0.0f;
    // FX
    int distType = 0;  // 0 soft, 1 hard, 2 fold, 3 crush, 4 tube
    float drive = 0.0f, distMix = 1.0f;
    bool cleanLows = true;
    float split = 120.0f, glue = 0.0f, width = 0.5f, volumeDb = 0.0f;
};

class BassEngine {
public:
    static constexpr int kMaxVoices = 8, kMaxUnison = 7;
    static constexpr int kNumFmRatios = 8, kNumLfoDivisions = 12, kNumFilterTypes = 5, kNumDistTypes = 5, kNumLfoShapes = 5;
    static const char* fmRatioName(int i) noexcept;
    static const char* lfoDivisionName(int i) noexcept;
    static const char* filterTypeName(int i) noexcept;
    static const char* distTypeName(int i) noexcept;
    static const char* lfoShapeName(int i) noexcept;
    // Morph value of the basic shape at phase 0..1 (no anti-aliasing; for the display).
    static float shapeValue(float shape, float pw, double phase) noexcept;

    void prepare(double sampleRate);
    void reset() noexcept;
    // Settings for the next notes and samples (call before the MIDI events of each block).
    void setSettings(const BassSettings& s) noexcept { s_ = s; }
    void noteOn(int note, float velocity) noexcept;
    void noteOff(int note) noexcept;
    void allNotesOff() noexcept;
    void pitchBend(float amount) noexcept;  // -1..1
    // Renders n samples (adds nothing: overwrites l / r).
    void render(float* l, float* r, int n, double bpm, double ppq, bool playing) noexcept;

    float lfoValue() const noexcept { return lfoOut_; }       // -1..1, for the display
    float cutoffNow() const noexcept { return cutoffNow_; }   // Hz of the last voice, for the display
    int activeVoices() const noexcept;

private:
    struct Svf {
        float ic1 = 0, ic2 = 0;
        // Returns low / band / high outputs of one TPT state-variable filter step.
        void tick(float x, float g, float k, float& lp, float& bp, float& hp) noexcept;
    };
    struct Env {
        enum Stage { Idle, Attack, Decay, Sustain, Release } stage = Idle;
        float value = 0;
        void gateOn() noexcept { stage = Attack; }
        void gateOff() noexcept {
            if (stage != Idle) stage = Release;
        }
        float tick(float a, float d, float s, float r, float sr) noexcept;
    };
    struct Voice {
        int note = -1;
        bool held = false;
        float velocity = 1.0f;
        double pitch = 0;  // current (gliding) note number
        double target = 0;
        std::array<double, kMaxUnison> phaseA{}, phaseB{};
        double fmPhase = 0, subPhase = 0;
        float punch = 0, clickEnv = 0;
        uint32_t noise = 0x9E3779B9u;
        Env amp, filt;
        Svf f1[2], f2[2], vf[3][2];
        long long age = 0;
    };

    void startVoice(Voice& v, int note, float vel, bool retrigger) noexcept;
    void renderVoice(Voice& v, float* l, float* r, float* sub, int n, const BassSettings& s, const float* lfo) noexcept;
    float distort(float x, int type, float drive) const noexcept;
    static float velocityGain(float amount, float velocity) noexcept;

    double sr_ = 48000.0;
    BassSettings s_;
    std::array<Voice, kMaxVoices> voices_{};
    double lastPitch_ = -1;
    std::array<int, 16> heldStack_{};  // mono note priority (last pressed)
    int heldCount_ = 0;
    long long clock_ = 0;
    float bend_ = 0;
    double lfoPhase_ = 0;
    float lfoOut_ = 0, lfoHold_ = 0, cutoffNow_ = 1000;
    float lastVelocity_ = 1.0f, velNow_ = 1.0f, polyLoudest_ = 1.0f;  // velocity, applied after the FX
    uint32_t lfoRand_ = 12345;
    bool retrigLfo_ = false;
    // Buffers (sized in prepare for one chunk).
    static constexpr int kChunk = 64;
    std::array<float, kChunk> bufL_{}, bufR_{}, bufSub_{}, lfoBuf_{};
    // FX state.
    struct Biquad {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float tick(float x) noexcept {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };
    Biquad splitLp_[2][2], splitHp_[2][2];
    float lastSplit_ = -1;
    Oversampler2x os_[2];
    float crushHold_[2] = {0, 0};
    int crushCount_ = 0;
    float glueEnv_ = 0;
};

}  // namespace pat::dsp
