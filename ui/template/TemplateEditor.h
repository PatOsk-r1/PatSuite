// PatTemplate editor: shows every shared piece working together.
//   Simple view: one big knob and the spectrum.
//   Pro view:    spectrum, knob and input/output meters.
#pragma once

#include "common/ui/LevelMeter.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SpectrumView.h"
#include "plugins/PatTemplate/Processor.h"

class TemplateEditor final : public pat::ui::PluginFrame {
public:
    explicit TemplateEditor(TemplateProcessor& p);

private:
    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;

    TemplateProcessor& proc_;
    pat::ui::SpectrumView spectrum_;
    pat::ui::NeonKnob gain_;
    pat::ui::LevelMeter inMeter_, outMeter_;
};
