#include "common/ui/NeonKnob.h"

namespace pat::ui {
namespace {
constexpr float kStart = juce::MathConstants<float>::pi * 1.25f;
constexpr float kEnd = juce::MathConstants<float>::pi * 2.75f;
}  // namespace

NeonKnob::NeonKnob(juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label, juce::Colour colour,
                   PerfSettings& settings, Size size)
    : label_(label), colour_(colour), settings_(settings), size_(size), attachment_(state, paramId, *this) {
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(kStart, kEnd, true);
    setMouseDragSensitivity(size == Size::Big ? 300 : 220);
    setVelocityModeParameters(0.6, 1, 0.08, true, juce::ModifierKeys::shiftModifier);
    if (auto* p = state.getParameter(paramId)) {
        setDoubleClickReturnValue(true, p->convertFrom0to1(p->getDefaultValue()));
        setTooltip(p->getName(64) + "  (double-click: reset, Shift+drag: fine)");
    }
    settings_.addChangeListener(this);
}

NeonKnob::~NeonKnob() { settings_.removeChangeListener(this); }

void NeonKnob::paint(juce::Graphics& g) {
    auto b = getLocalBounds().toFloat();
    const float labelH = size_ == Size::Big ? 20.0f : 15.0f;
    const auto labelArea = b.removeFromBottom(labelH);
    const float maxD = size_ == Size::Big ? 150.0f : (size_ == Size::Small ? 44.0f : 70.0f);
    const float d = juce::jmin(b.getWidth(), b.getHeight(), maxD);
    const auto c = b.getCentre();
    const float r = d * 0.5f - 6.0f;
    if (r < 4.0f) return;
    const float norm = static_cast<float>(valueToProportionOfLength(getValue()));
    const float ang = kStart + (kEnd - kStart) * norm;
    const auto col = isEnabled() ? colour_ : colours::faint;
    const bool glow = !settings_.neonFixed() && settings_.profile().glow;
    const float stroke = size_ == Size::Big ? 5.0f : 3.0f;

    juce::Path track;
    track.addCentredArc(c.x, c.y, r, r, 0.0f, kStart, kEnd, true);
    g.setColour(colours::hover);
    g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (norm > 0.002f) {
        juce::Path arc;
        arc.addCentredArc(c.x, c.y, r, r, 0.0f, kStart, ang, true);
        strokeNeon(g, arc, col, stroke, glow);
    }
    const float body = r * 0.72f;
    g.setGradientFill(juce::ColourGradient(colours::raised.brighter(0.15f), c.x, c.y - body, colours::background, c.x, c.y + body, false));
    g.fillEllipse(juce::Rectangle<float>(body * 2, body * 2).withCentre(c));
    g.setColour(col.withAlpha(0.35f));
    g.drawEllipse(juce::Rectangle<float>(body * 2, body * 2).withCentre(c), 1.0f);
    const juce::Point<float> dir(std::sin(ang), -std::cos(ang));
    g.setColour(colours::text);
    g.drawLine({c + dir * (body * 0.3f), c + dir * (body * 0.9f)}, size_ == Size::Big ? 3.0f : 2.0f);

    const bool showValue = isMouseOverOrDragging();
    g.setColour(showValue ? colours::text : colours::dim);
    g.setFont(font(size_ == Size::Big ? 15.0f : 11.0f, true));
    g.drawText(showValue ? getTextFromValue(getValue()) : label_, labelArea.expanded(10.0f, 0.0f), juce::Justification::centred, false);
}

}  // namespace pat::ui
