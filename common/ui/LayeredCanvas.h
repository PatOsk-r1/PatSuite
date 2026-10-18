// Base for every visual: a static layer (grid, scales, labels) drawn once into a cached image and
// a dynamic layer (curves, meters) drawn on top each frame. The static image is rebuilt only when
// the size, the screen scale or the visual settings change.
#pragma once

#include "common/ui/PerfSettings.h"
#include "common/ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace pat::ui {

class LayeredCanvas : public juce::Component, private juce::ChangeListener {
public:
    explicit LayeredCanvas(PerfSettings& settings);
    ~LayeredCanvas() override;

    void paint(juce::Graphics& g) final;
    void resized() override { invalidateStatic(); }
    void invalidateStatic() noexcept { staticValid_ = false; }

protected:
    virtual void paintStatic(juce::Graphics& g) = 0;   // called rarely
    virtual void paintDynamic(juce::Graphics& g) = 0;  // called every repaint
    PerfSettings& settings() noexcept { return settings_; }
    bool animated() const noexcept { return !settings_.neonFixed(); }
    bool glow() const noexcept { return animated() && settings_.profile().glow; }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override {
        invalidateStatic();
        repaint();
    }
    PerfSettings& settings_;
    juce::Image cache_;
    float cacheScale_ = 0;
    bool staticValid_ = false;
};

}  // namespace pat::ui
