// PatTune editor.
//   Piano roll: vertical keyboard on the left, the scale's notes as neon lanes (any cents, quarter
//               tones included), the sung pitch (grey) and the corrected pitch (neon) scrolling.
//   Simple view: key, major/minor, Auto and the correction speed.
//   Pro view:    every scale (Western, Arabic maqamat, Japanese, custom), a 24-step quarter-tone
//                editor to draw your own scale, retune speed, humanize, formant, mix, confidence
//                gate, A4 reference and the confidence meter.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "plugins/PatTune/Processor.h"

#include <array>
#include <memory>

class TuneEditor final : public pat::ui::PluginFrame {
public:
    explicit TuneEditor(TuneProcessor& p);
    ~TuneEditor() override;

private:
    class PianoRoll;
    class ConfidenceMeter;
    class ScaleEditor;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;
    void setParam(const char* id, float value);
    float param(const char* id) const;

    TuneProcessor& proc_;
    std::unique_ptr<PianoRoll> roll_;
    std::unique_ptr<ConfidenceMeter> confidence_;
    std::unique_ptr<ScaleEditor> scaleEditor_;
    juce::ComboBox key_, scale_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment_, scaleAttachment_;
    juce::TextButton major_{"MAJOR"}, minor_{"MINOR"}, auto_{"AUTO"};
    std::unique_ptr<juce::ParameterAttachment> autoAttachment_;
    pat::ui::NeonKnob speedBig_, speed_, humanize_, formant_, mix_, gate_, a4_;
    juce::Label keyLabel_, scaleLabel_, hint_;
    bool simulating_ = false;
};
