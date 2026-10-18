// Phase-1 test engine: a clean gain stage with peak metering. It exists to prove the plumbing
// every real plugin will use (parameters in, audio out, meters and analysis samples to the UI)
// without any DSP of interest. Pure C++, no JUCE, no allocation in process().
#pragma once

#include <algorithm>
#include <cmath>

namespace pat::dsp {

class TemplateEngine {
public:
    void prepare(double sampleRate) noexcept { smoothCoef_ = 1.0f - std::exp(-1.0f / (0.02f * static_cast<float>(sampleRate))); }

    // gainDb applied smoothly (20 ms) to avoid clicks. In place, any channel count up to 2.
    void process(float* l, float* r, int n, float gainDb) noexcept {
        const float target = std::pow(10.0f, gainDb / 20.0f);
        float inPk = 0, outPk = 0;
        for (int i = 0; i < n; ++i) {
            gain_ += (target - gain_) * smoothCoef_;
            inPk = std::max(inPk, std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.0f));
            l[i] *= gain_;
            if (r) r[i] *= gain_;
            outPk = std::max(outPk, std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.0f));
        }
        inPeak_ = inPk;
        outPeak_ = outPk;
    }

    float inputPeak() const noexcept { return inPeak_; }
    float outputPeak() const noexcept { return outPeak_; }

private:
    float gain_ = 1.0f, smoothCoef_ = 0.001f, inPeak_ = 0, outPeak_ = 0;
};

}  // namespace pat::dsp
