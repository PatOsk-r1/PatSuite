// Scale library for PatTune. Every scale is a list of cents above its root (any values, so
// quarter tones and other non-12 intervals work). Arabic maqamat use 24-EDO quarter tones.
// "In Sen" and "Insen" are the same Japanese scale: it appears once with both names.
#pragma once

#include "common/core/TuningTable.h"

#include <cstdint>

namespace pat::dsp {

struct ScaleDef {
    const char* name;
    const char* group;  // "Western", "Arabic (maqam)", "Japanese", "Custom"
    const double* cents;
    int count;
};

int numScales() noexcept;
const ScaleDef& scale(int index) noexcept;
int customScaleIndex() noexcept;  // the last entry: notes drawn by the user

const char* keyName(int key) noexcept;  // 0 = C ... 11 = B

// The 24 quarter-tone steps of the custom scale editor, as a bit mask (bit i = i * 50 cents).
inline constexpr std::uint32_t kDefaultCustomMask = 0b000000000101010100101010u;  // a major-like set

// Builds the table for a scale on a root key. a4Hz sets the reference pitch (440 = standard).
// Allocation-free (safe on the audio thread).
TuningTable buildTable(int scaleIndex, int key, std::uint32_t customMask, double a4Hz) noexcept;

}  // namespace pat::dsp
