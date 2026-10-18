// PatSquash editor.
//   Bands: three columns (low, mid, high). Each shows the band level, the level after squashing
//          (neon), the "down" threshold (coral: above it the band is turned down) and the "up"
//          threshold (ice: below it quiet detail is turned up). Drag the lines to move them,
//          double-click a line to reset it.
//   Simple view: bands + DEPTH, TIME, UPWARD, DOWNWARD, INPUT, OUTPUT.
//   Pro view:    adds each band's down/up ratio and gain, and the two split frequencies.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "plugins/PatSquash/Processor.h"

class SquashEditor final : public pat::ui::PluginFrame {
public:
    explicit SquashEditor(SquashProcessor& p);
    ~SquashEditor() override;

private:
    class BandsView;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;

    SquashProcessor& proc_;
    std::unique_ptr<BandsView> bands_;
    pat::ui::NeonKnob depth_, time_, upward_, downward_, input_, output_, lowCross_, highCross_;
    std::vector<std::unique_ptr<pat::ui::NeonKnob>> bandKnobs_;  // per band: down ratio, up ratio, gain
    juce::Label hint_;
    bool simulating_ = false;
};
