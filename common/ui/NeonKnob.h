// Rotary knob attached to a host parameter: neon value arc, value shown while hovering/dragging,
// double-click to reset, Shift for fine control. The halo appears only on the High level with
// animations allowed; in "Neon fixed" the arc is a plain stroke with the same colour.
#pragma once

#include "common/ui/PerfSettings.h"
#include "common/ui/Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace pat::ui {

class NeonKnob final : public juce::Slider, private juce::ChangeListener {
public:
    enum class Size { Small, Normal, Big };

    NeonKnob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label, juce::Colour colour,
             PerfSettings& settings, Size size = Size::Normal);
    // Not bound to a parameter (macro knobs that drive several parameters from the editor).
    NeonKnob(const juce::String& label, juce::Colour colour, PerfSettings& settings, Size size = Size::Normal);
    ~NeonKnob() override;

    void setAccent(juce::Colour c) {
        colour_ = c;
        repaint();
    }
    void paint(juce::Graphics&) override;
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { repaint(); }
    juce::String label_;
    juce::Colour colour_;
    PerfSettings& settings_;
    Size size_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    void init();
};

}  // namespace pat::ui
