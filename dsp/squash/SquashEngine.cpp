#include "dsp/squash/SquashEngine.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
inline float toDb(float g) noexcept { return 10.0f * std::log10(g + 1e-12f); }  // from power
inline float fromDb(float db) noexcept { return std::pow(10.0f, db * 0.05f); }
}  // namespace

void SquashEngine::design(Biquad& f, double fs, double hz, bool highpass) noexcept {
    const double w = 2.0 * kPi * std::clamp(hz, 10.0, fs * 0.45) / fs;
    const double c = std::cos(w), al = std::sin(w) / (2.0 * 0.70710678), a0 = 1.0 + al;
    f.b0 = (highpass ? (1.0 + c) : (1.0 - c)) * 0.5 / a0;
    f.b1 = (highpass ? -(1.0 + c) : (1.0 - c)) / a0;
    f.b2 = f.b0;
    f.a1 = -2.0 * c / a0;
    f.a2 = (1.0 - al) / a0;
}

void SquashEngine::designAllpass(Biquad& f, double fs, double hz) noexcept {
    const double w = 2.0 * kPi * std::clamp(hz, 10.0, fs * 0.45) / fs;
    const double c = std::cos(w), al = std::sin(w) / (2.0 * 0.70710678), a0 = 1.0 + al;
    f.b0 = (1.0 - al) / a0;
    f.b1 = -2.0 * c / a0;
    f.b2 = 1.0;
    f.a1 = -2.0 * c / a0;
    f.a2 = (1.0 - al) / a0;
}

void SquashEngine::prepare(double sampleRate) noexcept {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    lastLo_ = lastHi_ = -1;
    reset();
}

void SquashEngine::reset() noexcept {
    for (auto* f : {&lp1_[0], &lp1_[1], &hp1_[0], &hp1_[1], &lp2_[0], &lp2_[1], &hp2_[0], &hp2_[1], &ap2_})
        for (auto& ch : f->z) ch[0] = ch[1] = 0.0;
    env_.fill(0.0f);
    gr_.fill(0.0f);
    levelDb_.fill(-120.0f);
    shownGain_.fill(0.0f);
}

void SquashEngine::updateCrossovers(float lo, float hi) noexcept {
    hi = std::max(hi, lo * 1.5f);
    if (lo == lastLo_ && hi == lastHi_) return;
    for (int k = 0; k < 2; ++k) {
        design(lp1_[k], sr_, lo, false);
        design(hp1_[k], sr_, lo, true);
        design(lp2_[k], sr_, hi, false);
        design(hp2_[k], sr_, hi, true);
    }
    designAllpass(ap2_, sr_, hi);
    lastLo_ = lo;
    lastHi_ = hi;
}

float SquashEngine::gainDb(float levelDb, const SquashBand& b, float upward, float downward) noexcept {
    float g = 0.0f;
    if (levelDb > b.downThresholdDb) g -= (levelDb - b.downThresholdDb) * (1.0f - 1.0f / std::max(1.0f, b.downRatio)) * downward;
    if (levelDb < b.upThresholdDb && levelDb > kFloorDb) {
        // Fades out toward the floor so near-silence is left alone.
        const float fade = std::clamp((levelDb - kFloorDb) / 12.0f, 0.0f, 1.0f);
        g += std::min(kMaxUpDb, (b.upThresholdDb - levelDb) * (1.0f - 1.0f / std::max(1.0f, b.upRatio))) * upward * fade;
    }
    return g;
}

void SquashEngine::process(float* l, float* r, int n, const SquashSettings& s) noexcept {
    updateCrossovers(s.lowCrossHz, s.highCrossHz);
    const float time = std::clamp(s.time, 0.1f, 4.0f);
    // Lows move slower than highs (they have longer waves).
    const float attMs[3] = {12.0f, 6.0f, 3.0f}, relMs[3] = {220.0f, 140.0f, 90.0f};
    // Level: true RMS over a short window (longer for the lows), then the gain moves with
    // attack / release.
    const float rmsMs[3] = {20.0f, 6.0f, 3.0f};
    float att[3], rel[3], rms[3];
    for (int b = 0; b < 3; ++b) {
        rms[b] = 1.0f - std::exp(-1.0f / (rmsMs[b] * 0.001f * static_cast<float>(sr_)));
        att[b] = 1.0f - std::exp(-1.0f / (attMs[b] * time * 0.001f * static_cast<float>(sr_)));
        rel[b] = 1.0f - std::exp(-1.0f / (relMs[b] * time * 0.001f * static_cast<float>(sr_)));
    }
    const float depth = std::clamp(s.depth, 0.0f, 1.0f);
    const float inT = fromDb(s.inputDb), outT = fromDb(s.outputDb);
    const float smooth = 1.0f - std::exp(-1.0f / (0.02f * static_cast<float>(sr_)));
    float bandGain[3];
    for (int b = 0; b < 3; ++b) bandGain[b] = fromDb(s.band[static_cast<std::size_t>(b)].gainDb);
    float peak[3] = {-120.0f, -120.0f, -120.0f}, grSum[3] = {0, 0, 0};

    for (int i = 0; i < n; ++i) {
        inG_ += smooth * (inT - inG_);
        outG_ += smooth * (outT - outG_);
        double x[2] = {l[i] * inG_, (r ? r[i] : l[i]) * inG_};
        double band[3][2];
        for (int c = 0; c < 2; ++c) {
            const double lo = lp1_[1].tick(lp1_[0].tick(x[c], c), c);
            const double rest = hp1_[1].tick(hp1_[0].tick(x[c], c), c);
            band[0][c] = ap2_.tick(lo, c);
            band[1][c] = lp2_[1].tick(lp2_[0].tick(rest, c), c);
            band[2][c] = hp2_[1].tick(hp2_[0].tick(rest, c), c);
        }
        double out[2] = {0, 0};
        for (int b = 0; b < 3; ++b) {
            const auto& bs = s.band[static_cast<std::size_t>(b)];
            const float p = static_cast<float>(0.5 * (band[b][0] * band[b][0] + band[b][1] * band[b][1]));
            auto& e = env_[static_cast<std::size_t>(b)];
            e += rms[b] * (p - e);
            const float lvl = toDb(e * 2.0f);  // RMS of a sine reads its peak level
            auto& gr = gr_[static_cast<std::size_t>(b)];
            const float target = gainDb(lvl, bs, s.upward, s.downward);
            gr += (target < gr ? att[b] : rel[b]) * (target - gr);  // turning down = attack
            const float g = fromDb(gr) * bandGain[b];
            const float mixG = 1.0f + depth * (g - 1.0f);
            out[0] += band[b][0] * mixG;
            out[1] += band[b][1] * mixG;
            peak[b] = std::max(peak[b], lvl);
            grSum[b] += 20.0f * std::log10(std::max(1e-6f, mixG));
        }
        l[i] = static_cast<float>(out[0]) * outG_;
        if (r) r[i] = static_cast<float>(out[1]) * outG_;
    }
    for (int b = 0; b < 3; ++b) {
        levelDb_[static_cast<std::size_t>(b)] = peak[b];
        shownGain_[static_cast<std::size_t>(b)] = n > 0 ? grSum[b] / static_cast<float>(n) : 0.0f;
    }
}

}  // namespace pat::dsp
