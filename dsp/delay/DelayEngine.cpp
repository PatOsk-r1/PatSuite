#include "dsp/delay/DelayEngine.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kTwoPi = 6.283185307179586;
struct Division {
    const char* name;
    double beats;
};
constexpr Division kDivisions[DelayEngine::kNumDivisions] = {
    {"1/64", 0.0625},    {"1/32 tri", 1.0 / 12.0}, {"1/32", 0.125},  {"1/16 tri", 1.0 / 6.0}, {"1/16", 0.25},   {"1/16 dot", 0.375},
    {"1/8 tri", 1.0 / 3.0}, {"1/8", 0.5},          {"1/8 dot", 0.75}, {"1/4 tri", 2.0 / 3.0}, {"1/4", 1.0},    {"1/4 dot", 1.5},
    {"1/2 tri", 4.0 / 3.0}, {"1/2", 2.0},          {"1/2 dot", 3.0}, {"1 bar", 4.0},
};
constexpr float kDiffusionMs[4] = {4.1f, 6.7f, 9.3f, 13.1f};

float coef(double sr, float hz) noexcept { return 1.0f - std::exp(static_cast<float>(-kTwoPi * hz / sr)); }
}  // namespace

const char* DelayEngine::divisionName(int i) noexcept { return kDivisions[std::clamp(i, 0, kNumDivisions - 1)].name; }
double DelayEngine::divisionBeats(int i) noexcept { return kDivisions[std::clamp(i, 0, kNumDivisions - 1)].beats; }

double DelayEngine::timeSeconds(const DelaySettings& s, bool right, double bpm) noexcept {
    const bool useRight = right && !s.link;
    double t;
    if (s.sync) t = divisionBeats(useRight ? s.divisionR : s.divisionL) * 60.0 / std::max(20.0, bpm);
    else t = (useRight ? s.timeRMs : s.timeLMs) * 0.001;
    return std::clamp(t, 0.001, kMaxSeconds);
}

int DelayEngine::taps(const DelaySettings& s, double bpm, DelayTap* out, int maxTaps) noexcept {
    const double tl = timeSeconds(s, false, bpm), tr = timeSeconds(s, true, bpm);
    const float fb = s.freeze ? 1.0f : std::clamp(s.feedback, 0.0f, 1.0f);
    int n = 0;
    const auto add = [&](double t, float l, float r) {
        if (n < maxTaps) out[n++] = {t, l, r};
    };
    const int repeats = maxTaps;
    float level = 1.0f;
    switch (s.mode) {
        case DelaySettings::Mono:
            for (int k = 1; k <= repeats && level > 0.01f; ++k, level *= fb) add(k * tl, level, level);
            break;
        case DelaySettings::Stereo: {
            float ll = 1.0f, lr = 1.0f;
            int kl = 1, kr = 1;
            while (n < maxTaps && (ll > 0.01f || lr > 0.01f)) {
                const double nextL = kl * tl, nextR = kr * tr;
                if (ll > 0.01f && (nextL <= nextR || lr <= 0.01f)) {
                    add(nextL, ll, 0.0f);
                    ll *= fb;
                    ++kl;
                } else {
                    add(nextR, 0.0f, lr);
                    lr *= fb;
                    ++kr;
                }
            }
            break;
        }
        default: {
            // Ping-Pong: the sum starts left. Cross: each side starts on its own side, so a
            // click on both inputs lands on both sides with the two times interleaved.
            double t = 0.0;
            bool left = true;
            for (int k = 0; k < repeats && level > 0.01f; ++k, level *= fb) {
                t += left ? tl : tr;
                add(t, left ? level : 0.0f, left ? 0.0f : level);
                left = !left;
            }
            if (s.mode == DelaySettings::Cross) {  // the right input's own path
                t = 0.0;
                left = false;
                level = 1.0f;
                for (int k = 0; k < repeats && level > 0.01f; ++k, level *= fb) {
                    t += left ? tl : tr;
                    add(t, left ? level : 0.0f, left ? 0.0f : level);
                    left = !left;
                }
                std::sort(out, out + n, [](const DelayTap& a, const DelayTap& b) { return a.seconds < b.seconds; });
            }
            break;
        }
    }
    return n;
}

void DelayEngine::Line::push(float x) noexcept {
    buf[static_cast<std::size_t>(write)] = x;
    if (++write >= static_cast<int>(buf.size())) write = 0;
}

float DelayEngine::Line::read(double d) const noexcept {
    const int size = static_cast<int>(buf.size());
    double pos = write - d;
    while (pos < 0) pos += size;
    const int i = static_cast<int>(pos);
    const float f = static_cast<float>(pos - i);
    const auto at = [&](int k) { return buf[static_cast<std::size_t>((k % size + size) % size)]; };
    const float x0 = at(i - 1), x1 = at(i), x2 = at(i + 1), x3 = at(i + 2);
    // 4-point Hermite.
    const float c1 = 0.5f * (x2 - x0);
    const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
    const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * f + c2) * f + c1) * f + x1;
}

float DelayEngine::Allpass::run(float x, float g) noexcept {
    const float v = buf[idx];
    const float w = x + g * v;
    buf[idx] = w;
    if (++idx >= len) idx = 0;
    return v - g * w;
}

void DelayEngine::prepare(double sampleRate) {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    const auto size = static_cast<std::size_t>(sr_ * kMaxSeconds + 1024);
    for (auto& l : line_) l.buf.assign(size, 0.0f);
    for (auto& ch : diff_)
        for (int k = 0; k < 4; ++k) ch[k].len = std::clamp(static_cast<int>(kDiffusionMs[k] * 0.001 * sr_), 1, 2047);
    reset();
}

void DelayEngine::reset() noexcept {
    for (auto& l : line_) {
        std::fill(l.buf.begin(), l.buf.end(), 0.0f);
        l.write = 0;
    }
    for (int c = 0; c < 2; ++c) {
        lowpass_[c] = lowpass2_[c] = highpass_[c] = {};
        for (auto& a : diff_[c]) {
            std::fill(std::begin(a.buf), std::end(a.buf), 0.0f);
            a.idx = 0;
        }
        crushHold_[c] = 0;
    }
    lfo_ = wow_ = crushPhase_ = 0;
    inEnv_ = duckEnv_ = 0;
    duckGain_ = 1;
    peakL_ = peakR_ = 0;
    first_ = true;
}

float DelayEngine::shape(float x, float k) const noexcept { return std::tanh(k * x) / k; }

float DelayEngine::loopTone(float x, int ch, const DelaySettings& s, float lpA, float hpA) noexcept {
    x -= highpass_[ch].lp(x, hpA);
    x = lowpass2_[ch].lp(lowpass_[ch].lp(x, lpA), lpA);
    float k = 1.0f + 3.0f * s.drive;
    if (s.character == DelaySettings::Tape) k += 0.8f;
    else if (s.character == DelaySettings::Analog) k += 0.5f;
    else if (s.character == DelaySettings::LoFi) {
        // Crushed: 8 kHz sample-and-hold, 7-bit steps.
        if (ch == 0) crushPhase_ += 8000.0 / sr_;
        if (crushPhase_ >= 1.0) {
            crushHold_[ch] = std::round(x * 64.0f) / 64.0f;
            if (ch == 1) crushPhase_ -= 1.0;
        }
        x = crushHold_[ch];
    }
    return shape(x, k);
}

void DelayEngine::process(float* l, float* r, int n, const DelaySettings& s, double bpm) noexcept {
    if (n <= 0 || line_[0].buf.empty()) return;
    const bool mono = s.mode == DelaySettings::Mono;
    const bool pingpong = s.mode == DelaySettings::PingPong || s.mode == DelaySettings::Cross;
    const double target[2] = {std::max(3.0, timeSeconds(s, false, bpm) * sr_), std::max(3.0, timeSeconds(s, true, bpm) * sr_)};
    if (first_) {
        delay_[0] = target[0];
        delay_[1] = target[1];
        first_ = false;
    }
    const double glide = 1.0 - std::exp(-1.0 / (sr_ * 0.06));  // time changes glide like tape
    const float fb = s.freeze ? 1.0f : std::clamp(s.feedback, 0.0f, 1.0f);
    float hi = s.highCutHz, lo = s.lowCutHz;
    if (s.character == DelaySettings::Analog) hi = std::min(hi, 4500.0f);
    else if (s.character == DelaySettings::Tape) hi = std::min(hi, 10000.0f);
    else if (s.character == DelaySettings::LoFi) {
        hi = std::min(hi, 3500.0f);
        lo = std::max(lo, 300.0f);
    }
    const float lpA = coef(sr_, std::min(hi, static_cast<float>(sr_ * 0.45)));
    const float hpA = coef(sr_, lo);
    const float envAtt = coef(sr_, 300.0f), envRel = coef(sr_, 1.0f);
    const float duckAtt = coef(sr_, 25.0f), duckRel = coef(sr_, 0.8f);
    const double lfoInc = s.modRateHz / sr_, wowInc = 0.55 / sr_;
    const double modSamples = s.modDepth * 0.003 * sr_;
    const bool tape = s.character == DelaySettings::Tape;
    const float diffusion = std::clamp(s.diffusion, 0.0f, 1.0f);
    const float dryG = std::min(1.0f, 2.0f * (1.0f - s.mix)), wetG = std::min(1.0f, 2.0f * s.mix);
    const float out = std::pow(10.0f, s.outputDb / 20.0f);
    const float width = std::clamp(s.width, 0.0f, 2.0f);
    float pkL = 0, pkR = 0;

    for (int i = 0; i < n; ++i) {
        const float dl = l[i];
        const float dr = r ? r[i] : dl;
        const float a = std::max(std::abs(dl), std::abs(dr));
        inEnv_ += (a > inEnv_ ? envAtt : envRel) * (a - inEnv_);
        duckEnv_ += (inEnv_ > duckEnv_ ? duckAtt : duckRel) * (inEnv_ - duckEnv_);
        duckGain_ = 1.0f - s.duck * std::min(1.0f, duckEnv_ * 4.0f);

        // Echo line inputs.
        float in[2];
        if (mono || s.mode == DelaySettings::PingPong) {
            in[0] = 0.5f * (dl + dr);
            in[1] = 0.0f;
        } else {
            in[0] = dl;
            in[1] = dr;
        }
        if (diffusion > 0.0f)
            for (int c = 0; c < 2; ++c) {
                float x = in[c];
                for (auto& ap : diff_[c]) x = ap.run(x, 0.6f);
                in[c] += diffusion * (x - in[c]);
            }

        // Read the echoes (with modulation and, for Tape, wow and flutter).
        double mod[2] = {0, 0};
        if (modSamples > 0.0) {
            mod[0] = modSamples * (1.0 + std::sin(kTwoPi * lfo_));
            mod[1] = modSamples * (1.0 + std::cos(kTwoPi * lfo_));
        }
        if (tape) {
            const double w = 0.0008 * sr_ * (1.0 + std::sin(kTwoPi * wow_)) + 0.00008 * sr_ * (1.0 + std::sin(kTwoPi * wow_ * 12.7));
            mod[0] += w;
            mod[1] += w;
        }
        lfo_ += lfoInc;
        lfo_ -= std::floor(lfo_);
        wow_ += wowInc;
        if (wow_ >= 1.0) wow_ -= 1.0;
        float y[2];
        for (int c = 0; c < 2; ++c) {
            delay_[c] += glide * (target[c] - delay_[c]);
            y[c] = line_[c].read(delay_[c] + mod[c]);
        }
        if (mono) y[1] = 0.0f;

        // Feedback: same side, or the other side for ping-pong / cross.
        for (int c = 0; c < 2; ++c) {
            const float back = mono ? (c == 0 ? y[0] : 0.0f) : (pingpong ? y[1 - c] : y[c]);
            float x = (s.freeze ? 0.0f : in[c]) + fb * back;
            if (!s.freeze) x = loopTone(x, c, s, lpA, hpA);
            line_[c].push(x);
        }

        float wl = y[0], wr = mono ? y[0] : y[1];
        const float m = 0.5f * (wl + wr), sd = 0.5f * (wl - wr) * width;
        wl = (m + sd) * duckGain_;
        wr = (m - sd) * duckGain_;
        const float ol = (dl * dryG + wl * wetG) * out;
        const float orr = (dr * dryG + wr * wetG) * out;
        l[i] = r ? ol : 0.5f * (ol + orr);
        if (r) r[i] = orr;
        pkL = std::max(pkL, std::abs(ol));
        pkR = std::max(pkR, std::abs(orr));
    }
    peakL_ = pkL;
    peakR_ = pkR;
}

}  // namespace pat::dsp
