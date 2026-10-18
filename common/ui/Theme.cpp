#include "common/ui/Theme.h"

namespace pat::ui {

using namespace colours;

juce::Font font(float height, bool bold) {
    return bold ? juce::Font(juce::FontOptions("Segoe UI", "Semibold", height)) : juce::Font(juce::FontOptions("Segoe UI", height, juce::Font::plain));
}

void drawPanel(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent) {
    const auto rf = r.toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(rf, 8.0f);
    g.setColour(line);
    g.drawRoundedRectangle(rf.reduced(0.5f), 8.0f, 1.0f);
    if (title.isEmpty()) return;
    int x = r.getX() + 12;
    if (!accent.isTransparent()) {
        g.setColour(accent);
        g.fillRoundedRectangle(static_cast<float>(x), static_cast<float>(r.getY() + 10), 3.0f, 10.0f, 1.5f);
        x += 8;
    }
    g.setColour(dim);
    g.setFont(font(11.5f, true));
    g.drawText(title.toUpperCase(), x, r.getY() + 4, r.getWidth() - 24, 22, juce::Justification::centredLeft);
}

void strokeNeon(juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, bool glow) {
    if (glow) {
        g.setColour(c.withAlpha(0.12f));
        g.strokePath(p, juce::PathStrokeType(width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(c.withAlpha(0.25f));
        g.strokePath(p, juce::PathStrokeType(width * 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour(c);
    g.strokePath(p, juce::PathStrokeType(width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

LookAndFeel::LookAndFeel() {
    setDefaultSansSerifTypefaceName("Segoe UI");
    using CS = juce::LookAndFeel_V4::ColourScheme;
    setColourScheme(CS{background, panel, raised, line, text, cyan, background, raised, text});
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::ComboBox::backgroundColourId, raised);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, line);
    setColour(juce::PopupMenu::backgroundColourId, raised);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, hover);
    setColour(juce::PopupMenu::highlightedTextColourId, cyan);
    setColour(juce::TooltipWindow::backgroundColourId, raised);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, line);
    setColour(juce::Label::textColourId, text);
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) {
    const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour(juce::TextButton::buttonOnColourId);
    g.setColour(on ? accent.withAlpha(0.18f) : (down ? hover.brighter(0.1f) : (over ? hover : raised)));
    g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
    g.setColour(on ? accent : line);
    g.drawRoundedRectangle(r, r.getHeight() * 0.5f, on ? 1.5f : 1.0f);
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) {
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.setColour(b.getToggleState() ? text : (b.isEnabled() ? dim : faint));
    g.drawText(b.getButtonText(), b.getLocalBounds().reduced(6, 0), juce::Justification::centred, true);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int h) { return font(juce::jmin(13.0f, static_cast<float>(h) * 0.5f), true); }

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool) {
    // Neon switch: a pill with a sliding dot, label on the right.
    const auto r = b.getLocalBounds().toFloat();
    const auto pill = juce::Rectangle<float>(r.getX() + 2.0f, r.getCentreY() - 8.0f, 30.0f, 16.0f);
    const bool on = b.getToggleState();
    const auto accent = b.findColour(juce::ToggleButton::tickColourId);
    g.setColour(on ? accent.withAlpha(0.25f) : (over ? hover : raised));
    g.fillRoundedRectangle(pill, 8.0f);
    g.setColour(on ? accent : line);
    g.drawRoundedRectangle(pill, 8.0f, 1.0f);
    g.setColour(on ? accent : dim);
    g.fillEllipse(juce::Rectangle<float>(10.0f, 10.0f).withCentre({on ? pill.getRight() - 8.0f : pill.getX() + 8.0f, pill.getCentreY()}));
    g.setColour(on ? text : dim);
    g.setFont(font(11.5f, true));
    g.drawText(b.getButtonText(), r.withTrimmedLeft(40.0f), juce::Justification::centredLeft, true);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    const auto r = juce::Rectangle<float>(0, 0, static_cast<float>(w), static_cast<float>(h)).reduced(0.5f);
    g.setColour(box.isMouseOver(true) ? hover : raised);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(line);
    g.drawRoundedRectangle(r, 5.0f, 1.0f);
    juce::Path arrow;
    const float ax = static_cast<float>(w) - 12.0f, ay = static_cast<float>(h) * 0.5f;
    arrow.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour(dim);
    g.fillPath(arrow);
}

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    label.setBounds(6, 0, box.getWidth() - 22, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

juce::Component* LookAndFeel::getParentComponentForMenuOptions(const juce::PopupMenu::Options& o) {
    if (auto* parent = o.getParentComponent()) return parent;
    if (auto* target = o.getTargetComponent()) return target->getTopLevelComponent();
    return nullptr;
}

void LookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(raised);
    g.setColour(line);
    g.drawRect(0, 0, w, h);
}

}  // namespace pat::ui
