// Patan editor.
//   Curve: draw the movement. Click to add a point, drag to move it, double-click to delete it,
//          drag the small diamond in the middle of a segment to bend it, right-click a point for
//          its type (bend / smooth / hold). A playhead runs over the curve while audio plays.
//   Simple view: curve, ready-made shapes, mode (pan / volume / width), rate and depth.
//   Pro view:    adds phase, smooth, center, stereo width, mix, output, pan law and invert.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SegmentedChoice.h"
#include "plugins/Patan/Processor.h"

class PatanEditor final : public pat::ui::PluginFrame {
public:
    explicit PatanEditor(PatanProcessor& p);
    ~PatanEditor() override;

private:
    class CurveEditor;
    class StereoView;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;
    void tick(float dt);
    float param(const char* id) const;

    PatanProcessor& proc_;
    std::unique_ptr<CurveEditor> curve_;
    std::unique_ptr<StereoView> stereo_;
    pat::ui::SegmentedChoice mode_, law_;
    pat::ui::NeonKnob division_, rate_, depth_;
    pat::ui::NeonKnob phase_, smooth_, center_, width_, mix_, output_;
    juce::ToggleButton sync_{"TEMPO SYNC"}, invert_{"INVERT"}, snap_{"SNAP"};
    juce::AudioProcessorValueTreeState::ButtonAttachment syncAttachment_, invertAttachment_;
    juce::OwnedArray<juce::TextButton> shapes_;
    juce::ComboBox grid_;
    juce::Label modeLabel_, lawLabel_, hint_;
    bool simulating_ = false;
};
