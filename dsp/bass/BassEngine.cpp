#include "dsp/bass/BassEngine.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;
constexpr float kFmRatios[BassEngine::kNumFmRatios] = {0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 7.0f};
constexpr const char* kFmNames[BassEngine::kNumFmRatios] = {"0.5", "1", "1.5", "2", "3", "4", "5", "7"};
struct Div {
    const char* name;
    double beats;
};
constexpr Div kDivs[BassEngine::kNumLfoDivisions] = {
    {"1/1", 4.0}, {"1/2", 2.0},         {"1/4", 1.0},         {"1/8", 0.5},         {"1/16", 0.25},       {"1/32", 0.125},
    {"1/2 T", 4.0 / 3.0}, {"1/4 T", 2.0 / 3.0}, {"1/8 T", 1.0 / 3.0}, {"1/16 T", 1.0 / 6.0}, {"1/4 D", 1.5}, {"1/8 D", 0.75},
};
constexpr const char* kFilterNames[BassEngine::kNumFilterTypes] = {"LP 24", "LP 12", "Band", "HP", "Vowel"};
constexpr const char* kDistNames[BassEngine::kNumDistTypes] = {"Soft", "Hard", "Fold", "Crush", "Tube"};
constexpr const char* kLfoNames[BassEngine::kNumLfoShapes] = {"Sine", "Triangle", "Saw", "Square", "Random"};
// Vowel formants (A, E, I, O, U): three frequencies each.
constexpr float kVowels[5][3] = {{800, 1150, 2900}, {400, 1600, 2700}, {350, 2300, 3000}, {450, 800, 2830}, {325, 700, 2530}};

inline double frac(double x) noexcept { return x - std::floor(x); }
inline float noteHz(double note) noexcept { return static_cast<float>(440.0 * std::exp2((note - 69.0) / 12.0)); }

inline float polyBlep(double t, double dt) noexcept {
    if (t < dt) {
        t /= dt;
        return static_cast<float>(t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return static_cast<float>(t * t + t + t + 1.0);
    }
    return 0.0f;
}

// Anti-aliased morph: 0 sine, 1/3 triangle, 2/3 saw, 1 square.
inline float morph(double t, double dt, float shape, float pw) noexcept {
    const float seg = std::clamp(shape, 0.0f, 1.0f) * 3.0f;
    const int i = std::min(2, static_cast<int>(seg));
    const float f = seg - static_cast<float>(i);
    const auto wave = [&](int k) -> float {
        switch (k) {
            case 0: return static_cast<float>(std::sin(kTwoPi * t));
            case 1: return static_cast<float>(1.0 - 4.0 * std::abs(t - 0.5));
            case 2: return static_cast<float>(2.0 * t - 1.0) - polyBlep(t, dt);
            default: {
                const double p = std::clamp(static_cast<double>(pw), 0.05, 0.95);
                return (t < p ? 1.0f : -1.0f) + polyBlep(t, dt) - polyBlep(frac(t + 1.0 - p), dt);
            }
        }
    };
    if (f < 1e-4f) return wave(i);
    return wave(i) + f * (wave(i + 1) - wave(i));
}

inline uint32_t xorshift(uint32_t& s) noexcept {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}
inline float noiseSample(uint32_t& s) noexcept { return static_cast<float>(xorshift(s) >> 8) * (2.0f / 16777216.0f) - 1.0f; }

void butter(float& b0, float& b1, float& b2, float& a1, float& a2, double fs, double hz, bool hp) noexcept {
    const double w = kTwoPi * std::clamp(hz, 10.0, fs * 0.45) / fs;
    const double c = std::cos(w), al = std::sin(w) / (2.0 * 0.70710678), a0 = 1.0 + al;
    b0 = static_cast<float>((hp ? (1.0 + c) : (1.0 - c)) * 0.5 / a0);
    b1 = static_cast<float>((hp ? -(1.0 + c) : (1.0 - c)) / a0);
    b2 = b0;
    a1 = static_cast<float>(-2.0 * c / a0);
    a2 = static_cast<float>((1.0 - al) / a0);
}
}  // namespace

const char* BassEngine::fmRatioName(int i) noexcept { return kFmNames[std::clamp(i, 0, kNumFmRatios - 1)]; }
const char* BassEngine::lfoDivisionName(int i) noexcept { return kDivs[std::clamp(i, 0, kNumLfoDivisions - 1)].name; }
const char* BassEngine::filterTypeName(int i) noexcept { return kFilterNames[std::clamp(i, 0, kNumFilterTypes - 1)]; }
const char* BassEngine::distTypeName(int i) noexcept { return kDistNames[std::clamp(i, 0, kNumDistTypes - 1)]; }
const char* BassEngine::lfoShapeName(int i) noexcept { return kLfoNames[std::clamp(i, 0, kNumLfoShapes - 1)]; }

float BassEngine::shapeValue(float shape, float pw, double phase) noexcept { return morph(frac(phase), 0.0, shape, pw); }

void BassEngine::Svf::tick(float x, float g, float k, float& lp, float& bp, float& hp) noexcept {
    const float a1 = 1.0f / (1.0f + g * (g + k));
    const float v3 = x - ic2;
    const float v1 = a1 * ic1 + g * a1 * v3;
    const float v2 = ic2 + g * v1;
    ic1 = 2.0f * v1 - ic1;
    ic2 = 2.0f * v2 - ic2;
    lp = v2;
    bp = v1;
    hp = x - k * v1 - v2;
}

float BassEngine::Env::tick(float a, float d, float s, float r, float sr) noexcept {
    switch (stage) {
        case Attack:
            value += 1.0f / std::max(1.0f, a * sr);
            if (value >= 1.0f) {
                value = 1.0f;
                stage = Decay;
            }
            break;
        case Decay:
            value = s + (value - s) * std::exp(-1.0f / std::max(1.0f, d * sr * 0.3f));
            if (std::abs(value - s) < 1e-4f) stage = Sustain;
            break;
        case Sustain: value = s; break;
        case Release:
            value *= std::exp(-1.0f / std::max(1.0f, r * sr * 0.3f));
            if (value < 1e-4f) {
                value = 0.0f;
                stage = Idle;
            }
            break;
        default: value = 0.0f; break;
    }
    return value;
}

void BassEngine::prepare(double sampleRate) {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    lastSplit_ = -1;
    reset();
}

void BassEngine::reset() noexcept {
    for (auto& v : voices_) v = Voice{};
    heldCount_ = 0;
    bend_ = 0;
    lfoPhase_ = 0;
    lfoOut_ = lfoHold_ = 0;
    for (auto& ch : splitLp_)
        for (auto& b : ch) b.z1 = b.z2 = 0;
    for (auto& ch : splitHp_)
        for (auto& b : ch) b.z1 = b.z2 = 0;
    for (auto& o : os_) o.reset();
    glueEnv_ = 0;
}

int BassEngine::activeVoices() const noexcept {
    int n = 0;
    for (const auto& v : voices_) n += v.amp.stage != Env::Idle ? 1 : 0;
    return n;
}

void BassEngine::pitchBend(float amount) noexcept { bend_ = std::clamp(amount, -1.0f, 1.0f); }

void BassEngine::startVoice(Voice& v, int note, float vel, bool retrigger) noexcept {
    const bool wasActive = v.amp.stage != Env::Idle;
    v.target = note;
    if (!wasActive) v.pitch = note;
    v.note = note;
    v.held = true;
    if (!retrigger) return;
    v.velocity = vel;
    v.age = ++clock_;
    for (int k = 0; k < kMaxUnison; ++k) {
        // Unison voices start spread out; the first one starts at zero (a clean, repeatable attack).
        v.phaseA[static_cast<std::size_t>(k)] = k == 0 ? 0.0 : frac(k * 0.37);
        v.phaseB[static_cast<std::size_t>(k)] = k == 0 ? 0.0 : frac(k * 0.61);
    }
    v.fmPhase = 0;
    v.subPhase = 0;
    v.punch = 1.0f;
    v.clickEnv = 1.0f;
    v.amp.gateOn();
    v.filt.gateOn();
    if (!wasActive) {
        v.amp.value = 0;
        v.filt.value = 0;
    }
    retrigLfo_ = true;
}

// Velocity -> gain. Amount 0 = every note equal; 0.5 = 18 dB between the softest and the hardest
// note (FL's default velocity 100 sounds about 4 dB under full); 1 = 36 dB.
float BassEngine::velocityGain(float amount, float velocity) noexcept {
    return std::pow(10.0f, std::clamp(amount, 0.0f, 1.0f) * 36.0f * (std::clamp(velocity, 0.0f, 1.0f) - 1.0f) / 20.0f);
}

void BassEngine::noteOn(int note, float velocity) noexcept {
    lastVelocity_ = velocity;  // mono: every new note sets the level, even when it slides
    if (activeVoices() == 0) velNow_ = polyLoudest_ = velocityGain(s_.velocity, velocity);  // first note: no ramp
    // Note stack for mono mode (last pressed wins).
    for (int i = 0; i < heldCount_; ++i)
        if (heldStack_[static_cast<std::size_t>(i)] == note) {
            std::copy(heldStack_.begin() + i + 1, heldStack_.begin() + heldCount_, heldStack_.begin() + i);
            --heldCount_;
            break;
        }
    if (heldCount_ < static_cast<int>(heldStack_.size())) heldStack_[static_cast<std::size_t>(heldCount_++)] = note;

    if (s_.mono) {
        auto& v = voices_[0];
        for (int i = 1; i < kMaxVoices; ++i) {  // coming from poly mode: release the others
            voices_[static_cast<std::size_t>(i)].held = false;
            voices_[static_cast<std::size_t>(i)].amp.gateOff();
            voices_[static_cast<std::size_t>(i)].filt.gateOff();
        }
        const bool slide = s_.legato && v.held && v.amp.stage != Env::Idle;
        startVoice(v, note, velocity, !slide);
        if (s_.glideMs <= 0.5f) v.pitch = note;  // no glide: jump
    } else {
        Voice* pick = nullptr;
        for (auto& v : voices_)
            if (v.amp.stage == Env::Idle) {
                pick = &v;
                break;
            }
        if (!pick) {
            pick = &voices_[0];
            for (auto& v : voices_)
                if (v.age < pick->age) pick = &v;
        }
        const double from = lastPitch_;
        startVoice(*pick, note, velocity, true);
        pick->pitch = s_.glideMs > 0.5f && from >= 0 ? from : note;
    }
    lastPitch_ = note;
}

void BassEngine::noteOff(int note) noexcept {
    for (int i = 0; i < heldCount_; ++i)
        if (heldStack_[static_cast<std::size_t>(i)] == note) {
            std::copy(heldStack_.begin() + i + 1, heldStack_.begin() + heldCount_, heldStack_.begin() + i);
            --heldCount_;
            break;
        }
    if (s_.mono) {
        auto& v = voices_[0];
        if (v.note != note) return;  // a note that is no longer playing
        if (heldCount_ > 0) {        // go back to the previous held note
            const int prev = heldStack_[static_cast<std::size_t>(heldCount_ - 1)];
            startVoice(v, prev, v.velocity, !s_.legato);
            if (s_.glideMs <= 0.5f) v.pitch = prev;
            lastPitch_ = prev;
        } else {
            v.held = false;
            v.amp.gateOff();
            v.filt.gateOff();
        }
        return;
    }
    for (auto& v : voices_)
        if (v.held && v.note == note) {
            v.held = false;
            v.amp.gateOff();
            v.filt.gateOff();
        }
}

void BassEngine::allNotesOff() noexcept {
    heldCount_ = 0;
    for (auto& v : voices_) {
        v.held = false;
        v.amp.gateOff();
        v.filt.gateOff();
    }
}

float BassEngine::distort(float x, int type, float drive) const noexcept {
    const float g = 1.0f + drive * 12.0f;
    const float norm = 1.0f / std::sqrt(g);
    switch (type) {
        case 1: return std::clamp(x * g, -1.0f, 1.0f) * norm * 1.2f;                           // hard
        case 2: return static_cast<float>(std::sin(x * g * 1.5)) * norm * 1.3f;                // fold
        case 3: {                                                                               // crush
            const float q = std::pow(2.0f, 12.0f - drive * 9.0f);
            return std::round(std::clamp(x * (1.0f + drive * 3.0f), -1.0f, 1.0f) * q) / q;
        }
        case 4: return (std::tanh(g * (x + 0.15f)) - std::tanh(g * 0.15f)) * norm * 1.2f;    // tube (asymmetric)
        default: return std::tanh(g * x) * norm * 1.1f;                                        // soft
    }
}

void BassEngine::renderVoice(Voice& v, float* l, float* r, float* sub, int n, const BassSettings& s, const float* lfo) noexcept {
    const float sr = static_cast<float>(sr_);
    const int uni = std::clamp(s.unison, 1, kMaxUnison);
    const float uniNorm = 1.0f / std::sqrt(static_cast<float>(uni));
    const float glideCoef = s.glideMs > 0.5f ? 1.0f - std::exp(-1.0f / (s.glideMs * 0.001f * sr / 3.0f)) : 1.0f;
    const float punchDecay = std::exp(-1.0f / (std::max(1.0f, s.punchMs) * 0.001f * sr));
    const float clickDecay = std::exp(-1.0f / (0.0025f * sr));
    const float fmRatio = kFmRatios[std::clamp(s.aFmRatio, 0, kNumFmRatios - 1)];
    // The level of the loudest note is applied after drive and glue (see render), otherwise they
    // would even it out and velocity would barely be heard. Here only the balance between notes.
    const float vel = s.mono ? 1.0f : velocityGain(s.velocity, v.velocity) / polyLoudest_;
    const float k1 = 1.414f, kRes = 2.0f - 1.96f * std::clamp(s.reso, 0.0f, 1.0f);
    // Unison: detune ratio and pan of each copy (fixed for the whole chunk).
    double up[kMaxUnison], down[kMaxUnison];
    float panL[kMaxUnison], panR[kMaxUnison];
    for (int k = 0; k < uni; ++k) {
        const float pos = uni > 1 ? static_cast<float>(k) / static_cast<float>(uni - 1) * 2.0f - 1.0f : 0.0f;
        up[k] = std::exp2(s.detune * 40.0 * pos / 1200.0);
        down[k] = 1.0 / up[k];
        const float pan = pos * s.spread;
        panL[k] = 1.0f - std::max(0.0f, pan);
        panR[k] = 1.0f + std::min(0.0f, pan);
    }
    const double bOffset = s.bOct * 12 + s.bSemi + s.bFine * 0.01;

    for (int i = 0; i < n; ++i) {
        v.pitch += glideCoef * (v.target - v.pitch);
        v.punch *= punchDecay;
        v.clickEnv *= clickDecay;
        const float lf = lfo[i];
        const double basePitch = v.pitch + bend_ * s.bendRange + v.punch * s.punchSemis + lf * s.lfoPitch * 12.0f;
        const float env = v.amp.tick(s.aA, s.aD, s.aS, s.aR, sr);
        const float fenv = v.filt.tick(s.fA, s.fD, s.fS, s.fR, sr);
        if (v.amp.stage == Env::Idle) break;

        // Oscillators.
        const float shapeA = std::clamp(s.aShape + lf * s.lfoShapeAmt, 0.0f, 1.0f);
        const float hzA = noteHz(basePitch);
        const double fmInc = hzA * fmRatio / sr_;
        v.fmPhase = frac(v.fmPhase + fmInc);
        const double fmOffset = s.aFm > 0.0f ? s.aFm * 0.8 * std::sin(kTwoPi * v.fmPhase) : 0.0;
        float oL = 0, oR = 0;
        const double hzB = s.bOn ? noteHz(basePitch + bOffset) : 0.0;
        for (int k = 0; k < uni; ++k) {
            const double incA = hzA * up[k] / sr_;
            auto& pa = v.phaseA[static_cast<std::size_t>(k)];
            pa = frac(pa + incA);
            float x = s.aLevel * morph(frac(pa + fmOffset), incA, shapeA, s.aPw);
            if (s.bOn) {
                const double incB = hzB * down[k] / sr_;
                auto& pb = v.phaseB[static_cast<std::size_t>(k)];
                pb = frac(pb + incB);
                x += s.bLevel * morph(pb, incB, s.bShape, s.aPw);
            }
            oL += x * panL[k];
            oR += x * panR[k];
        }
        oL *= uniNorm;
        oR *= uniNorm;
        if (s.noise > 0.0f || s.click > 0.0f) {
            const float nz = noiseSample(v.noise);
            const float extra = s.noise * 0.5f * nz + s.click * v.clickEnv * (0.6f * nz + 0.8f * static_cast<float>(std::sin(kTwoPi * v.fmPhase * 8.0)));
            oL += extra;
            oR += extra;
        }

        // Filter.
        const float oct = s.filterEnv * fenv * 6.0f + s.keyTrack * static_cast<float>((basePitch - 48.0) / 12.0) + lf * s.lfoCut * 3.0f;
        const float fc = std::clamp(s.cutoff * std::exp2(oct), 20.0f, sr * 0.45f);
        cutoffNow_ = fc;
        const float g = static_cast<float>(std::tan(kPi * fc / sr_));
        float in[2] = {oL, oR};
        if (s.filterDrive > 0.0f)
            for (float& x : in) x = std::tanh(x * (1.0f + s.filterDrive * 4.0f)) / (1.0f + s.filterDrive);
        float out[2];
        for (int c = 0; c < 2; ++c) {
            float lp, bp, hp, lp2, bp2, hp2;
            switch (s.filterType) {
                case 1:  // LP 12
                    v.f1[c].tick(in[c], g, kRes, lp, bp, hp);
                    out[c] = lp;
                    break;
                case 2:  // band pass
                    v.f1[c].tick(in[c], g, kRes, lp, bp, hp);
                    out[c] = bp * kRes;
                    break;
                case 3:  // high pass 24
                    v.f1[c].tick(in[c], g, k1, lp, bp, hp);
                    v.f2[c].tick(hp, g, kRes, lp2, bp2, hp2);
                    out[c] = hp2;
                    break;
                case 4: {  // vowel: three formant band passes, morphing A-E-I-O-U
                    const float vw = std::clamp(s.vowel + lf * s.lfoVowel, 0.0f, 1.0f) * 4.0f;
                    const int a = std::min(3, static_cast<int>(vw));
                    const float f = vw - static_cast<float>(a);
                    const float gains[3] = {1.0f, 0.7f, 0.35f};
                    const float q = 0.12f + (1.0f - s.reso) * 0.25f;
                    float sum = 0.0f;
                    for (int fm = 0; fm < 3; ++fm) {
                        const float hz = kVowels[a][fm] + f * (kVowels[a + 1][fm] - kVowels[a][fm]);
                        const float gf = static_cast<float>(std::tan(kPi * std::min(hz, sr * 0.45f) / sr_));
                        v.vf[fm][c].tick(in[c], gf, q, lp, bp, hp);
                        sum += bp * q * gains[fm] * 2.5f;
                    }
                    // Keep some body under the vowel, following the cutoff.
                    v.f1[c].tick(in[c], g, k1, lp, bp, hp);
                    out[c] = sum + 0.35f * lp;
                    break;
                }
                default:  // LP 24
                    v.f1[c].tick(in[c], g, k1, lp, bp, hp);
                    v.f2[c].tick(lp, g, kRes, lp2, bp2, hp2);
                    out[c] = lp2;
                    break;
            }
        }
        const float trem = 1.0f - s.lfoVol * (0.5f - 0.5f * lf);
        const float amp = env * vel * trem;
        l[i] += out[0] * amp;
        r[i] += out[1] * amp;

        // Sub: clean sine / triangle below, mono.
        if (s.subOn) {
            const double incS = noteHz(basePitch - 12.0 * std::clamp(s.subOct, 1, 2)) / sr_;
            v.subPhase = frac(v.subPhase + incS);
            const float sv = s.subShape == 1 ? static_cast<float>(1.0 - 4.0 * std::abs(v.subPhase - 0.5)) : static_cast<float>(std::sin(kTwoPi * v.subPhase));
            sub[i] += sv * s.subLevel * amp;
        }
    }
}

void BassEngine::render(float* l, float* r, int n, double bpm, double ppq, bool playing) noexcept {
    const BassSettings& s = s_;
    if (s.split != lastSplit_) {
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 2; ++k) {
                auto& lp = splitLp_[c][k];
                auto& hp = splitHp_[c][k];
                butter(lp.b0, lp.b1, lp.b2, lp.a1, lp.a2, sr_, s.split, false);
                butter(hp.b0, hp.b1, hp.b2, hp.a1, hp.a2, sr_, s.split, true);
            }
        lastSplit_ = s.split;
    }
    const double beats = kDivs[std::clamp(s.lfoDiv, 0, kNumLfoDivisions - 1)].beats;
    const double lfoInc = s.lfoSync ? std::max(20.0, bpm) / 60.0 / beats / sr_ : std::max(0.01f, s.lfoHz) / sr_;
    const float vol = std::pow(10.0f, s.volumeDb / 20.0f);
    const float glueRatio = 1.0f + s.glue * 5.0f, glueThr = -16.0f, makeup = std::pow(10.0f, s.glue * 6.0f / 20.0f);
    const float gAtt = 1.0f - std::exp(-1.0f / (0.004f * static_cast<float>(sr_))), gRel = 1.0f - std::exp(-1.0f / (0.08f * static_cast<float>(sr_)));
    const float width = std::clamp(s.width, 0.0f, 1.0f) * 2.0f;
    const float velCoef = 1.0f - std::exp(-1.0f / (0.003f * static_cast<float>(sr_)));
    if (retrigLfo_ && s.lfoRetrig) lfoPhase_ = 0.0;
    retrigLfo_ = false;
    const bool followSong = s.lfoSync && !s.lfoRetrig && playing;
    double songPos = ppq / beats;

    for (int off = 0; off < n; off += kChunk) {
        const int m = std::min(kChunk, n - off);
        // LFO for this chunk.
        for (int i = 0; i < m; ++i) {
            const double ph = followSong ? frac(songPos) : lfoPhase_;
            if (!followSong) {
                const double next = lfoPhase_ + lfoInc;
                if (next >= 1.0) lfoHold_ = noiseSample(lfoRand_);
                lfoPhase_ = frac(next);
            } else {
                const double next = songPos + std::max(20.0, bpm) / 60.0 / beats / sr_;
                if (std::floor(next) != std::floor(songPos)) lfoHold_ = noiseSample(lfoRand_);
                songPos = next;
            }
            float v;
            switch (s.lfoShape) {
                case 1: v = static_cast<float>(1.0 - 4.0 * std::abs(ph - 0.5)); break;
                case 2: v = static_cast<float>(1.0 - 2.0 * ph); break;
                case 3: v = ph < 0.5 ? 1.0f : -1.0f; break;
                case 4: v = lfoHold_; break;
                default: v = static_cast<float>(std::sin(kTwoPi * ph)); break;
            }
            lfoBuf_[static_cast<std::size_t>(i)] = v;
        }
        lfoOut_ = lfoBuf_[0];

        std::fill(bufL_.begin(), bufL_.begin() + m, 0.0f);
        std::fill(bufR_.begin(), bufR_.begin() + m, 0.0f);
        std::fill(bufSub_.begin(), bufSub_.begin() + m, 0.0f);
        float loudest = 0.0f;
        for (auto& v : voices_)
            if (v.amp.stage != Env::Idle) loudest = std::max(loudest, velocityGain(s.velocity, v.velocity));
        if (loudest > 0.0f) polyLoudest_ = loudest;
        const float velTarget = s.mono ? velocityGain(s.velocity, lastVelocity_) : polyLoudest_;
        for (auto& v : voices_)
            if (v.amp.stage != Env::Idle) renderVoice(v, bufL_.data(), bufR_.data(), bufSub_.data(), m, s, lfoBuf_.data());

        for (int i = 0; i < m; ++i) {
            float x[2] = {bufL_[static_cast<std::size_t>(i)], bufR_[static_cast<std::size_t>(i)]};
            float low[2] = {0, 0};
            if (s.cleanLows) {
                for (int c = 0; c < 2; ++c) {
                    low[c] = splitLp_[c][1].tick(splitLp_[c][0].tick(x[c]));
                    x[c] = splitHp_[c][1].tick(splitHp_[c][0].tick(x[c]));
                }
                const float lm = 0.5f * (low[0] + low[1]);  // lows in mono
                low[0] = low[1] = lm;
            }
            if (s.drive > 0.0f) {
                for (int c = 0; c < 2; ++c) {
                    float a, b;
                    os_[c].up(x[c], a, b);
                    if (s.distType == 3) {  // crush also lowers the sample rate
                        if (++crushCount_ >= 1 + static_cast<int>(s.drive * 6.0f)) {
                            crushCount_ = 0;
                            crushHold_[c] = distort(a, 3, s.drive);
                        }
                        a = b = crushHold_[c];
                    } else {
                        a = distort(a, s.distType, s.drive);
                        b = distort(b, s.distType, s.drive);
                    }
                    const float d = os_[c].down(a, b);
                    x[c] += s.distMix * (d - x[c]);
                }
            }
            // Width above the split; the lows (and the sub) stay mono.
            const float mid = 0.5f * (x[0] + x[1]), side = 0.5f * (x[0] - x[1]) * width;
            float o[2] = {mid + side + low[0], mid - side + low[1]};
            const float subv = bufSub_[static_cast<std::size_t>(i)];
            o[0] += subv;
            o[1] += subv;
            if (s.glue > 0.0f) {
                const float a = std::max(std::abs(o[0]), std::abs(o[1]));
                glueEnv_ += (a > glueEnv_ ? gAtt : gRel) * (a - glueEnv_);
                const float lvl = 20.0f * std::log10(glueEnv_ + 1e-9f);
                const float gr = lvl > glueThr ? (glueThr - lvl) * (1.0f - 1.0f / glueRatio) : 0.0f;
                const float gg = std::pow(10.0f, gr / 20.0f) * makeup;
                o[0] *= gg;
                o[1] *= gg;
            }
            velNow_ += velCoef * (velTarget - velNow_);
            l[off + i] = o[0] * vol * velNow_;
            r[off + i] = o[1] * vol * velNow_;
        }
    }
}

}  // namespace pat::dsp
