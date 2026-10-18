// PatSuite look: black, neon, elegant. Every plugin shares these colours and fonts; each one picks
// its own accent (de-esser cyan, sidechain pink, doubler lime, tune violet, panner gold,
// delay orange, EQ ice).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pat::ui {

namespace colours {
inline const juce::Colour background{0xff05060a};
inline const juce::Colour panel{0xff0d1017};
inline const juce::Colour raised{0xff161a24};
inline const juce::Colour hover{0xff1f2533};
inline const juce::Colour line{0xff252b3a};
inline const juce::Colour grid{0xff151a26};
inline const juce::Colour text{0xffe8ebf3};
inline const juce::Colour dim{0xff8a93a8};
inline const juce::Colour faint{0xff4c5468};
inline const juce::Colour cyan{0xff00e5ff};
inline const juce::Colour pink{0xffff2bd6};
inline const juce::Colour lime{0xff39ff88};
inline const juce::Colour violet{0xffa060ff};
inline const juce::Colour gold{0xffffd166};
inline const juce::Colour orange{0xffff8a3d};
inline const juce::Colour ice{0xff5cc8ff};
inline const juce::Colour hot{0xffff4d6d};  // clipping / over threshold
}  // namespace colours

juce::Font font(float height, bool bold = false);

// Rounded panel with an optional title; `accent` draws a small colour tick before the title.
void drawPanel(juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title = {}, juce::Colour accent = {});

// A neon stroke: a soft wider halo underneath (only when `glow` is allowed) and the bright line.
void strokeNeon(juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, bool glow);

class LookAndFeel final : public juce::LookAndFeel_V4 {
public:
    LookAndFeel();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int h) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(12.5f); }
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return font(13.5f); }
    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
};

}  // namespace pat::ui
