// Vertical level meter (dBFS). Animated mode: smooth fall and a peak-hold line that drops slowly.
// "Neon fixed": the bar jumps straight to the current value (refreshed every 100 ms by the clock)
// with no falling animation.
#pragma once

#include "common/ui/PerfSettings.h"
#include "common/ui/Theme.h"

namespace pat::ui {

class LevelMeter final : public juce::Component {
public:
    LevelMeter(PerfSettings& settings, const juce::String& label, juce::Colour colour);
    // Feed the newest peak (linear gain) once per frame; dt in seconds.
    void update(float peak, float dt);
    void paint(juce::Graphics&) override;

private:
    PerfSettings& settings_;
    juce::String label_;
    juce::Colour colour_;
    float shownDb_ = -90, holdDb_ = -90, holdTime_ = 0;
};

}  // namespace pat::ui
