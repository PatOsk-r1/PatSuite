#include "dsp/master/LimiterEngine.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

void LimiterEngine::prepare(double sampleRate) {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    lookahead_ = std::max(1, static_cast<int>(std::lround(kLookaheadMs * 0.001 * sr_)));
    delayLen_ = lookahead_ + 3 + 1;
    for (int c = 0; c < 2; ++c) {
        delay_[c].assign(static_cast<std::size_t>(delayLen_), 0.0f);
        req_[c].assign(static_cast<std::size_t>(lookahead_), 1.0f);
        box_[c].assign(static_cast<std::size_t>(lookahead_), 1.0f);
    }
    reset();
}

void LimiterEngine::reset() noexcept {
    for (int c = 0; c < 2; ++c) {
        std::fill(delay_[c].begin(), delay_[c].end(), 0.0f);
        std::fill(req_[c].begin(), req_[c].end(), 1.0f);
        std::fill(box_[c].begin(), box_[c].end(), 1.0f);
        boxSum_[c] = static_cast<double>(lookahead_);
        env_[c] = 1.0f;
        tp_[c] = {};
    }
    delayPos_ = reqPos_ = 0;
    grAvg_ = 0;
    grDb_ = 0;
}

float LimiterEngine::softClip(float x, float amount) const noexcept {
    if (amount <= 0.0f) return x;
    return x + amount * (std::tanh(x) - x);
}

void LimiterEngine::process(float* l, float* r, int n, const LimiterSettings& s) noexcept {
    if (delay_[0].empty()) return;
    const float inTarget = std::pow(10.0f, s.gainDb / 20.0f);
    const float ceiling = std::pow(10.0f, std::min(0.0f, s.ceilingDb) / 20.0f);
    const float smooth = 1.0f - std::exp(-1.0f / (0.02f * static_cast<float>(sr_)));
    const float grAvgCoef = 1.0f - std::exp(-1.0f / (1.0f * static_cast<float>(sr_)));
    const float link = std::clamp(s.link, 0.0f, 1.0f);
    const float clip = std::clamp(s.clip, 0.0f, 1.0f);
    const int chans = r ? 2 : 1;
    float minG = 1.0f;

    for (int i = 0; i < n; ++i) {
        inG_ += smooth * (inTarget - inG_);
        float x[2] = {softClip(l[i] * inG_, clip), r ? softClip(r[i] * inG_, clip) : 0.0f};

        // Peak of each side (true peak: also between samples; both paths are 4 samples late).
        float peak[2] = {0, 0};
        for (int c = 0; c < chans; ++c) {
            const float tp = tp_[c].push(x[c]);
            peak[c] = s.truePeak ? tp : std::abs(tp_[c].hist[static_cast<std::size_t>((tp_[c].pos + 3) & 7)]);
        }
        // Gain each side needs so its peak stays under the ceiling; linked = share the deepest.
        float need[2];
        for (int c = 0; c < chans; ++c) need[c] = peak[c] > ceiling ? ceiling / peak[c] : 1.0f;
        if (chans == 2 && link > 0.0f) {
            const float both = std::min(need[0], need[1]);
            for (float& v : need) v = std::pow(v, 1.0f - link) * std::pow(both, link);
        }

        // Release time: automatic = quick for single peaks, slower when it limits all the time.
        const float relMs = s.autoRelease ? 30.0f + 600.0f * std::clamp(grAvg_ / 8.0f, 0.0f, 1.0f) : std::max(1.0f, s.releaseMs);
        const float rel = 1.0f - std::exp(-1.0f / (relMs * 0.001f * static_cast<float>(sr_)));

        float g[2] = {1, 1};
        for (int c = 0; c < chans; ++c) {
            req_[c][static_cast<std::size_t>(reqPos_)] = need[c];
            float held = 1.0f;  // deepest need over the look-ahead window
            for (float v : req_[c]) held = std::min(held, v);
            env_[c] = held < env_[c] ? held : env_[c] + rel * (held - env_[c]);
            boxSum_[c] += env_[c] - box_[c][static_cast<std::size_t>(reqPos_)];
            box_[c][static_cast<std::size_t>(reqPos_)] = env_[c];
            g[c] = static_cast<float>(boxSum_[c] / lookahead_);
        }
        reqPos_ = (reqPos_ + 1) % lookahead_;

        // Delayed audio meets its gain; a final clip guards the ceiling.
        const int outPos = (delayPos_ + 1) % delayLen_;  // oldest sample
        for (int c = 0; c < chans; ++c) {
            delay_[c][static_cast<std::size_t>(delayPos_)] = x[c];
            const float y = std::clamp(delay_[c][static_cast<std::size_t>(outPos)] * g[c], -ceiling, ceiling);
            if (c == 0) l[i] = y;
            else r[i] = y;
            minG = std::min(minG, g[c]);
        }
        delayPos_ = outPos;
        const float grNow = -20.0f * std::log10(std::max(1e-6f, std::min(g[0], chans == 2 ? g[1] : g[0])));
        grAvg_ += grAvgCoef * (grNow - grAvg_);
    }
    grDb_ = 20.0f * std::log10(std::max(1e-6f, minG));
}

}  // namespace pat::dsp
