// PatDoubler editor.
//   Wave stack: the voice drawn once per copy in neon, each shifted by its delay and moved up or
//               down by its pan, so the copies visibly spread apart or come together.
//   Simple view: Width and Thickness.
//   Pro view:    copies, per-copy delay / detune / pan, timing variation, stereo spread, output,
//                and a vectorscope of the stereo output.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SegmentedChoice.h"
#include "plugins/PatDoubler/Processor.h"

#include <array>
#include <memory>
#include <vector>

class DoublerEditor final : public pat::ui::PluginFrame {
public:
    explicit DoublerEditor(DoublerProcessor& p);
    ~DoublerEditor() override;

private:
    class WaveStack;
    class Vectorscope;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;

    DoublerProcessor& proc_;
    std::unique_ptr<WaveStack> waves_;
    std::unique_ptr<Vectorscope> scope_;
    pat::ui::NeonKnob width_, thickness_, widthSmall_, thicknessSmall_, variation_, spread_, output_;
    std::vector<std::unique_ptr<pat::ui::NeonKnob>> copyKnobs_;  // delay, detune, pan per copy
    pat::ui::SegmentedChoice copies_;
    std::array<juce::Label, 4> copyLabels_;
    juce::Label copiesLabel_;
    bool simulating_ = false;
};
