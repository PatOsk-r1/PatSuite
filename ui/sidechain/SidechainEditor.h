// PatChain editor.
//   Graph: the kick (trigger) waveform scrolls on top, the gain curve below; a neon ring pulses on
//          each hit or beat (a steady flash in "Neon fixed").
//   Simple view: "Pump" knob and rhythm selector (1/4, 1/8, 1/16).
//   Pro view:    trigger (Auto / Tempo / Sidechain), threshold, attack, release, knee, ratio,
//                sidechain filter (low/high cut), listen to the sidechain, output.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SegmentedChoice.h"
#include "plugins/PatChain/Processor.h"

#include <array>
#include <vector>

class SidechainEditor final : public pat::ui::PluginFrame {
public:
    explicit SidechainEditor(SidechainProcessor& p);
    ~SidechainEditor() override;

private:
    class PumpGraph;
    class PulseRing;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;

    SidechainProcessor& proc_;
    std::unique_ptr<PumpGraph> graph_;
    std::unique_ptr<PulseRing> ring_;
    pat::ui::NeonKnob pump_;
    pat::ui::SegmentedChoice rate_, shape_, trigger_;
    pat::ui::NeonKnob threshold_, attack_, release_, knee_, ratio_, scLow_, scHigh_, output_;
    juce::ToggleButton listen_{"LISTEN SIDECHAIN"};
    juce::AudioProcessorValueTreeState::ButtonAttachment listenAttachment_;
    juce::Label rateLabel_, shapeLabel_, triggerLabel_, hint_, scStatus_;
    juce::TextButton help_{"CONECTAR BOMBO"};
    bool simulating_ = false;
    std::array<pat::dsp::GraphPoint, 2048> buf_{};
    int lastTriggers_ = 0;
};
