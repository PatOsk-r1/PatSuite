#include "dsp/pan/PanEngine.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr float kHalfPi = 1.57079632679f;
struct Division {
    const char* name;
    double beats;
};
constexpr Division kDivisions[PanEngine::kNumDivisions] = {
    {"4 bars", 16.0}, {"2 bars", 8.0},    {"1 bar", 4.0},        {"1/2", 2.0},    {"1/2 dot", 3.0},   {"1/2 tri", 4.0 / 3.0},
    {"1/4", 1.0},     {"1/4 dot", 1.5},   {"1/4 tri", 2.0 / 3.0}, {"1/8", 0.5},    {"1/8 dot", 0.75},  {"1/8 tri", 1.0 / 3.0},
    {"1/16", 0.25},   {"1/16 dot", 0.375}, {"1/16 tri", 1.0 / 6.0}, {"1/32", 0.125},
};
}  // namespace

const char* PanEngine::divisionName(int i) noexcept { return kDivisions[std::clamp(i, 0, kNumDivisions - 1)].name; }
double PanEngine::divisionBeats(int i) noexcept { return kDivisions[std::clamp(i, 0, kNumDivisions - 1)].beats; }

void PanEngine::prepare(double sampleRate) noexcept {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    reset();
}

void PanEngine::reset() noexcept {
    phase_ = 0.0;
    value_ = 0.0f;
    peakL_ = peakR_ = 0.0f;
}

void PanEngine::panGains(float pos, int law, float& gl, float& gr) noexcept {
    pos = std::clamp(pos, -1.0f, 1.0f);
    if (law == PanSettings::Balance) {
        // Stereo balance: the centre is untouched, one side fades out.
        gl = pos > 0 ? 1.0f - pos : 1.0f;
        gr = pos < 0 ? 1.0f + pos : 1.0f;
        return;
    }
    // Pan laws: sides at full level, the centre lowered by 3, 4.5 or 6 dB.
    const float a = (pos + 1.0f) * 0.5f;  // 0..1
    const float cosL = std::cos(a * kHalfPi), sinR = std::sin(a * kHalfPi);
    if (law == PanSettings::Law3dB) {
        gl = cosL;
        gr = sinR;
    } else if (law == PanSettings::Law6dB) {
        gl = 1.0f - a;
        gr = a;
    } else {  // -4.5 dB: halfway between constant power and linear
        gl = std::sqrt(cosL * (1.0f - a));
        gr = std::sqrt(sinR * a);
    }
}

void PanEngine::process(float* l, float* r, int n, const CurveTable& curve, const PanSettings& s, const PanTransport& t) noexcept {
    if (n <= 0) return;
    const double beats = divisionBeats(s.division);
    const double phaseOffset = s.phaseDeg / 360.0;
    double inc;
    if (s.sync) {
        inc = t.bpm / 60.0 / beats / sr_;
        if (t.playing) phase_ = t.ppq / beats;  // follow the song
    } else {
        inc = std::max(0.0, static_cast<double>(s.rateHz)) / sr_;
    }
    const float smoothCoef = s.smoothMs <= 0.01f ? 1.0f : 1.0f - std::exp(-1.0f / (static_cast<float>(sr_) * s.smoothMs * 0.001f));
    const float out = std::pow(10.0f, s.outputDb / 20.0f);
    const float depth = std::clamp(s.depth, 0.0f, 1.0f);
    const float mix = std::clamp(s.mix, 0.0f, 1.0f);
    const float inWidth = std::clamp(s.stereoWidth, 0.0f, 2.0f);
    float pkL = 0, pkR = 0;

    for (int i = 0; i < n; ++i) {
        float target = curve.at(phase_ + phaseOffset);
        if (s.invert) target = -target;
        value_ += smoothCoef * (target - value_);
        phase_ += inc;

        const float dl = l[i];
        const float dr = r ? r[i] : dl;
        // Input width first (mid/side).
        const float mid = 0.5f * (dl + dr), side = 0.5f * (dl - dr) * inWidth;
        float wl = mid + side, wr = mid - side;

        switch (s.mode) {
            case PanSettings::Volume: {
                const float g = 1.0f - depth * (0.5f - 0.5f * value_);  // curve top = full level
                wl *= g;
                wr *= g;
                break;
            }
            case PanSettings::Width: {
                const float w = std::max(0.0f, 1.0f + depth * value_);  // bottom = mono, top = double width
                const float m = 0.5f * (wl + wr), sd = 0.5f * (wl - wr) * w;
                wl = m + sd;
                wr = m - sd;
                break;
            }
            default: {
                float gl, gr;
                panGains(s.center + depth * value_, s.law, gl, gr);
                wl *= gl;
                wr *= gr;
                break;
            }
        }
        const float ol = (dl + mix * (wl - dl)) * out;
        const float orr = (dr + mix * (wr - dr)) * out;
        l[i] = ol;
        if (r) r[i] = orr;
        pkL = std::max(pkL, std::abs(ol));
        pkR = std::max(pkR, std::abs(orr));
    }
    phase_ -= std::floor(phase_);
    peakL_ = pkL;
    peakR_ = pkR;
}

}  // namespace pat::dsp
