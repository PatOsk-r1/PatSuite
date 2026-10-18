// Performance levels for the visuals and the rule that picks one from the machine's hardware.
// Pure C++ so the rule can be unit-tested; hardware detection itself lives in common/ui.
#pragma once

#include <string_view>

namespace pat {

enum class PerfTier { Low = 0, Medium = 1, High = 2 };

struct HardwareInfo {
    int physicalCores = 0;
    int memoryMB = 0;
    int gpuMemoryMB = 0;  // dedicated video memory; 0 = integrated or unknown
};

// What each level means for the visuals.
struct PerfProfile {
    int fps;              // repaint rate of animated layers
    int fftSize;          // analysis size for spectra
    int spectrumBands;    // 0 = full resolution, otherwise reduce to this many bands
    bool smoothing;       // interpolated / eased curves
    bool glow;            // soft glow and particles
    bool repaintOnlyOnChange;
};

// Rule (documented in docs/DECISIONES.md):
//   Low    if <= 2 cores, or < 6 GB RAM, or no dedicated GPU
//   High   if >= 8 cores, >= 16 GB RAM and >= 4 GB of video memory
//   Medium otherwise
PerfTier chooseTier(const HardwareInfo& hw) noexcept;
PerfProfile profileFor(PerfTier tier) noexcept;
std::string_view tierName(PerfTier tier) noexcept;  // "Low" / "Medium" / "High"

// "Neon fixed" mode: no animations, static neon strokes, meters refreshed every 100 ms.
inline constexpr int kNeonFixedFps = 10;

}  // namespace pat
