#include "common/ui/SegmentedChoice.h"

namespace pat::ui {

SegmentedChoice::SegmentedChoice(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, juce::StringArray labels,
                                 juce::Colour accent) {
    auto* param = state.getParameter(paramId);
    jassert(param != nullptr);
    for (int i = 0; i < labels.size(); ++i) {
        auto b = std::make_unique<juce::TextButton>(labels[i]);
        b->setColour(juce::TextButton::buttonOnColourId, accent);
        b->setClickingTogglesState(false);
        b->setWantsKeyboardFocus(false);
        b->setMouseClickGrabsKeyboardFocus(false);
        b->onClick = [this, i] {
            if (attachment_) attachment_->setValueAsCompleteGesture(static_cast<float>(i));
        };
        if (param) b->setTooltip(param->getName(64) + ": " + labels[i]);
        addAndMakeVisible(*b);
        buttons_.push_back(std::move(b));
    }
    if (param) {
        attachment_ = std::make_unique<juce::ParameterAttachment>(*param, [this](float v) { show(juce::roundToInt(v)); });
        attachment_->sendInitialUpdate();
    }
}

void SegmentedChoice::show(int index) {
    selected_ = index;
    for (int i = 0; i < static_cast<int>(buttons_.size()); ++i) buttons_[static_cast<std::size_t>(i)]->setToggleState(i == index, juce::dontSendNotification);
}

void SegmentedChoice::resized() {
    auto r = getLocalBounds();
    const int n = static_cast<int>(buttons_.size());
    if (n == 0) return;
    const int cols = columns_ > 0 ? columns_ : n;
    const int rows = (n + cols - 1) / cols;
    const int w = r.getWidth() / cols, h = r.getHeight() / rows;
    for (int i = 0; i < n; ++i)
        buttons_[static_cast<std::size_t>(i)]->setBounds(
            juce::Rectangle<int>(r.getX() + (i % cols) * w, r.getY() + (i / cols) * h, w, h).reduced(2, rows > 1 ? 2 : 0));
}

}  // namespace pat::ui
