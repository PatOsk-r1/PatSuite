// A row of pill buttons bound to a choice parameter (e.g. 1/4 | 1/8 | 1/16). Host automation and
// undo work through juce::ParameterAttachment; the buttons never take keyboard focus.
#pragma once

#include "common/ui/Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace pat::ui {

class SegmentedChoice final : public juce::Component {
public:
    SegmentedChoice(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::StringArray labels, juce::Colour accent);
    void resized() override;
    int selected() const noexcept { return selected_; }
    // Lay the buttons out in a grid with this many columns (default: one row).
    void setColumns(int columns) {
        columns_ = columns;
        resized();
    }

private:
    void show(int index);
    std::vector<std::unique_ptr<juce::TextButton>> buttons_;
    std::unique_ptr<juce::ParameterAttachment> attachment_;
    int selected_ = 0, columns_ = 0;
};

}  // namespace pat::ui
