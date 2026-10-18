#include "common/core/TuningTable.h"

#include <algorithm>
#include <cmath>

namespace pat {

TuningTable::TuningTable(const std::vector<double>& degreesCents, double referenceHz, double periodCents)
    : refHz_(referenceHz), period_(periodCents > 1.0 ? periodCents : 1200.0) {
    std::vector<double> d(degreesCents);
    for (auto& v : d) {
        v = std::fmod(v, period_);
        if (v < 0) v += period_;
    }
    std::sort(d.begin(), d.end());
    d.erase(std::unique(d.begin(), d.end(), [](double a, double b) { return std::abs(a - b) < 1e-6; }), d.end());
    count_ = static_cast<int>(std::min<std::size_t>(d.size(), kMaxDegrees));
    std::copy_n(d.begin(), count_, degrees_.begin());
}

TuningTable::TuningTable(const double* degreesCents, int count, double referenceHz, double periodCents) noexcept
    : refHz_(referenceHz), period_(periodCents > 1.0 ? periodCents : 1200.0) {
    for (int i = 0; i < count && count_ < kMaxDegrees; ++i) {
        double v = std::fmod(degreesCents[i], period_);
        if (v < 0) v += period_;
        // Insertion sort, skipping duplicates.
        int pos = 0;
        while (pos < count_ && degrees_[static_cast<std::size_t>(pos)] < v - 1e-6) ++pos;
        if (pos < count_ && std::abs(degrees_[static_cast<std::size_t>(pos)] - v) < 1e-6) continue;
        for (int j = count_; j > pos; --j) degrees_[static_cast<std::size_t>(j)] = degrees_[static_cast<std::size_t>(j - 1)];
        degrees_[static_cast<std::size_t>(pos)] = v;
        ++count_;
    }
}

TuningTable TuningTable::equal(int steps, double referenceHz) {
    std::vector<double> d;
    for (int i = 0; i < steps; ++i) d.push_back(1200.0 * i / steps);
    return TuningTable(std::move(d), referenceHz, 1200.0);
}

double TuningTable::hzToCents(double hz, double refHz) noexcept { return 1200.0 * std::log2(hz / refHz); }
double TuningTable::centsToHz(double cents, double refHz) noexcept { return refHz * std::exp2(cents / 1200.0); }

double TuningTable::nearest(double hz) const noexcept {
    if (count_ == 0 || hz <= 0.0) return hz;
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
    for (int i = 0; i < count_; ++i) consider(degrees_[static_cast<std::size_t>(i)]);
    consider(degrees_[0] + period_);
    consider(degrees_[static_cast<std::size_t>(count_ - 1)] - period_);
    return centsToHz(periodIndex * period_ + best, refHz_);
}

double TuningTable::centsToNearest(double hz) const noexcept {
    if (count_ == 0 || hz <= 0.0) return 0.0;
    return hzToCents(nearest(hz), refHz_) - hzToCents(hz, refHz_);
}

}  // namespace pat
