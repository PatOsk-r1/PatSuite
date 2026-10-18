// PatBass editor.
//   Top:    neon oscilloscope of what you hear, the oscillator shapes (A, B and the sub) and the
//           LFO / cutoff movement.
//   Simple: twelve knobs that shape any bass (shape, sub, punch, glide, drive, glue, cutoff,
//           resonance, filter envelope, wobble, wobble rate, volume).
//   Pro:    every section: OSC A, OSC B, SUB & PUNCH, FILTER, ENVELOPES, VOICE, LFO, FX.
//   Bottom: a keyboard you can click (the computer keyboard stays with the host).
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "plugins/PatBass/Processor.h"

#include <juce_audio_utils/juce_audio_utils.h>

class BassEditor final : public pat::ui::PluginFrame {
public:
    explicit BassEditor(BassProcessor& p);
    ~BassEditor() override;

private:
    class Scope;
    class ShapeView;
    struct Section {
        juce::String title;
        juce::Colour colour;
        std::vector<juce::Component*> items;
        int columns = 4;
        juce::Rectangle<int> bounds;
        int weight = 0;  // share of the row width (0 = same as columns)
    };

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;
    pat::ui::NeonKnob* knob(const char* id, const char* label, juce::Colour c, bool simple = false);
    juce::Component* combo(const char* id, const char* label);
    juce::Component* toggle(const char* id, const char* label, juce::Colour c);
    void paintOverChildren(juce::Graphics&) override {}

    class Panels;

    BassProcessor& proc_;
    std::unique_ptr<Scope> scope_;
    std::unique_ptr<ShapeView> shape_;
    std::unique_ptr<Panels> panels_;
    std::vector<std::unique_ptr<pat::ui::NeonKnob>> knobs_, simpleKnobs_;
    std::vector<std::unique_ptr<juce::Component>> others_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt_;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt_;
    std::vector<Section> sections_;
    juce::MidiKeyboardComponent keyboard_;
    bool simulating_ = false;
};
