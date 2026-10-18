#include "dsp/tune/TuneEngine.h"

#include "dsp/tune/Scales.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

void TuneEngine::prepare(double sampleRate) {
    sr_ = sampleRate;
    detector_.prepare(sampleRate);
    shifter_.prepare(sampleRate);
    dryL_.assign(static_cast<std::size_t>(shifter_.latencySamples()), 0.0f);
    dryR_.assign(static_cast<std::size_t>(shifter_.latencySamples()), 0.0f);
    reset();
}

void TuneEngine::reset() noexcept {
    detector_.reset();
    shifter_.reset();
    std::fill(dryL_.begin(), dryL_.end(), 0.0f);
    std::fill(dryR_.begin(), dryR_.end(), 0.0f);
    dryPos_ = 0;
    correctionCents_ = 0;
    period_ = 0;
    tableScale_ = -1;
}

void TuneEngine::updateTable(const TuneSettings& s) noexcept {
    if (s.scale == tableScale_ && s.key == tableKey_ && s.customMask == tableMask_ && s.a4Hz == tableA4_) return;
    table_ = buildTable(s.scale, s.key, s.customMask, s.a4Hz);
    tableScale_ = s.scale;
    tableKey_ = s.key;
    tableMask_ = s.customMask;
    tableA4_ = s.a4Hz;
}

int TuneEngine::process(float* l, float* r, int n, const TuneSettings& s, TunePoint* points, int maxPoints) noexcept {
    updateTable(s);
    const float formant = std::exp2(std::clamp(s.formantSt, -12.0f, 12.0f) / 12.0f);
    const float mix = std::clamp(s.mix, 0.0f, 1.0f);
    const float hopSeconds = static_cast<float>(detector_.hop() / sr_);
    int written = 0;

    for (int i = 0; i < n; ++i) {
        const float xl = l[i], xr = r ? r[i] : xl;
        const float mono = 0.5f * (xl + xr);

        if (detector_.push(mono)) {
            const float hz = detector_.frequency();
            const float clarity = detector_.clarity();
            const bool voiced = hz > 0.0f && clarity >= s.gate;
            period_ = hz > 0.0f ? static_cast<float>(sr_ / hz) : 0.0f;
            float targetHz = 0, wanted = 0;
            if (voiced && s.autoOn && table_.size() > 0) {
                targetHz = static_cast<float>(table_.nearest(hz));
                wanted = static_cast<float>(TuningTable::hzToCents(targetHz, hz));
            }
            // Retune speed as a time constant; humanize stretches it on small wobbles of held notes.
            const float cents = voiced ? static_cast<float>(TuningTable::hzToCents(hz, 440.0)) : lastDetectedCents_;
            const bool held = voiced && std::abs(cents - lastDetectedCents_) < 15.0f;
            lastDetectedCents_ = cents;
            float tau = std::max(0.0f, s.retuneMs) * 0.001f;
            if (held && std::abs(wanted) < 40.0f) tau += std::clamp(s.humanize, 0.0f, 1.0f) * 0.25f;
            const float a = tau <= 0.0f ? 1.0f : 1.0f - std::exp(-hopSeconds / tau);
            correctionCents_ += (wanted - correctionCents_) * a;
            if (!voiced) correctionCents_ *= 0.5f;  // let go quickly in breaths and consonants

            last_ = {voiced ? hz : 0.0f, targetHz, voiced ? hz * std::exp2(correctionCents_ / 1200.0f) : 0.0f, clarity};
            if (points && written < maxPoints) points[written++] = last_;
        }

        // Dry signal delayed by the same latency (for the mix and for bypass).
        const float dl = dryL_[static_cast<std::size_t>(dryPos_)], dr = dryR_[static_cast<std::size_t>(dryPos_)];
        dryL_[static_cast<std::size_t>(dryPos_)] = xl;
        dryR_[static_cast<std::size_t>(dryPos_)] = xr;
        dryPos_ = (dryPos_ + 1) % static_cast<int>(dryL_.size());

        const float ratio = std::exp2(correctionCents_ / 1200.0f);
        const float wet = shifter_.process(mono, period_, ratio, formant);
        const bool bypass = !s.autoOn && formant == 1.0f;
        const float wl = bypass ? dl : wet, wr = bypass ? dr : wet;
        l[i] = dl + (wl - dl) * mix;
        if (r) r[i] = dr + (wr - dr) * mix;
    }
    return written;
}

}  // namespace pat::dsp
