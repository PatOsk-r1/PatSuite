#include "common/core/TuningTable.h"

#include <algorithm>
#include <cmath>

namespace pat {

TuningTable::TuningTable(std::vector<double> degreesCents, double referenceHz, double periodCents)
    : degrees_(std::move(degreesCents)), refHz_(referenceHz), period_(periodCents > 1.0 ? periodCents : 1200.0) {
    for (auto& d : degrees_) {
        d = std::fmod(d, period_);
        if (d < 0) d += period_;
    }
    std::sort(degrees_.begin(), degrees_.end());
    degrees_.erase(std::unique(degrees_.begin(), degrees_.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }), degrees_.end());
}

TuningTable TuningTable::equal(int steps, double referenceHz) {
    std::vector<double> d;
    for (int i = 0; i < steps; ++i) d.push_back(1200.0 * i / steps);
    return TuningTable(std::move(d), referenceHz, 1200.0);
}

double TuningTable::hzToCents(double hz, double refHz) noexcept { return 1200.0 * std::log2(hz / refHz); }
double TuningTable::centsToHz(double cents, double refHz) noexcept { return refHz * std::exp2(cents / 1200.0); }

double TuningTable::nearest(double hz) const noexcept {
    if (degrees_.empty() || hz <= 0.0) return hz;
    const double c = hzToCents(hz, refHz_);
    const double periodIndex = std::floor(c / period_);
    const double within = c - periodIndex * period_;
    // Candidates: every degree in this period, plus the first degree of the next period and the
    // last degree of the previous one (so notes near the period boundary snap correctly).
    double best = 0, bestDist = 1e300;
    const auto consider = [&](double candidate) {
        const double dist = std::abs(candidate - within);
        if (dist < bestDist) {
            bestDist = dist;
            best = candidate;
        }
    };
    for (double d : degrees_) consider(d);
    consider(degrees_.front() + period_);
    consider(degrees_.back() - period_);
    return centsToHz(periodIndex * period_ + best, refHz_);
}

double TuningTable::centsToNearest(double hz) const noexcept {
    if (degrees_.empty() || hz <= 0.0) return 0.0;
    return hzToCents(nearest(hz), refHz_) - hzToCents(hz, refHz_);
}

}  // namespace pat
