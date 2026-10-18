#include "common/ui/SpectrumView.h"

namespace pat::ui {
namespace {
constexpr int kMaxFft = 4096;
constexpr float kMinDb = -90.0f, kMaxDb = 0.0f;
constexpr float kMinHz = 20.0f, kMaxHz = 20000.0f;
}  // namespace

SpectrumView::SpectrumView(PerfSettings& settings, SpscFifo<float>& fifo, std::function<double()> sampleRate, juce::Colour colour)
    : LayeredCanvas(settings), fifo_(fifo), sampleRate_(std::move(sampleRate)), colour_(colour),
      history_(kMaxFft, 0.0f), scratchIn_(kMaxFft), mags_(kMaxFft / 2 + 1), window_(kMaxFft), scratch_(kMaxFft) {}

float SpectrumView::xForHz(float hz, float width) noexcept {
    return width * std::log(juce::jlimit(kMinHz, kMaxHz, hz) / kMinHz) / std::log(kMaxHz / kMinHz);
}

float SpectrumView::hzForX(float x, float width) noexcept {
    return kMinHz * std::pow(kMaxHz / kMinHz, juce::jlimit(0.0f, 1.0f, x / juce::jmax(1.0f, width)));
}

void SpectrumView::setHighlightIntensity(float v) {
    v = juce::jlimit(0.0f, 1.0f, v);
    if (std::abs(v - hlIntensity_) > 0.02f) {
        hlIntensity_ = v;
        repaint();
    }
}

const Fft& SpectrumView::fftFor(int size) {
    auto& f = ffts_[size];
    if (!f) f = std::make_unique<Fft>(size);
    return *f;
}

void SpectrumView::setHighlight(float lowHz, float highHz, juce::Colour c) {
    if (highlight_ && lowHz == hlLow_ && highHz == hlHigh_ && c == hlColour_) return;
    highlight_ = true;
    hlLow_ = lowHz;
    hlHigh_ = highHz;
    hlColour_ = c;
    repaint();
}

void SpectrumView::tick(float /*dt*/) {
    const auto profile = settings().profile();
    const int n = profile.fftSize;
    const double sr = juce::jmax(8000.0, sampleRate_());

    // Bring in the newest audio (the UI only cares about the latest window, never a backlog).
    std::size_t fresh = 0;
    if (testSignal_) {
        for (int i = 0; i < 1024; ++i) {
            testPhase_ += 2.0 * juce::MathConstants<double>::pi * 220.0 / sr;
            const float s = 0.25f * static_cast<float>(std::sin(testPhase_) + 0.5 * std::sin(testPhase_ * 3.0) + 0.3 * std::sin(testPhase_ * 23.0)) +
                            0.03f * (rng_.nextFloat() * 2.0f - 1.0f);
            std::rotate(history_.begin(), history_.begin() + 1, history_.end());
            history_.back() = s;
        }
        fresh = 1024;
    } else {
        fifo_.skipToNewest(kMaxFft);
        const std::size_t avail = fifo_.available();
        if (avail > 0) {
            const std::size_t take = std::min<std::size_t>(avail, kMaxFft);
            std::rotate(history_.begin(), history_.begin() + static_cast<std::ptrdiff_t>(take), history_.end());
            fresh = fifo_.pop(history_.data() + (kMaxFft - take), take);
        }
    }
    if (fresh == 0 && profile.repaintOnlyOnChange && !display_.empty()) return;

    // Windowed FFT of the newest n samples.
    for (int i = 0; i < n; ++i) {
        const float w = 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(n - 1));
        scratchIn_[static_cast<std::size_t>(i)] = history_[static_cast<std::size_t>(kMaxFft - n + i)] * w;
    }
    fftFor(n).realMagnitudes(scratchIn_.data(), mags_.data(), scratch_.data());
    const float norm = 4.0f / static_cast<float>(n);

    // Points to draw: a few log-spaced bands (Low) or one per pixel.
    const int points = profile.spectrumBands > 0 ? profile.spectrumBands : juce::jmax(16, getWidth());
    std::vector<float> target(static_cast<std::size_t>(points));
    for (int p = 0; p < points; ++p) {
        const float f0 = kMinHz * std::pow(kMaxHz / kMinHz, static_cast<float>(p) / static_cast<float>(points));
        const float f1 = kMinHz * std::pow(kMaxHz / kMinHz, static_cast<float>(p + 1) / static_cast<float>(points));
        const int b0 = juce::jlimit(1, n / 2, static_cast<int>(f0 / sr * n));
        const int b1 = juce::jlimit(b0, n / 2, static_cast<int>(f1 / sr * n));
        float m = 0;
        for (int b = b0; b <= b1; ++b) m = juce::jmax(m, mags_[static_cast<std::size_t>(b)]);
        target[static_cast<std::size_t>(p)] = juce::jlimit(kMinDb, kMaxDb, 20.0f * std::log10(m * norm + 1e-9f));
    }
    if (display_.size() != target.size()) display_.assign(target.size(), kMinDb - 1.0f);
    const bool smooth = profile.smoothing && animated();
    float maxDelta = 0;
    for (std::size_t i = 0; i < target.size(); ++i) {
        float& d = display_[i];
        const float before = d;
        d = (!smooth || target[i] > d) ? target[i] : d + (target[i] - d) * 0.25f;  // eased fall
        maxDelta = juce::jmax(maxDelta, std::abs(d - before));
    }
    // Low level: skip the repaint when the picture would look the same (e.g. silence).
    if (profile.repaintOnlyOnChange && maxDelta < 0.5f) return;
    repaint();
}

void SpectrumView::paintStatic(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff07090f));
    g.fillRoundedRectangle(r, 6.0f);
    g.setFont(font(10.0f));
    for (float hz : {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f}) {
        const float x = xForHz(hz, r.getWidth());
        g.setColour(colours::grid);
        g.drawVerticalLine(juce::roundToInt(x), r.getY(), r.getBottom());
        g.setColour(colours::faint);
        g.drawText(hz >= 1000.0f ? juce::String(juce::roundToInt(hz / 1000.0f)) + "k" : juce::String(juce::roundToInt(hz)),
                   juce::Rectangle<float>(x + 3.0f, r.getBottom() - 15.0f, 30.0f, 12.0f), juce::Justification::left);
    }
    for (float db = -80.0f; db < 0.0f; db += 20.0f) {
        const float y = r.getY() + r.getHeight() * (db - kMaxDb) / (kMinDb - kMaxDb);
        g.setColour(colours::grid);
        g.drawHorizontalLine(juce::roundToInt(y), r.getX(), r.getRight());
        g.setColour(colours::faint);
        g.drawText(juce::String(juce::roundToInt(db)), juce::Rectangle<float>(4.0f, y + 1.0f, 30.0f, 12.0f), juce::Justification::left);
    }
    g.setColour(colours::line);
    g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
}

void SpectrumView::paintDynamic(juce::Graphics& g) {
    const auto r = getLocalBounds().toFloat();
    if (highlight_) {
        const float x0 = xForHz(hlLow_, r.getWidth()), x1 = xForHz(hlHigh_, r.getWidth());
        const auto band = juce::Rectangle<float>::leftTopRightBottom(x0, r.getY(), x1, r.getBottom());
        g.setColour(hlColour_.withAlpha(0.08f + 0.22f * hlIntensity_));
        g.fillRect(band);
        if (glow() && hlIntensity_ > 0.01f) {  // soft bloom while the band is active
            g.setGradientFill(juce::ColourGradient(hlColour_.withAlpha(0.35f * hlIntensity_), band.getCentreX(), r.getY(),
                                                   hlColour_.withAlpha(0.0f), band.getCentreX(), r.getBottom(), false));
            g.fillRect(band);
        }
        g.setColour(hlColour_.withAlpha(0.6f + 0.4f * hlIntensity_));
        g.drawVerticalLine(juce::roundToInt(x0), r.getY(), r.getBottom());
        g.drawVerticalLine(juce::roundToInt(x1), r.getY(), r.getBottom());
    }
    if (display_.size() < 2) return;
    juce::Path line, fill;
    const float dx = r.getWidth() / static_cast<float>(display_.size() - 1);
    for (std::size_t i = 0; i < display_.size(); ++i) {
        const float x = dx * static_cast<float>(i);
        const float y = r.getY() + r.getHeight() * (display_[i] - kMaxDb) / (kMinDb - kMaxDb);
        if (i == 0) {
            line.startNewSubPath(x, y);
            fill.startNewSubPath(x, r.getBottom());
        }
        line.lineTo(x, y);
        fill.lineTo(x, y);
    }
    fill.lineTo(r.getRight(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill(juce::ColourGradient(colour_.withAlpha(0.22f), 0, r.getY(), colour_.withAlpha(0.02f), 0, r.getBottom(), false));
    g.fillPath(fill);
    strokeNeon(g, line, colour_, 1.6f, glow());
}

}  // namespace pat::ui
