// Suite-wide visual settings: performance level and "Neon fixed" mode.
//
// One shared instance per process (juce::SharedResourcePointer), saved in
// %APPDATA%\PatSuite\settings.xml so the choice applies to every Pat plugin and survives restarts.
// Hardware is detected once at startup: physical cores, RAM, and dedicated video memory (DXGI on
// Windows; JUCE has no GPU query). Everything here runs on the message thread only.
#pragma once

#include "common/core/PerfTier.h"

#include <juce_events/juce_events.h>
#include <juce_data_structures/juce_data_structures.h>

namespace pat::ui {

class PerfSettings final : public juce::ChangeBroadcaster {
public:
    enum class Choice { Auto = 0, Low = 1, Medium = 2, High = 3 };
    enum class NeonFixed { Auto = 0, On = 1, Off = 2 };

    PerfSettings();

    const HardwareInfo& hardware() const noexcept { return hw_; }
    juce::String gpuName() const { return gpuName_; }
    PerfTier detectedTier() const noexcept { return detected_; }

    Choice choice() const noexcept { return choice_; }
    NeonFixed neonFixedChoice() const noexcept { return neonFixed_; }
    void setChoice(Choice c);
    void setNeonFixed(NeonFixed n);

    // What is actually in use.
    PerfTier tier() const noexcept;
    bool neonFixed() const noexcept;  // Auto = on for the Low level
    PerfProfile profile() const noexcept { return profileFor(tier()); }
    int frameRate() const noexcept { return neonFixed() ? kNeonFixedFps : profile().fps; }

    // Temporary override used by the diagnostic snapshots (not saved).
    void forceForSnapshot(PerfTier t, bool neon);

private:
    void detectHardware();
    void save();

    HardwareInfo hw_;
    juce::String gpuName_;
    PerfTier detected_ = PerfTier::Medium;
    Choice choice_ = Choice::Auto;
    NeonFixed neonFixed_ = NeonFixed::Auto;
    std::unique_ptr<juce::PropertiesFile> file_;
};

}  // namespace pat::ui
