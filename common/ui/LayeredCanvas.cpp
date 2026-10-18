#include "common/ui/LayeredCanvas.h"

namespace pat::ui {

LayeredCanvas::LayeredCanvas(PerfSettings& settings) : settings_(settings) {
    settings_.addChangeListener(this);
    setOpaque(true);
}

LayeredCanvas::~LayeredCanvas() { settings_.removeChangeListener(this); }

void LayeredCanvas::paint(juce::Graphics& g) {
    // Render the static layer at the real pixel density so it stays sharp when the editor is scaled.
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (!staticValid_ || std::abs(scale - cacheScale_) > 0.01f || !cache_.isValid()) {
        const int w = juce::jmax(1, juce::roundToInt(static_cast<float>(getWidth()) * scale));
        const int h = juce::jmax(1, juce::roundToInt(static_cast<float>(getHeight()) * scale));
        cache_ = juce::Image(juce::Image::RGB, w, h, false);
        juce::Graphics cg(cache_);
        cg.addTransform(juce::AffineTransform::scale(scale));
        cg.fillAll(colours::background);
        paintStatic(cg);
        cacheScale_ = scale;
        staticValid_ = true;
    }
    g.drawImage(cache_, getLocalBounds().toFloat());
    paintDynamic(g);
}

}  // namespace pat::ui
