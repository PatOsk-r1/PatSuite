#include "dsp/deesser/DeEsserEngine.h"

#include <algorithm>

namespace pat::dsp {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kKneeDb = 6.0f;
inline float toDb(float g) noexcept { return g > 1e-9f ? 20.0f * std::log10(g) : -180.0f; }
inline float fromDb(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
}  // namespace

void BandSplitter::setup(float sampleRate, float freqHz, float widthOct) noexcept {
    const float fc = std::clamp(freqHz, 20.0f, sampleRate * 0.45f);
    // Q from the bandwidth in octaves (band edges at -3 dB).
    const float bw = std::clamp(widthOct, 0.1f, 4.0f);
    const float p = std::exp2(bw);
    const float q = std::sqrt(p) / (p - 1.0f);
    const float g = std::tan(kPi * fc / sampleRate);
    k_ = 1.0f / q;
    a1_ = 1.0f / (1.0f + g * (g + k_));
    a2_ = g * a1_;
    a3_ = g * a2_;
}

void DeEsserEngine::prepare(double sampleRate) noexcept {
    sr_ = static_cast<float>(sampleRate);
    attack_ = std::exp(-1.0f / (0.0005f * sr_));
    reset();
}

void DeEsserEngine::reset() noexcept {
    detL_.reset();
    detR_.reset();
    sigL_.reset();
    sigR_.reset();
    env_ = 0;
    lastFreq_ = lastWidth_ = -1;
}

float DeEsserEngine::gainReductionDb(float levelDb, float thresholdDb, float ratio) noexcept {
    const float slope = 1.0f - 1.0f / std::max(1.0f, ratio);
    const float over = levelDb - thresholdDb;
    if (over <= -kKneeDb * 0.5f) return 0.0f;
    if (over >= kKneeDb * 0.5f) return -over * slope;
    const float t = over + kKneeDb * 0.5f;  // inside the soft knee
    return -slope * t * t / (2.0f * kKneeDb);
}

void DeEsserEngine::process(float* l, float* r, int n, const DeEsserSettings& s) noexcept {
    if (s.freqHz != lastFreq_ || s.widthOct != lastWidth_) {
        for (auto* f : {&detL_, &detR_, &sigL_, &sigR_}) f->setup(sr_, s.freqHz, s.widthOct);
        lastFreq_ = s.freqHz;
        lastWidth_ = s.widthOct;
    }
    const float release = std::exp(-1.0f / (std::max(1.0f, s.releaseMs) * 0.001f * sr_));
    const float outTarget = fromDb(s.outputDb);
    const float outStep = 1.0f - std::exp(-1.0f / (0.02f * sr_));  // 20 ms smoothing
    float detMax = 0, grMin = 0, inPk = 0, outPk = 0;

    for (int i = 0; i < n; ++i) {
        const float xl = l[i], xr = r ? r[i] : xl;
        inPk = std::max(inPk, std::max(std::abs(xl), std::abs(xr)));

        // Detector (stereo linked): the loudest side drives both.
        const float d = std::max(std::abs(detL_.band(xl)), std::abs(detR_.band(xr)));
        env_ = d > env_ ? d + (env_ - d) * attack_ : d + (env_ - d) * release;
        detMax = std::max(detMax, env_);
        const float grDb = gainReductionDb(toDb(env_), s.thresholdDb, s.ratio);
        grMin = std::min(grMin, grDb);
        const float g = fromDb(grDb);

        // Signal path.
        const float bl = sigL_.band(xl), br = sigR_.band(xr);
        float yl, yr;
        if (s.listen) {
            yl = bl;
            yr = br;
        } else if (s.split) {
            yl = xl + (g - 1.0f) * bl;
            yr = xr + (g - 1.0f) * br;
        } else {
            yl = xl * g;
            yr = xr * g;
        }
        outGain_ += (outTarget - outGain_) * outStep;
        yl *= outGain_;
        yr *= outGain_;
        l[i] = yl;
        if (r) r[i] = yr;
        outPk = std::max(outPk, std::max(std::abs(yl), std::abs(yr)));
    }
    detDb_ = toDb(detMax);
    grDb_ = grMin;
    inPeak_ = inPk;
    outPeak_ = outPk;
}

}  // namespace pat::dsp
