// Drives the animated layers of one editor at the frame rate of the current performance level
// (or 10 Hz in "Neon fixed" mode) and pauses itself when the window is not visible: closed,
// minimised or hidden behind the DAW. While paused it only checks twice a second whether the
// window came back.
#pragma once

#include "common/ui/PerfSettings.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace pat::ui {

class FrameClock final : private juce::Timer, private juce::ChangeListener {
public:
    FrameClock(juce::Component& owner, PerfSettings& settings);
    ~FrameClock() override;

    // Called once per frame with the time since the previous frame (seconds).
    void add(std::function<void(float dt)> callback) { callbacks_.push_back(std::move(callback)); }

    bool paused() const noexcept { return paused_; }
    int frameRate() const noexcept { return rate_; }
    float measuredFps() const noexcept { return fps_; }

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override { restart(); }
    void restart();

    juce::Component& owner_;
    PerfSettings& settings_;
    std::vector<std::function<void(float)>> callbacks_;
    bool paused_ = false;
    int rate_ = 30;
    double last_ = 0;
    float fps_ = 0;
};

}  // namespace pat::ui
