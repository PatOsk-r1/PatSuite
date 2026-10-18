#include "dsp/doubler/DoublerEngine.h"

#include <algorithm>

namespace pat::dsp {
namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kWindowMs = 24.0f;       // pitch-shifter window
constexpr float kMaxVariationMs = 4.0f;
}  // namespace

void DoublerEngine::prepare(double sampleRate) {
    sr_ = static_cast<float>(sampleRate);
    // Longest read: 80 ms delay + 2 x 4 ms drift + 24 ms window.
    const int need = static_cast<int>(sr_ * 0.14f) + 8;
    int size = 1;
    while (size < need) size <<= 1;
    line_.assign(static_cast<std::size_t>(size), 0.0f);
    mask_ = size - 1;
    reset();
}

void DoublerEngine::reset() noexcept {
    std::fill(line_.begin(), line_.end(), 0.0f);
    pos_ = 0;
    for (int c = 0; c < DoublerSettings::kMaxCopies; ++c) {
        phase_[static_cast<std::size_t>(c)] = 0.0f;  // without detune the copy reads one steady tap
        drift_[static_cast<std::size_t>(c)] = driftTarget_[static_cast<std::size_t>(c)] = 0;
    }
}

float DoublerEngine::copyGain(int copy, const DoublerSettings& s) noexcept {
    if (copy >= s.copies) return 0.0f;
    // Copies fade in one after the other as thickness goes up.
    const float t = std::clamp(s.thickness, 0.0f, 1.0f) * static_cast<float>(s.copies);
    return std::clamp(t - static_cast<float>(copy), 0.0f, 1.0f) * 0.7f;
}

float DoublerEngine::copyPan(int copy, const DoublerSettings& s) noexcept {
    return std::clamp(s.pan[static_cast<std::size_t>(copy)] * std::clamp(s.width, 0.0f, 1.0f) * s.spread, -1.0f, 1.0f);
}

float DoublerEngine::copyDetune(int copy, const DoublerSettings& s) noexcept {
    return s.detuneCents[static_cast<std::size_t>(copy)] * (0.5f + 0.5f * std::clamp(s.thickness, 0.0f, 1.0f));
}

float DoublerEngine::readDelay(float d) const noexcept {
    const float rp = static_cast<float>(pos_) - d;
    const float fl = std::floor(rp);
    const int i0 = static_cast<int>(fl) & mask_;
    const int i1 = (i0 + 1) & mask_;
    const float f = rp - fl;
    return line_[static_cast<std::size_t>(i0)] + (line_[static_cast<std::size_t>(i1)] - line_[static_cast<std::size_t>(i0)]) * f;
}

int DoublerEngine::process(float* l, float* r, int n, const DoublerSettings& s, StereoSample* scope, int maxScope) noexcept {
    const float window = kWindowMs * 0.001f * sr_;
    const float outTarget = std::pow(10.0f, s.outputDb * 0.05f);
    const float outStep = 1.0f - std::exp(-1.0f / (0.02f * sr_));
    const float driftStep = 1.0f - std::exp(-1.0f / (0.4f * sr_));  // drift glides over ~0.4 s

    // Per-copy constants for this block.
    std::array<float, DoublerSettings::kMaxCopies> gain{}, gl{}, gr{}, inc{}, base{};
    for (int c = 0; c < DoublerSettings::kMaxCopies; ++c) {
        const auto sc = static_cast<std::size_t>(c);
        gain[sc] = copyGain(c, s);
        const float a = (copyPan(c, s) + 1.0f) * 0.25f * kPi;  // constant-power pan
        gl[sc] = std::cos(a) * 1.4142f;
        gr[sc] = std::sin(a) * 1.4142f;
        const float ratio = std::exp2(copyDetune(c, s) / 1200.0f);
        inc[sc] = (1.0f - ratio) / window;  // delay sweep speed of the pitch shifter
        base[sc] = std::clamp(s.delayMs[sc], 1.0f, 80.0f) * 0.001f * sr_;
    }

    int written = 0;
    for (int i = 0; i < n; ++i) {
        const float dryL = l[i], dryR = r ? r[i] : dryL;
        line_[static_cast<std::size_t>(pos_)] = 0.5f * (dryL + dryR);
        pos_ = (pos_ + 1) & mask_;

        // New random drift targets every 50 ms (slow, smooth "human" timing).
        if (++driftCounter_ >= static_cast<int>(0.05f * sr_)) {
            driftCounter_ = 0;
            for (auto& t : driftTarget_) {
                rng_ = rng_ * 1664525u + 1013904223u;
                t = (static_cast<float>(rng_ >> 8) / 16777216.0f * 2.0f - 1.0f) * s.variation * kMaxVariationMs * 0.001f * sr_;
            }
        }

        float wetL = 0, wetR = 0;
        for (int c = 0; c < DoublerSettings::kMaxCopies; ++c) {
            const auto sc = static_cast<std::size_t>(c);
            drift_[sc] += (driftTarget_[sc] - drift_[sc]) * driftStep;
            if (gain[sc] <= 0.0f) continue;
            float& ph = phase_[sc];
            ph += inc[sc];
            ph -= std::floor(ph);
            const float ph2 = ph + 0.5f - std::floor(ph + 0.5f);
            const float d = base[sc] + drift_[sc] + kMaxVariationMs * 0.001f * sr_ + 1.0f;
            const float w1 = std::sin(kPi * ph), w2 = std::sin(kPi * ph2);
            const float y = readDelay(d + ph * window) * w1 * w1 + readDelay(d + ph2 * window) * w2 * w2;
            wetL += y * gain[sc] * gl[sc];
            wetR += y * gain[sc] * gr[sc];
        }
        outGain_ += (outTarget - outGain_) * outStep;
        const float outL = (dryL * s.dryGain + wetL) * outGain_;
        const float outR = (dryR * s.dryGain + wetR) * outGain_;
        if (r) {
            l[i] = outL;
            r[i] = outR;
        } else {
            l[i] = 0.5f * (outL + outR);
        }
        if (++scopeCount_ >= 4) {
            scopeCount_ = 0;
            if (scope && written < maxScope) scope[written++] = {outL, outR};
        }
    }
    return written;
}

}  // namespace pat::dsp
