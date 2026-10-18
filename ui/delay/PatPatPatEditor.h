// PatPatPat editor.
//   Stage: every hit you play throws a neon ball that bounces between the left and right walls
//          following the real echo times and levels; below, the echo map (left up, right down).
//          In "Neon fixed" only the echo map is shown, without movement.
//   Simple view: mode, tempo sync, freeze, time, feedback and mix.
//   Pro view:    adds the right time and link, character, low/high cut, drive, modulation,
//                ducking, width, diffusion and output.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SegmentedChoice.h"
#include "plugins/PatPatPat/Processor.h"

class PatPatPatEditor final : public pat::ui::PluginFrame {
public:
    explicit PatPatPatEditor(PatPatPatProcessor& p);
    ~PatPatPatEditor() override;

private:
    class Stage;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;
    void tick(float dt);
    float param(const char* id) const;

    PatPatPatProcessor& proc_;
    std::unique_ptr<Stage> stage_;
    pat::ui::SegmentedChoice mode_, character_;
    pat::ui::NeonKnob divL_, timeL_, divR_, timeR_, feedback_, mix_;
    pat::ui::NeonKnob lowCut_, highCut_, drive_, modRate_, modDepth_, duck_, width_, diffusion_, output_;
    juce::ToggleButton sync_{"TEMPO SYNC"}, link_{"LINK L/R"};
    juce::TextButton freeze_{"FREEZE"};
    juce::AudioProcessorValueTreeState::ButtonAttachment syncAttachment_, linkAttachment_, freezeAttachment_;
    juce::Label modeLabel_, characterLabel_, hint_;
    bool simulating_ = false;
    float avgIn_ = 0, sinceShot_ = 1;
};
