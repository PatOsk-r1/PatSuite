// PatDoubler audio engine. Pure C++, no JUCE, no allocation in process(), zero latency for the
// dry signal.
//
// Up to four copies of the (mono-summed) input, each with its own delay, detune and pan, plus a
// slow random timing drift ("variation") so the copies never sound mechanical. Detune uses a
// two-tap crossfading pitch shifter: a delay that slowly sweeps, read by two taps half a window
// apart and crossfaded with sine-squared windows (smooth for the small shifts a doubler uses).
// The shifter and the drift headroom add a fixed 16 ms: a copy is heard at delayMs + 16 ms.
//   width:     scales every pan position (0 = all copies in the centre, mono)
//   thickness: how many copies sound and how loud (0 = dry only, 1 = all copies at full level)
//   spread:    extra stereo spread on top of width (pro)
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pat::dsp {

struct DoublerSettings {
    static constexpr int kMaxCopies = 4;
    int copies = 4;
    std::array<float, kMaxCopies> delayMs{14.0f, 23.0f, 31.0f, 41.0f};
    std::array<float, kMaxCopies> detuneCents{-9.0f, 11.0f, -17.0f, 19.0f};
    std::array<float, kMaxCopies> pan{-0.8f, 0.8f, -0.45f, 0.45f};
    float variation = 0.35f;  // 0..1 timing drift (up to +/- 4 ms)
    float spread = 1.0f;      // 0..1.5
    float width = 0.8f;       // 0..1
    float thickness = 0.6f;   // 0..1
    float dryGain = 1.0f;
    float outputDb = 0.0f;
};

struct StereoSample {
    float l = 0, r = 0;
};

class DoublerEngine {
public:
    void prepare(double sampleRate);  // allocates the delay line
    void reset() noexcept;
    // In place (r may be null: the output is then mono). Writes every 4th output frame to `scope`
    // (up to maxScope) for the vectorscope and returns how many it wrote.
    int process(float* l, float* r, int n, const DoublerSettings& s, StereoSample* scope, int maxScope) noexcept;

    // Effective per-copy values after width/thickness/spread (also used by the UI).
    static float copyGain(int copy, const DoublerSettings& s) noexcept;
    static float copyPan(int copy, const DoublerSettings& s) noexcept;
    static float copyDetune(int copy, const DoublerSettings& s) noexcept;

private:
    float readDelay(float d) const noexcept;

    float sr_ = 48000;
    std::vector<float> line_;
    int mask_ = 0, pos_ = 0;
    std::array<float, DoublerSettings::kMaxCopies> phase_{}, drift_{}, driftTarget_{};
    std::uint32_t rng_ = 0x2468ace1u;
    int driftCounter_ = 0;
    float outGain_ = 1;
    int scopeCount_ = 0;
};

}  // namespace pat::dsp
