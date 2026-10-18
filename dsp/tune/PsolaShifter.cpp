#include "dsp/tune/PsolaShifter.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {
namespace {
constexpr float kPi = 3.14159265358979f;
std::int64_t pow2AtLeast(std::int64_t n) {
    std::int64_t p = 1;
    while (p < n) p <<= 1;
    return p;
}
}  // namespace

void PsolaShifter::prepare(double sampleRate) {
    sr_ = sampleRate;
    // A grain starts being played as soon as it is scheduled, and reads the input up to one period
    // past a mark that is itself one period ahead (plus the quarter-period peak search): the output
    // must lag the input by a little over two of the longest periods (1/70 s).
    latency_ = static_cast<int>(std::ceil(sampleRate / 70.0 * 2.3)) + 16;
    const std::int64_t size = pow2AtLeast(static_cast<std::int64_t>(latency_) * 4 + 4096);
    inBuf_.assign(static_cast<std::size_t>(size), 0.0f);
    outBuf_.assign(static_cast<std::size_t>(size), 0.0f);
    inMask_ = outMask_ = size - 1;
    reset();
}

void PsolaShifter::reset() noexcept {
    std::fill(inBuf_.begin(), inBuf_.end(), 0.0f);
    std::fill(outBuf_.begin(), outBuf_.end(), 0.0f);
    inPos_ = 0;
    nextMark_ = 0;
    markHead_ = markCount_ = 0;
    nextSynth_ = 0;
    lastPeriod_ = 0;
}

float PsolaShifter::inInterp(double t) const noexcept {
    const double fl = std::floor(t);
    const auto i = static_cast<std::int64_t>(fl);
    const float f = static_cast<float>(t - fl);
    return in(i) + (in(i + 1) - in(i)) * f;
}

void PsolaShifter::addGrain(double centreOut, double mark, float period, float gain, float formantRatio) noexcept {
    const double p = std::max(2.0f, period);
    const auto first = static_cast<std::int64_t>(std::ceil(centreOut - p));
    const auto last = static_cast<std::int64_t>(std::floor(centreOut + p));
    for (std::int64_t j = first; j <= last; ++j) {
        const double offset = static_cast<double>(j) - centreOut;
        const float w = 0.5f + 0.5f * static_cast<float>(std::cos(static_cast<double>(kPi) * offset / p));  // Hann over 2 periods
        outBuf_[static_cast<std::size_t>(j & outMask_)] += inInterp(mark + offset * formantRatio) * w * gain;
    }
}

float PsolaShifter::process(float x, float periodSamples, float ratio, float formantRatio) noexcept {
    inBuf_[static_cast<std::size_t>(inPos_ & inMask_)] = x;
    ++inPos_;

    const bool voiced = periodSamples > 8.0f;
    const float period = voiced ? periodSamples : static_cast<float>(sr_ * 0.01);
    lastPeriod_ = period;

    // Analysis marks, exactly one period apart. A mark is placed once the input has passed it.
    while (nextMark_ < static_cast<double>(inPos_)) {
        marks_[markHead_] = nextMark_;
        markPeriod_[markHead_] = period;
        markHead_ = (markHead_ + 1) % 64;
        markCount_ = std::min(markCount_ + 1, 64);
        nextMark_ += period;
    }

    // Synthesis: output time runs `latency_` behind the input. When the output reaches the start of
    // the next grain (synthesis mark minus one period), overlap-add the grain whose analysis mark
    // is closest to that synthesis mark.
    const std::int64_t outTime = inPos_ - latency_;
    const float r = voiced ? std::clamp(ratio, 0.25f, 4.0f) : 1.0f;
    while (nextSynth_ - period <= static_cast<double>(outTime) && markCount_ > 0) {
        const double target = nextSynth_;
        int bestIdx = -1;
        double bestDist = 1e300;
        for (int i = 0; i < markCount_; ++i) {
            const int idx = (markHead_ - 1 - i + 64) % 64;
            if (marks_[idx] + markPeriod_[idx] + 1.0 >= static_cast<double>(inPos_)) continue;  // grain would need future input
            const double d = std::abs(marks_[idx] - target);
            if (d < bestDist) {
                bestDist = d;
                bestIdx = idx;
            }
        }
        if (bestIdx < 0) break;
        const float p = markPeriod_[bestIdx];
        const float hopOut = p / r;
        // Hann grains of length 2p sum to 1 when spaced p apart; spaced p/r they sum to r.
        addGrain(target, marks_[bestIdx], p, hopOut / p, formantRatio);
        nextSynth_ += hopOut;
    }

    const auto idx = static_cast<std::size_t>(outTime & outMask_);
    const float y = outTime >= 0 ? outBuf_[idx] : 0.0f;
    outBuf_[idx] = 0.0f;
    return y;
}

}  // namespace pat::dsp
