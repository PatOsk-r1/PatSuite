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

// Ducking shape after each beat: fast dip (attack), smooth recovery (release, never longer than
// the beat division so the next hit always starts from full level).
float SidechainEngine::tempoAmount(double t, const SidechainSettings& s) const noexcept {
    const double att = std::max(0.0005, s.attackMs * 0.001);
    if (t < att) return static_cast<float>(t / att);
    const double r = (t - att) / std::max(0.01, s.releaseMs * 0.001);
    if (r >= 1.0) return 0.0f;
    const double v = 1.0 - r;
    return static_cast<float>(v * v);
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
            compGain_ = fromDb(compressorGainDb(toDb(env_), s.thresholdDb, s.ratio, s.kneeDb));
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
            const float amt = tempoAmount(phase * secPerBeat, s);
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
