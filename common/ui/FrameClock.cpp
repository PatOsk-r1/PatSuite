#include "common/ui/FrameClock.h"

namespace pat::ui {

FrameClock::FrameClock(juce::Component& owner, PerfSettings& settings) : owner_(owner), settings_(settings) {
    settings_.addChangeListener(this);
    restart();
}

FrameClock::~FrameClock() {
    settings_.removeChangeListener(this);
    stopTimer();
}

void FrameClock::restart() {
    rate_ = settings_.frameRate();
    paused_ = false;
    startTimerHz(rate_);
}

void FrameClock::timerCallback() {
    // isShowing() is false when the editor is closed, its window is minimised or hidden.
    if (!owner_.isShowing()) {
        if (!paused_) {
            paused_ = true;
            startTimerHz(2);
        }
        return;
    }
    if (paused_) restart();
    // A menu (or a dialog) is open: let it have the message thread, animate at about 5 fps.
    if (auto* mcm = juce::ModalComponentManager::getInstanceWithoutCreating(); mcm != nullptr && mcm->getNumModalComponents() > 0)
        if (++skipped_ < juce::jmax(1, rate_ / 5)) return;
    skipped_ = 0;

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = last_ > 0 ? static_cast<float>(juce::jlimit(0.0, 0.25, now - last_)) : 1.0f / static_cast<float>(rate_);
    last_ = now;
    fps_ += (1.0f / juce::jmax(dt, 1e-3f) - fps_) * 0.1f;
    for (auto& cb : callbacks_) cb(dt);
}

}  // namespace pat::ui
