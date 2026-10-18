#include "common/ui/LevelMeter.h"

namespace pat::ui {
namespace {
constexpr float kFloorDb = -60.0f;
float toDb(float g) { return g > 1e-6f ? 20.0f * std::log10(g) : -120.0f; }
}  // namespace

LevelMeter::LevelMeter(PerfSettings& settings, const juce::String& label, juce::Colour colour)
    : settings_(settings), label_(label), colour_(colour) {
    setInterceptsMouseClicks(false, false);
}

void LevelMeter::update(float peak, float dt) {
    const float db = toDb(peak);
    const float before = shownDb_, holdBefore = holdDb_;
    if (settings_.neonFixed()) {
        shownDb_ = db;  // discrete: no fall animation
        holdDb_ = db;
    } else {
        shownDb_ = db > shownDb_ ? db : shownDb_ - 24.0f * dt;  // 24 dB/s fall
        if (db >= holdDb_) {
            holdDb_ = db;
            holdTime_ = 0;
        } else if ((holdTime_ += dt) > 1.2f) {
            holdDb_ -= 12.0f * dt;
        }
    }
    shownDb_ = juce::jmax(shownDb_, -90.0f);
    holdDb_ = juce::jmax(holdDb_, -90.0f);
    // Repaint only when what is drawn changed (saves work on the Low level).
    if (std::abs(before - shownDb_) > 0.2f || std::abs(holdBefore - holdDb_) > 0.2f) repaint();
}

void LevelMeter::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    const auto labelArea = r.removeFromBottom(14.0f);
    const auto bar = r.withSizeKeepingCentre(juce::jmin(10.0f, r.getWidth()), r.getHeight());
    g.setColour(colours::raised);
    g.fillRoundedRectangle(bar, 3.0f);
    const auto yFor = [&](float db) { return bar.getBottom() - bar.getHeight() * juce::jlimit(0.0f, 1.0f, (db - kFloorDb) / -kFloorDb); };
    const float top = yFor(shownDb_);
    if (top < bar.getBottom() - 0.5f) {
        const auto fill = bar.withTop(top);
        g.setGradientFill(juce::ColourGradient(colour_.withAlpha(0.6f), bar.getX(), bar.getBottom(), shownDb_ > -1.0f ? colours::hot : colour_,
                                               bar.getX(), bar.getY(), false));
        g.fillRoundedRectangle(fill, 3.0f);
    }
    g.setColour(holdDb_ > -1.0f ? colours::hot : colours::text);
    g.fillRect(bar.getX(), yFor(holdDb_) - 1.0f, bar.getWidth(), 2.0f);
    g.setColour(colours::dim);
    g.setFont(font(10.0f, true));
    g.drawText(label_, labelArea, juce::Justification::centred);
}

}  // namespace pat::ui
