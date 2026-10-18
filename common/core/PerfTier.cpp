#include "common/core/PerfTier.h"

namespace pat {

PerfTier chooseTier(const HardwareInfo& hw) noexcept {
    if (hw.physicalCores <= 2 || hw.memoryMB < 6 * 1024 || hw.gpuMemoryMB <= 0) return PerfTier::Low;
    if (hw.physicalCores >= 8 && hw.memoryMB >= 16 * 1024 - 512 && hw.gpuMemoryMB >= 4 * 1024 - 256) return PerfTier::High;
    return PerfTier::Medium;
}

PerfProfile profileFor(PerfTier tier) noexcept {
    switch (tier) {
        case PerfTier::Low: return {20, 1024, 48, false, false, true};
        case PerfTier::Medium: return {30, 2048, 0, true, false, false};
        case PerfTier::High: return {60, 4096, 0, true, true, false};
    }
    return {30, 2048, 0, true, false, false};
}

std::string_view tierName(PerfTier tier) noexcept {
    switch (tier) {
        case PerfTier::Low: return "Low";
        case PerfTier::Medium: return "Medium";
        case PerfTier::High: return "High";
    }
    return "Medium";
}

}  // namespace pat
