// A tuning as an arbitrary set of pitches, not tied to 12 semitones.
//
// A table is a list of allowed pitch classes, given in cents above a reference, repeating every
// `periodCents` (1200 = octave). That covers 12-TET scales, quarter tones (24-EDO), maqam
// "neutral" intervals at any cent value, non-octave tunings, and note sets drawn by the user.
// The pitch corrector asks it for the allowed pitch nearest to what is being sung.
#pragma once

#include <vector>

namespace pat {

class TuningTable {
public:
    // degrees: cents above the reference within one period, any order, any values.
    // referenceHz: frequency of degree 0 in the period that contains it (e.g. C4 = 261.63 Hz).
    TuningTable(std::vector<double> degreesCents, double referenceHz, double periodCents = 1200.0);

    // Equal temperament with `steps` divisions of the octave (12 = semitones, 24 = quarter tones).
    static TuningTable equal(int steps, double referenceHz = 261.6255653005986);

    double referenceHz() const noexcept { return refHz_; }
    double periodCents() const noexcept { return period_; }
    const std::vector<double>& degrees() const noexcept { return degrees_; }

    // Nearest allowed frequency to `hz`. Returns hz unchanged for an empty table or hz <= 0.
    double nearest(double hz) const noexcept;
    // Signed distance in cents from `hz` to the nearest allowed pitch.
    double centsToNearest(double hz) const noexcept;

    static double hzToCents(double hz, double refHz) noexcept;
    static double centsToHz(double cents, double refHz) noexcept;

private:
    std::vector<double> degrees_;  // sorted, normalised into [0, period)
    double refHz_;
    double period_;
};

}  // namespace pat
