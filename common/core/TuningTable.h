// A tuning as an arbitrary set of pitches, not tied to 12 semitones.
//
// A table is a list of allowed pitch classes, given in cents above a reference, repeating every
// `periodCents` (1200 = octave). That covers 12-TET scales, quarter tones (24-EDO), maqam
// "neutral" intervals at any cent value, non-octave tunings, and note sets drawn by the user.
// The pitch corrector asks it for the allowed pitch nearest to what is being sung.
// Fixed capacity (128 degrees), no heap: a copy can be handed to the audio thread safely.
#pragma once

#include <array>
#include <vector>

namespace pat {

class TuningTable {
public:
    // degrees: cents above the reference within one period, any order, any values.
    // referenceHz: frequency of degree 0 in the period that contains it (e.g. C4 = 261.63 Hz).
    static constexpr int kMaxDegrees = 128;

    TuningTable() = default;  // empty: nearest() returns its input
    TuningTable(const std::vector<double>& degreesCents, double referenceHz, double periodCents = 1200.0);
    // Same, from a plain array: no allocation at all (usable on the audio thread).
    TuningTable(const double* degreesCents, int count, double referenceHz, double periodCents = 1200.0) noexcept;

    // Equal temperament with `steps` divisions of the octave (12 = semitones, 24 = quarter tones).
    static TuningTable equal(int steps, double referenceHz = 261.6255653005986);

    double referenceHz() const noexcept { return refHz_; }
    double periodCents() const noexcept { return period_; }
    int size() const noexcept { return count_; }
    double degree(int i) const noexcept { return degrees_[static_cast<std::size_t>(i)]; }
    std::vector<double> degrees() const { return {degrees_.begin(), degrees_.begin() + count_}; }

    // Nearest allowed frequency to `hz`. Returns hz unchanged for an empty table or hz <= 0.
    double nearest(double hz) const noexcept;
    // Signed distance in cents from `hz` to the nearest allowed pitch.
    double centsToNearest(double hz) const noexcept;

    static double hzToCents(double hz, double refHz) noexcept;
    static double centsToHz(double cents, double refHz) noexcept;

private:
    std::array<double, kMaxDegrees> degrees_{};  // sorted, normalised into [0, period)
    int count_ = 0;
    double refHz_ = 261.6255653005986;
    double period_ = 1200.0;
};

}  // namespace pat
