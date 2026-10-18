// 2x up/down sampler made of two polyphase IIR half-band filters (chains of first order
// allpasses, elliptic design). Used by PatQpro's "Natural" mode: the EQ runs at twice the sample
// rate, so bells and shelves near the top of the spectrum keep their analog shape instead of
// being squeezed by the bilinear transform. No allocation after construction.
#pragma once

#include <array>

namespace pat::dsp {

class HalfBandDesign {
public:
    static constexpr int kCoefs = 8;
    // Allpass coefficients for the given transition band (fraction of the sample rate, e.g. 0.04).
    static std::array<double, kCoefs> compute(double transition) noexcept;
};

class Oversampler2x {
public:
    Oversampler2x() noexcept;
    void reset() noexcept;
    // One input sample -> two output samples at twice the rate.
    void up(float in, float& out0, float& out1) noexcept;
    // Two input samples at twice the rate -> one output sample.
    float down(float in0, float in1) noexcept;
    // Low-frequency group delay of up + down, in samples at the base rate.
    static constexpr float kLatency = 3.0f;

private:
    struct Chain {
        std::array<double, HalfBandDesign::kCoefs> x{}, y{};
    };
    static void stage(const std::array<double, HalfBandDesign::kCoefs>& c, Chain& s, double& even, double& odd) noexcept;
    std::array<double, HalfBandDesign::kCoefs> coef_{};
    Chain up_, down_;
};

}  // namespace pat::dsp
