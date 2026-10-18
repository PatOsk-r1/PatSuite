#include "ui/deesser/DeEsserEditor.h"

using namespace pat::ui;
using namespace deesser;

namespace {
const juce::Colour kAccent = colours::cyan;  // plugin accent (spectrum, knobs)
const juce::Colour kBand = colours::pink;    // the sibilance band
constexpr float kMeterFloorDb = -60.0f;
constexpr float kMaxReductionDb = 24.0f;

float bandLow(float f, float w) { return f / std::exp2(w * 0.5f); }
float bandHigh(float f, float w) { return f * std::exp2(w * 0.5f); }
}  // namespace

// ------------------------------------------------------------------------------------------- band overlay
// Transparent layer over the spectrum: drag the band (frequency) or its edges (width).
class DeEsserEditor::BandOverlay final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit BandOverlay(DeEsserEditor& ed) : ed_(ed) {
        setTooltip("Drag the band to move it. Drag its edges to make it wider or narrower.");
    }
    void paint(juce::Graphics& g) override {
        const float f = ed_.param(kFreq), w = ed_.param(kWidth), width = static_cast<float>(getWidth());
        const float x0 = SpectrumView::xForHz(bandLow(f, w), width), x1 = SpectrumView::xForHz(bandHigh(f, w), width);
        const float xc = SpectrumView::xForHz(f, width);
        const auto active = [this](int part) { return drag_ == part || (drag_ == 0 && hover_ == part); };
        // Edge grips and centre handle.
        for (int side = 0; side < 2; ++side) {
            const float x = side ? x1 : x0;
            g.setColour(kBand.withAlpha(active(side ? 3 : 2) ? 1.0f : 0.7f));
            g.fillRoundedRectangle(x - 3.0f, static_cast<float>(getHeight()) * 0.5f - 18.0f, 6.0f, 36.0f, 3.0f);
        }
        juce::Path diamond;
        diamond.addStar({xc, 14.0f}, 4, 5.0f, active(1) ? 10.0f : 8.0f, juce::MathConstants<float>::pi * 0.25f);
        g.setColour(kBand);
        g.fillPath(diamond);
        g.setColour(colours::text);
        g.setFont(font(11.0f, true));
        const juce::String label = (f >= 1000.0f ? juce::String(f / 1000.0f, 1) + " kHz" : juce::String(juce::roundToInt(f)) + " Hz");
        g.drawText(label, juce::Rectangle<float>(xc - 40.0f, 24.0f, 80.0f, 14.0f), juce::Justification::centred);
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const int h = partAt(e.position.x);
        if (h != hover_) {
            hover_ = h;
            setMouseCursor(h == 2 || h == 3 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override {
        hover_ = 0;
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        drag_ = juce::jmax(1, partAt(e.position.x));
        for (const char* id : {kFreq, kWidth})
            if (auto* p = ed_.proc_.state.getParameter(id)) p->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        const float width = static_cast<float>(getWidth());
        const float hz = SpectrumView::hzForX(e.position.x, width);
        if (drag_ == 1) {
            ed_.setParam(kFreq, hz);
        } else {
            // Width = twice the distance in octaves from the centre to the dragged edge.
            const float f = ed_.param(kFreq);
            ed_.setParam(kWidth, 2.0f * std::abs(std::log2(hz / f)));
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        drag_ = 0;
        for (const char* id : {kFreq, kWidth})
            if (auto* p = ed_.proc_.state.getParameter(id)) p->endChangeGesture();
        repaint();
    }

private:
    // 0 = nothing, 1 = band body, 2 = left edge, 3 = right edge
    int partAt(float x) const {
        const float f = ed_.param(kFreq), w = ed_.param(kWidth), width = static_cast<float>(getWidth());
        const float x0 = SpectrumView::xForHz(bandLow(f, w), width), x1 = SpectrumView::xForHz(bandHigh(f, w), width);
        if (std::abs(x - x0) < 8.0f) return 2;
        if (std::abs(x - x1) < 8.0f) return 3;
        return (x > x0 && x < x1) ? 1 : 0;
    }
    DeEsserEditor& ed_;
    int hover_ = 0, drag_ = 0;
};

// ------------------------------------------------------------------------------------------- sibilance meter
// How loud the sibilance is, with the threshold as a draggable line.
class DeEsserEditor::SibilanceMeter final : public juce::Component, public juce::SettableTooltipClient {
public:
    SibilanceMeter(DeEsserEditor& ed, PerfSettings& s) : ed_(ed), settings_(s) {
        setTooltip("Sibilance level. Drag the line to set the threshold: above it, the de-esser works.");
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }
    void update(float db, float dt) {
        const float before = shown_;
        shown_ = (settings_.neonFixed() || db > shown_) ? db : shown_ - 30.0f * dt;
        shown_ = juce::jmax(shown_, kMeterFloorDb - 1.0f);
        const float thr = ed_.param(kThreshold);
        if (std::abs(before - shown_) > 0.2f || thr != lastThr_) {
            lastThr_ = thr;
            repaint();
        }
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        const auto labelArea = r.removeFromBottom(16.0f);
        const auto bar = r.withSizeKeepingCentre(16.0f, r.getHeight());
        g.setColour(colours::raised);
        g.fillRoundedRectangle(bar, 4.0f);
        const float thr = ed_.param(kThreshold);
        const float yThr = yFor(thr, bar), yLvl = yFor(shown_, bar);
        if (yLvl < bar.getBottom()) {
            g.setColour(kAccent.withAlpha(0.7f));
            g.fillRoundedRectangle(bar.withTop(juce::jmax(yLvl, yThr)), 4.0f);
            if (yLvl < yThr) {  // the part above the threshold glows in the band colour
                g.setColour(kBand);
                g.fillRoundedRectangle(bar.withTop(yLvl).withBottom(yThr), 4.0f);
            }
        }
        g.setColour(colours::text);
        g.fillRect(bar.getX() - 8.0f, yThr - 1.0f, bar.getWidth() + 16.0f, 2.0f);
        g.setFont(font(10.5f, true));
        g.drawText(juce::String(juce::roundToInt(thr)) + " dB", juce::Rectangle<float>(r.getX(), yThr - 16.0f, r.getWidth(), 12.0f),
                   juce::Justification::centred);
        g.setColour(colours::dim);
        g.drawText("SIBILANCE", labelArea, juce::Justification::centred);
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (auto* p = ed_.proc_.state.getParameter(kThreshold)) p->beginChangeGesture();
        mouseDrag(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        auto r = getLocalBounds().toFloat();
        r.removeFromBottom(16.0f);
        const float t = juce::jlimit(0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        ed_.setParam(kThreshold, kMeterFloorDb * (1.0f - t));
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (auto* p = ed_.proc_.state.getParameter(kThreshold)) p->endChangeGesture();
    }

private:
    static float yFor(float db, juce::Rectangle<float> bar) {
        return bar.getBottom() - bar.getHeight() * juce::jlimit(0.0f, 1.0f, (db - kMeterFloorDb) / -kMeterFloorDb);
    }
    DeEsserEditor& ed_;
    PerfSettings& settings_;
    float shown_ = kMeterFloorDb, lastThr_ = 1;
};

// ------------------------------------------------------------------------------------------- reduction meter
// How much the de-esser is turning down right now (bar grows downwards).
class DeEsserEditor::ReductionMeter final : public juce::Component {
public:
    explicit ReductionMeter(PerfSettings& s) : settings_(s) { setInterceptsMouseClicks(false, false); }
    void update(float grDb, float dt) {
        const float target = juce::jlimit(0.0f, kMaxReductionDb, -grDb);
        const float before = shown_;
        shown_ = (settings_.neonFixed() || target > shown_) ? target : shown_ - 20.0f * dt;
        shown_ = juce::jmax(0.0f, shown_);
        if (std::abs(before - shown_) > 0.1f) repaint();
    }
    float shown() const noexcept { return shown_; }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        const auto labelArea = r.removeFromBottom(16.0f);
        const auto valueArea = r.removeFromBottom(16.0f);
        const auto bar = r.withSizeKeepingCentre(16.0f, r.getHeight());
        g.setColour(colours::raised);
        g.fillRoundedRectangle(bar, 4.0f);
        if (shown_ > 0.05f) {
            const auto fill = bar.withHeight(bar.getHeight() * shown_ / kMaxReductionDb);
            g.setGradientFill(juce::ColourGradient(kBand, fill.getX(), fill.getY(), kBand.withAlpha(0.5f), fill.getX(), fill.getBottom(), false));
            g.fillRoundedRectangle(fill, 4.0f);
        }
        g.setColour(colours::faint);
        g.setFont(font(9.5f));
        for (int db : {6, 12, 18}) {
            const float y = bar.getY() + bar.getHeight() * static_cast<float>(db) / kMaxReductionDb;
            g.drawText(juce::String(db), juce::Rectangle<float>(bar.getRight() + 3.0f, y - 6.0f, 20.0f, 12.0f), juce::Justification::left);
        }
        g.setColour(colours::text);
        g.setFont(font(11.0f, true));
        g.drawText(shown_ > 0.05f ? "-" + juce::String(shown_, 1) + " dB" : "0 dB", valueArea, juce::Justification::centred);
        g.setColour(colours::dim);
        g.setFont(font(10.5f, true));
        g.drawText("REDUCTION", labelArea, juce::Justification::centred);
    }

private:
    PerfSettings& settings_;
    float shown_ = 0;
};

// ------------------------------------------------------------------------------------------- editor

DeEsserEditor::DeEsserEditor(DeEsserProcessor& p)
    : PluginFrame(p, p.state, "PatDeEsser", "sibilance control", kAccent, 940, 600),
      proc_(p),
      spectrum_(settings(), p.analysis, [&p] { return p.sampleRate.load(); }, kAccent),
      smooth_("SMOOTH", kAccent, settings(), NeonKnob::Size::Big),
      freq_(p.state, kFreq, "FREQUENCY", kAccent, settings()),
      width_(p.state, kWidth, "WIDTH", kAccent, settings()),
      threshold_(p.state, kThreshold, "THRESHOLD", kAccent, settings()),
      ratio_(p.state, kRatio, "RATIO", kAccent, settings()),
      release_(p.state, kRelease, "RELEASE", kAccent, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      listenAttachment_(p.state, kListen, listen_) {
    overlay_ = std::make_unique<BandOverlay>(*this);
    sibilance_ = std::make_unique<SibilanceMeter>(*this, settings());
    reduction_ = std::make_unique<ReductionMeter>(settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{&spectrum_, overlay_.get(), sibilance_.get(), reduction_.get(), &smooth_, &freq_,
                                                                       &width_, &threshold_, &ratio_, &release_, &output_, &split_, &wide_, &listen_,
                                                                       &gentle_, &strong_, &hint_})
        content().addAndMakeVisible(c);

    // "Smooth" macro: moves threshold and ratio together.
    smooth_.setRange(0.0, 1.0);
    smooth_.setDoubleClickReturnValue(true, 0.5);
    smooth_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
    smooth_.setTooltip("How much sibilance to remove. Moves threshold and ratio together (see them in Pro).");
    smooth_.onDragStart = [this] {
        for (const char* id : {kThreshold, kRatio})
            if (auto* prm = proc_.state.getParameter(id)) prm->beginChangeGesture();
    };
    smooth_.onDragEnd = [this] {
        for (const char* id : {kThreshold, kRatio})
            if (auto* prm = proc_.state.getParameter(id)) prm->endChangeGesture();
    };
    smooth_.onValueChange = [this] {
        const auto v = static_cast<float>(smooth_.getValue());
        setParam(kThreshold, smoothToThreshold(v));
        setParam(kRatio, smoothToRatio(v));
    };

    for (auto* b : {&split_, &wide_}) {
        b->setColour(juce::TextButton::buttonOnColourId, kBand);
        b->setClickingTogglesState(false);
    }
    split_.setTooltip("Split band: only the sibilance band is turned down (most natural)");
    wide_.setTooltip("Wide band: the whole voice dips while an 's' sounds (classic)");
    split_.onClick = [this] { setParam(kMode, 0.0f); };
    wide_.onClick = [this] { setParam(kMode, 1.0f); };
    listen_.setColour(juce::ToggleButton::tickColourId, kBand);
    listen_.setTooltip("Listen: hear only the band being detected, to find the sibilance");

    for (auto* l : {&gentle_, &strong_, &hint_}) {
        l->setFont(font(12.0f, true));
        l->setColour(juce::Label::textColourId, colours::dim);
    }
    gentle_.setText("GENTLE", juce::dontSendNotification);
    strong_.setText("STRONG", juce::dontSendNotification);
    gentle_.setJustificationType(juce::Justification::centredRight);
    hint_.setText("Turn SMOOTH up until the harsh 's' sounds calm down.", juce::dontSendNotification);
    hint_.setFont(font(12.0f));
    hint_.setJustificationType(juce::Justification::centred);

    clock().add([this](float dt) {
        if (!simulating_) tick(dt, proc_.detectorDb.load(), proc_.reductionDb.load());
    });
    finishConstruction();
}

DeEsserEditor::~DeEsserEditor() = default;

float DeEsserEditor::param(const char* id) const { return proc_.state.getRawParameterValue(id)->load(); }

void DeEsserEditor::setParam(const char* id, float value) {
    if (auto* p = proc_.state.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
}

void DeEsserEditor::tick(float dt, float detectorDb, float reductionDb) {
    spectrum_.tick(dt);
    const float f = param(kFreq), w = param(kWidth);
    spectrum_.setHighlight(bandLow(f, w), bandHigh(f, w), kBand);
    reduction_->update(reductionDb, dt);
    // Neon fixed: the band shows a steady brightness instead of pulsing with each "s".
    spectrum_.setHighlightIntensity(settings().neonFixed() ? 0.4f : reduction_->shown() / 12.0f);
    sibilance_->update(detectorDb, dt);
    if (!smooth_.isMouseButtonDown()) smooth_.setValue(thresholdToSmooth(param(kThreshold)), juce::dontSendNotification);
    const bool split = param(kMode) < 0.5f;
    split_.setToggleState(split, juce::dontSendNotification);
    wide_.setToggleState(!split, juce::dontSendNotification);
    if (f != lastFreq_ || w != lastWidth_) {  // the handles only move when the band changes
        lastFreq_ = f;
        lastWidth_ = w;
        overlay_->repaint();
    }
}

void DeEsserEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&freq_, &width_, &threshold_, &ratio_, &release_, &output_, &split_, &wide_,
                                                                       &listen_, sibilance_.get(), overlay_.get()})
        c->setVisible(pro);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&smooth_, &gentle_, &strong_, &hint_}) c->setVisible(!pro);
}

void DeEsserEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    auto meters = r.removeFromRight(proView() ? 150 : 80);
    r.removeFromRight(12);
    if (proView()) {
        auto controls = r.removeFromBottom(150);
        r.removeFromBottom(12);
        spectrum_.setBounds(r);
        overlay_->setBounds(r);
        auto buttons = controls.removeFromRight(170).reduced(0, 10);
        split_.setBounds(buttons.removeFromTop(34).reduced(4, 2));
        wide_.setBounds(buttons.removeFromTop(34).reduced(4, 2));
        buttons.removeFromTop(10);
        listen_.setBounds(buttons.removeFromTop(30).reduced(10, 2));
        const int kw = controls.getWidth() / 6;
        for (auto* k : {&freq_, &width_, &threshold_, &ratio_, &release_, &output_}) k->setBounds(controls.removeFromLeft(kw).reduced(2, 12));
        sibilance_->setBounds(meters.removeFromLeft(75).withTrimmedBottom(0));
        reduction_->setBounds(meters);
    } else {
        auto bottom = r.removeFromBottom(230);
        r.removeFromBottom(10);
        spectrum_.setBounds(r);
        overlay_->setBounds(r);
        hint_.setBounds(bottom.removeFromBottom(22));
        auto knob = bottom.withSizeKeepingCentre(220, bottom.getHeight());
        smooth_.setBounds(knob);
        gentle_.setBounds(knob.getX() - 110, knob.getCentreY() - 10, 100, 20);
        strong_.setBounds(knob.getRight() + 10, knob.getCentreY() - 10, 100, 20);
        reduction_->setBounds(meters);
    }
}

void DeEsserEditor::simulateForSnapshot(float seconds) {
    simulating_ = true;
    spectrum_.useTestSignal(true);
    for (float t = 0; t < seconds; t += 1.0f / 30.0f) tick(1.0f / 30.0f, -16.0f, -8.0f);
    simulating_ = false;
}
