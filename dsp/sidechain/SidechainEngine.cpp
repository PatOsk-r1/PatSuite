#include "dsp/sidechain/SidechainEngine.h"

#include <algorithm>

namespace pat::dsp {
namespace {
constexpr float kPi = 3.14159265358979f;
inline float toDb(float g) noexcept { return g > 1e-9f ? 20.0f * std::log10(g) : -180.0f; }
inline float fromDb(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
}  // namespace

void SidechainEngine::Biquad::set(float sr, float f, bool highpass) noexcept {
    const float w = 2.0f * kPi * std::clamp(f, 10.0f, sr * 0.45f) / sr;
    const float c = std::cos(w), al = std::sin(w) / (2.0f * 0.7071f), a0 = 1.0f + al;
    if (highpass) {
        b0 = (1.0f + c) * 0.5f / a0;
        b1 = -(1.0f + c) / a0;
    } else {
        b0 = (1.0f - c) * 0.5f / a0;
        b1 = (1.0f - c) / a0;
    }
    b2 = b0;
    a1 = -2.0f * c / a0;
    a2 = (1.0f - al) / a0;
}

void SidechainEngine::prepare(double sampleRate) noexcept {
    sr_ = static_cast<float>(sampleRate);
    reset();
}

void SidechainEngine::reset() noexcept {
    hp_.z1 = hp_.z2 = lp_.z1 = lp_.z2 = 0;
    lastLow_ = lastHigh_ = -1;
    env_ = 0;
    grDb_ = 0;
    compGain_ = 1;
    above_ = false;
    freeBeats_ = 0;
    scActivity_ = 0;
    graphCount_ = 0;
}

float SidechainEngine::compressorGainDb(float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept {
    const float slope = 1.0f - 1.0f / std::max(1.0f, ratio);
    const float over = levelDb - thresholdDb;
    const float k = std::max(0.0f, kneeDb);
    if (k <= 0.0f || over >= k * 0.5f) return over > 0.0f ? -over * slope : 0.0f;
    if (over <= -k * 0.5f) return 0.0f;
    const float t = over + k * 0.5f;
    return -slope * t * t / (2.0f * k);
}

namespace {
struct Division {
    const char* name;
    double beats;
};
constexpr Division kDivisions[SidechainEngine::kNumDivisions] = {
    {"1/1", 4.0}, {"1/2", 2.0}, {"1/4", 1.0}, {"1/8", 0.5}, {"1/16", 0.25}, {"1/32", 0.125},
    {"1/4 T", 2.0 / 3.0}, {"1/8 T", 1.0 / 3.0}, {"1/16 T", 1.0 / 6.0}, {"1/8 D", 0.75},
};
constexpr const char* kShapes[SidechainEngine::kNumShapes] = {"Classic", "Smooth", "Punchy", "Long", "Cut", "Bounce", "Swell", "Breathe"};
}  // namespace

double SidechainEngine::divisionBeats(int rate) noexcept { return kDivisions[std::clamp(rate, 0, kNumDivisions - 1)].beats; }
const char* SidechainEngine::divisionName(int rate) noexcept { return kDivisions[std::clamp(rate, 0, kNumDivisions - 1)].name; }
const char* SidechainEngine::shapeName(int shape) noexcept { return kShapes[std::clamp(shape, 0, kNumShapes - 1)]; }

// Tempo curves. Every curve that starts fully ducked fades in over at least 3 ms (no click) and is
// back at full level before the next hit, so each beat starts from the same place.
float SidechainEngine::shapeAmount(int shape, double f, double t, double length, const SidechainSettings& s) noexcept {
    const double att = std::max(0.003, s.attackMs * 0.001);
    const double ramp = t < att ? 0.5 - 0.5 * std::cos(kPi * t / att) : 1.0;
    f = std::clamp(f, 0.0, 1.0);
    double v;
    switch (shape) {
        case 1:  // Smooth: cosine back up over 70 % of the note
            v = f < 0.7 ? 0.5 + 0.5 * std::cos(kPi * f / 0.7) : 0.0;
            break;
        case 2:  // Punchy: deep and short, fast exponential recovery
            v = std::exp(-f * 10.0) * (1.0 - f);
            break;
        case 3:  // Long: comes back slowly across the whole note
            v = std::pow(1.0 - f, 1.6);
            break;
        case 4:  // Cut: silent, then snaps back (gate)
            v = f < 0.35 ? 1.0 : (f < 0.42 ? 0.5 + 0.5 * std::cos(kPi * (f - 0.35) / 0.07) : 0.0);
            break;
        case 5:  // Bounce: the dip, then a small second one
            v = std::max(std::exp(-f * 7.0) * (1.0 - f), 0.4 * std::exp(-std::pow((f - 0.5) * 9.0, 2.0)));
            break;
        case 6:  // Swell: straight line back up, like a reverse fade
            v = 1.0 - f;
            break;
        case 7:  // Breathe: one smooth wave per note, no hard edge at all
            return static_cast<float>(0.5 + 0.5 * std::cos(2.0 * kPi * f));
        default: {  // Classic: attack, then the release time (never longer than the note)
            const double r = (t - att) / std::max(0.01, std::min(s.releaseMs * 0.001, length));
            v = t < att ? 1.0 : (r >= 1.0 ? 0.0 : (1.0 - r) * (1.0 - r));
            break;
        }
    }
    return static_cast<float>(v * ramp);
}

int SidechainEngine::process(float* l, float* r, const float* scL, const float* scR, int n, const SidechainSettings& s, const Transport& t,
                             GraphPoint* graph, int maxPoints) noexcept {
    if (s.scLowCutHz != lastLow_) {
        hp_.set(sr_, s.scLowCutHz, true);
        lastLow_ = s.scLowCutHz;
    }
    if (s.scHighCutHz != lastHigh_) {
        lp_.set(sr_, s.scHighCutHz, false);
        lastHigh_ = s.scHighCutHz;
    }
    const float att = std::exp(-1.0f / (std::max(0.05f, s.attackMs) * 0.001f * sr_));
    const float rel = std::exp(-1.0f / (std::max(1.0f, s.releaseMs) * 0.001f * sr_));
    // The gain comes back almost fully (95 %) within the release time.
    const float relGr = std::exp(-3.0f / (std::max(1.0f, s.releaseMs) * 0.001f * sr_));
    const float outTarget = fromDb(s.outputDb);
    const float outStep = 1.0f - std::exp(-1.0f / (0.02f * sr_));
    const double bpm = t.bpm > 1.0 ? t.bpm : 120.0;
    const double beatsPerSample = bpm / 60.0 / sr_;
    const double div = divisionBeats(s.rate);
    const double secPerBeat = 60.0 / bpm;
    const float pump = std::clamp(s.pump, 0.0f, 1.0f);
    const float thrLin = fromDb(s.thresholdDb);

    // Decide the trigger for this block.
    const bool hasSc = scL != nullptr;
    bool useSc = s.trigger == SidechainSettings::Sidechain;
    if (s.trigger == SidechainSettings::Auto) useSc = hasSc && scActivity_ > 0.0f;
    usingSc_ = useSc;

    float minG = 1.0f, maxEnv = 0.0f;
    int points = 0;
    for (int i = 0; i < n; ++i) {
        // Sidechain detector (always runs so Auto can notice a sidechain appearing).
        float sc = 0.0f;
        if (hasSc) sc = 0.5f * (scL[i] + (scR ? scR[i] : scL[i]));
        const float filtered = lp_.tick(hp_.tick(sc));
        const float a = std::abs(filtered);
        env_ = a > env_ ? a + (env_ - a) * att : a + (env_ - a) * rel;
        maxEnv = std::max(maxEnv, env_);
        if (std::abs(sc) > 1e-4f) scActivity_ = 0.5f;  // stay in sidechain mode 0.5 s after the last sound
        else scActivity_ = std::max(0.0f, scActivity_ - 1.0f / sr_);

        float gain, trig;
        if (useSc) {
            // Like a classic compressor: the gain reduction itself moves with attack and release,
            // so it does not follow each cycle of the kick (that would distort the bass).
            const float target = compressorGainDb(toDb(a), s.thresholdDb, s.ratio, s.kneeDb);
            grDb_ = target < grDb_ ? target + (grDb_ - target) * att : target + (grDb_ - target) * relGr;
            compGain_ = fromDb(grDb_);
            gain = 1.0f - pump * (1.0f - compGain_);
            trig = std::min(1.0f, env_ * 2.0f);
            // Count a hit when the envelope crosses the threshold upwards (with 3 dB hysteresis).
            if (!above_ && env_ > thrLin) {
                above_ = true;
                ++triggers_;
            } else if (above_ && env_ < thrLin * 0.7f) {
                above_ = false;
            }
        } else {
            const double beats = (t.playing ? t.ppq : freeBeats_) + static_cast<double>(i) * beatsPerSample;
            const double phase = std::fmod(beats, div);
            if (phase < lastBeatPhase_) ++triggers_;  // wrapped: a new beat started
            lastBeatPhase_ = phase;
            const float amt = shapeAmount(s.shape, phase / div, phase * secPerBeat, div * secPerBeat, s);
            gain = 1.0f - pump * amt * 0.985f;  // keep a hint of sound at full pump (-36 dB)
            trig = amt;
        }
        minG = std::min(minG, gain);

        outGain_ += (outTarget - outGain_) * outStep;
        if (s.listenSidechain) {
            l[i] = filtered * outGain_;
            if (r) r[i] = filtered * outGain_;
        } else {
            l[i] *= gain * outGain_;
            if (r) r[i] *= gain * outGain_;
        }

        // Graph: one point every kGraphStep samples (loudest trigger, lowest gain in between).
        graphTrig_ = std::max(graphTrig_, trig);
        graphGain_ = std::min(graphGain_, gain);
        if (++graphCount_ >= kGraphStep) {
            if (graph && points < maxPoints) graph[points++] = {graphTrig_, graphGain_};
            graphCount_ = 0;
            graphTrig_ = 0;
            graphGain_ = 1;
        }
    }
    if (!t.playing) freeBeats_ = std::fmod(freeBeats_ + n * beatsPerSample, 64.0);
    minGain_ = minG;
    triggerDb_ = toDb(maxEnv);
    return points;
}

}  // namespace pat::dsp
