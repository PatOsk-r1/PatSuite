#include "ui/doubler/DoublerEditor.h"

using namespace pat::ui;
using namespace doubler;

namespace {
const juce::Colour kAccent = colours::lime;
const juce::Colour kCopyColour[4] = {colours::cyan, colours::pink, colours::violet, colours::gold};
constexpr int kWaveSamples = 2048;
constexpr int kScopePoints = 2048;
}  // namespace

// ------------------------------------------------------------------------------------------- wave stack
class DoublerEditor::WaveStack final : public LayeredCanvas {
public:
    WaveStack(PerfSettings& s, DoublerProcessor& p) : LayeredCanvas(s), proc_(p), samples_(kWaveSamples, 0.0f) {}

    void tick(float dt) {
        bool changed = false;
        if (feedTest_) {
            for (int i = 0; i < 256; ++i) push(0.5f * std::sin(testPhase_ += 0.031f) + 0.25f * std::sin(testPhase_ * 2.7f));
            changed = true;
        } else {
            proc_.wave.skipToNewest(kWaveSamples);
            float buf[1024];
            std::size_t n;
            while ((n = proc_.wave.pop(buf, 1024)) > 0) {
                for (std::size_t i = 0; i < n; ++i) push(buf[i]);
                changed = true;
            }
        }
        // Copies glide to their new places (instantly in "Neon fixed").
        const auto s = proc_.settings();
        const float k = animated() ? 1.0f - std::exp(-dt * 10.0f) : 1.0f;
        for (int c = 0; c < 4; ++c) {
            auto& v = view_[static_cast<std::size_t>(c)];
            const float tgtOffset = pat::dsp::DoublerEngine::copyPan(c, s);
            const float tgtShift = s.delayMs[static_cast<std::size_t>(c)];
            const float tgtGain = pat::dsp::DoublerEngine::copyGain(c, s) / 0.7f;
            if (std::abs(v.offset - tgtOffset) + std::abs(v.shift - tgtShift) + std::abs(v.gain - tgtGain) > 0.002f) changed = true;
            v.offset += (tgtOffset - v.offset) * k;
            v.shift += (tgtShift - v.shift) * k;
            v.gain += (tgtGain - v.gain) * k;
        }
        if (changed || !settings().profile().repaintOnlyOnChange) repaint();
    }
    void feedTestSignal(bool on) { feedTest_ = on; }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(colours::grid);
        g.drawHorizontalLine(juce::roundToInt(r.getCentreY()), r.getX(), r.getRight());
        g.setFont(font(10.5f, true));
        g.setColour(colours::faint);
        g.drawText("L", r.reduced(10.0f, 6.0f), juce::Justification::topLeft);
        g.drawText("R", r.reduced(10.0f, 6.0f), juce::Justification::bottomLeft);
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }
    void paintDynamic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat().reduced(14.0f, 10.0f);
        const float sr = static_cast<float>(proc_.sampleRate.load());
        const int span = juce::jmin(kWaveSamples - 400, static_cast<int>(0.025f * sr));  // 25 ms visible
        const float amp = r.getHeight() * 0.16f;
        const auto s = proc_.settings();
        const int step = settings().profile().spectrumBands > 0 ? 6 : 2;
        const auto wave = [&](float shiftMs, float yOffset) {
            juce::Path p;
            const int shift = static_cast<int>((shiftMs + 16.0f) * 0.001f * sr * 0.08f);  // scaled so delays read as offsets
            for (int i = 0; i <= span; i += step) {
                const int idx = (head_ - span - shift + i + kWaveSamples * 2) % kWaveSamples;
                const float x = r.getX() + r.getWidth() * static_cast<float>(i) / static_cast<float>(span);
                const float y = r.getCentreY() + yOffset - samples_[static_cast<std::size_t>(idx)] * amp;
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            return p;
        };
        for (int c = 3; c >= 0; --c) {  // back to front
            const auto& v = view_[static_cast<std::size_t>(c)];
            if (v.gain < 0.02f || c >= s.copies) continue;
            const auto col = kCopyColour[c].withAlpha(juce::jlimit(0.15f, 1.0f, v.gain));
            strokeNeon(g, wave(v.shift, v.offset * r.getHeight() * 0.33f), col, 1.3f + 1.2f * s.thickness, glow());
        }
        strokeNeon(g, wave(0.0f, 0.0f), colours::text, 1.8f, glow());  // the dry voice in front
    }

private:
    void push(float v) {
        samples_[static_cast<std::size_t>(head_)] = v;
        head_ = (head_ + 1) % kWaveSamples;
    }
    struct View {
        float offset = 0, shift = 0, gain = 0;
    };
    DoublerProcessor& proc_;
    std::vector<float> samples_;
    int head_ = 0;
    std::array<View, 4> view_{};
    bool feedTest_ = false;
    float testPhase_ = 0;
};

// ------------------------------------------------------------------------------------------- vectorscope
class DoublerEditor::Vectorscope final : public LayeredCanvas {
public:
    Vectorscope(PerfSettings& s, DoublerProcessor& p) : LayeredCanvas(s), proc_(p), points_(kScopePoints) {}

    void tick() {
        pat::dsp::StereoSample buf[512];
        std::size_t n;
        bool fresh = false;
        proc_.scope.skipToNewest(kScopePoints);
        while ((n = proc_.scope.pop(buf, 512)) > 0) {
            for (std::size_t i = 0; i < n; ++i) {
                points_[static_cast<std::size_t>(head_)] = buf[i];
                head_ = (head_ + 1) % kScopePoints;
            }
            fresh = true;
        }
        if (fresh || !settings().profile().repaintOnlyOnChange) repaint();
    }
    void fill(const std::vector<pat::dsp::StereoSample>& pts) {
        for (const auto& p : pts) {
            points_[static_cast<std::size_t>(head_)] = p;
            head_ = (head_ + 1) % kScopePoints;
        }
        repaint();
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        const auto c = r.getCentre();
        const float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.42f;
        g.setColour(colours::grid);
        g.drawEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(c), 1.0f);
        g.drawLine(c.x, c.y - rad, c.x, c.y + rad);
        g.drawLine(c.x - rad, c.y, c.x + rad, c.y);
        g.setFont(font(10.0f, true));
        g.setColour(colours::faint);
        g.drawText("M", juce::Rectangle<float>(c.x - 10, c.y - rad - 14, 20, 12), juce::Justification::centred);
        g.drawText("L", juce::Rectangle<float>(c.x - rad - 14, c.y - 6, 12, 12), juce::Justification::centred);
        g.drawText("R", juce::Rectangle<float>(c.x + rad + 2, c.y - 6, 12, 12), juce::Justification::centred);
        g.drawText("VECTORSCOPE", r.reduced(8.0f, 6.0f), juce::Justification::bottomLeft);
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }
    void paintDynamic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        const auto c = r.getCentre();
        const float rad = juce::jmin(r.getWidth(), r.getHeight()) * 0.42f;
        const int step = settings().profile().spectrumBands > 0 ? 4 : 1;  // Low level: a quarter of the dots
        g.setColour(kAccent.withAlpha(glow() ? 0.55f : 0.8f));
        for (int i = 0; i < kScopePoints; i += step) {
            const auto& p = points_[static_cast<std::size_t>(i)];
            const float x = c.x + juce::jlimit(-1.0f, 1.0f, (p.r - p.l) * 0.7071f) * rad;
            const float y = c.y - juce::jlimit(-1.0f, 1.0f, (p.l + p.r) * 0.7071f) * rad;
            g.fillRect(x, y, 1.6f, 1.6f);
        }
    }

private:
    DoublerProcessor& proc_;
    std::vector<pat::dsp::StereoSample> points_;
    int head_ = 0;
};

// ------------------------------------------------------------------------------------------- editor

DoublerEditor::DoublerEditor(DoublerProcessor& p)
    : PluginFrame(p, p.state, "PatDoubler", "vocal doubler", kAccent, 980, 640),
      proc_(p),
      width_(p.state, kWidth, "WIDTH", kAccent, settings(), NeonKnob::Size::Big),
      thickness_(p.state, kThickness, "THICKNESS", kAccent, settings(), NeonKnob::Size::Big),
      widthSmall_(p.state, kWidth, "WIDTH", kAccent, settings()),
      thicknessSmall_(p.state, kThickness, "THICKNESS", kAccent, settings()),
      variation_(p.state, kVariation, "VARIATION", kAccent, settings()),
      spread_(p.state, kSpread, "SPREAD", kAccent, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      copies_(p.state, kCopies, {"1", "2", "3", "4"}, kAccent) {
    waves_ = std::make_unique<WaveStack>(settings(), p);
    scope_ = std::make_unique<Vectorscope>(settings(), p);
    for (juce::Component* c : std::initializer_list<juce::Component*>{waves_.get(), scope_.get(), &width_, &thickness_, &widthSmall_, &thicknessSmall_,
                                                                       &variation_, &spread_, &output_, &copies_, &copiesLabel_})
        content().addAndMakeVisible(c);
    for (int c = 0; c < 4; ++c) {
        copyKnobs_.push_back(std::make_unique<NeonKnob>(p.state, delayId(c), "DELAY", kCopyColour[c], settings(), NeonKnob::Size::Small));
        copyKnobs_.push_back(std::make_unique<NeonKnob>(p.state, detuneId(c), "DETUNE", kCopyColour[c], settings(), NeonKnob::Size::Small));
        copyKnobs_.push_back(std::make_unique<NeonKnob>(p.state, panId(c), "PAN", kCopyColour[c], settings(), NeonKnob::Size::Small));
        auto& lbl = copyLabels_[static_cast<std::size_t>(c)];
        lbl.setText("COPY " + juce::String(c + 1), juce::dontSendNotification);
        lbl.setFont(font(11.0f, true));
        lbl.setColour(juce::Label::textColourId, kCopyColour[c]);
        content().addAndMakeVisible(lbl);
    }
    for (auto& k : copyKnobs_) content().addAndMakeVisible(*k);
    copiesLabel_.setText("COPIES", juce::dontSendNotification);
    copiesLabel_.setFont(font(11.5f, true));
    copiesLabel_.setColour(juce::Label::textColourId, colours::dim);
    width_.setTooltip("Width: how far apart the copies sit in the stereo field");
    thickness_.setTooltip("Thickness: how many copies you hear and how strong they are");

    clock().add([this](float dt) {
        if (simulating_) return;
        waves_->tick(dt);
        if (scope_->isVisible()) scope_->tick();
    });
    finishConstruction();
}

DoublerEditor::~DoublerEditor() = default;

void DoublerEditor::viewChanged(bool pro) {
    width_.setVisible(!pro);
    thickness_.setVisible(!pro);
    for (juce::Component* c : std::initializer_list<juce::Component*>{scope_.get(), &widthSmall_, &thicknessSmall_, &variation_, &spread_, &output_,
                                                                       &copies_, &copiesLabel_})
        c->setVisible(pro);
    for (auto& k : copyKnobs_) k->setVisible(pro);
    for (auto& l : copyLabels_) l.setVisible(pro);
}

void DoublerEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    if (!proView()) {
        waves_->setBounds(r.removeFromTop(330));
        r.removeFromTop(16);
        auto knobs = r.withSizeKeepingCentre(520, r.getHeight());
        width_.setBounds(knobs.removeFromLeft(260));
        thickness_.setBounds(knobs);
        return;
    }
    auto top = r.removeFromTop(250);
    scope_->setBounds(top.removeFromRight(250));
    top.removeFromRight(12);
    waves_->setBounds(top);
    r.removeFromTop(12);
    // Per-copy grid on the left, global controls on the right.
    auto right = r.removeFromRight(330);
    auto grid = r;
    const int rowH = grid.getHeight() / 4;
    for (int c = 0; c < 4; ++c) {
        auto row = grid.removeFromTop(rowH);
        copyLabels_[static_cast<std::size_t>(c)].setBounds(row.removeFromLeft(70));
        const int kw = row.getWidth() / 3;
        for (int k = 0; k < 3; ++k) copyKnobs_[static_cast<std::size_t>(c * 3 + k)]->setBounds(row.removeFromLeft(kw).reduced(2, 0));
    }
    auto cp = right.removeFromTop(52);
    copiesLabel_.setBounds(cp.removeFromTop(18));
    copies_.setBounds(cp.removeFromTop(30));
    right.removeFromTop(6);
    auto row1 = right.removeFromTop(right.getHeight() / 2);
    const int kw = row1.getWidth() / 3;
    widthSmall_.setBounds(row1.removeFromLeft(kw));
    thicknessSmall_.setBounds(row1.removeFromLeft(kw));
    variation_.setBounds(row1);
    spread_.setBounds(right.removeFromLeft(kw));
    output_.setBounds(right.removeFromLeft(kw));
}

void DoublerEditor::simulateForSnapshot(float seconds) {
    simulating_ = true;
    waves_->feedTestSignal(true);
    for (float t = 0; t < seconds; t += 1.0f / 30.0f) waves_->tick(1.0f / 30.0f);
    waves_->feedTestSignal(false);
    // A believable doubled-vocal cloud for the scope.
    std::vector<pat::dsp::StereoSample> pts;
    juce::Random rng(3);
    for (int i = 0; i < kScopePoints; ++i) {
        const float m = 0.5f * std::sin(i * 0.05f) * (0.6f + 0.4f * rng.nextFloat());
        const float sd = 0.18f * std::sin(i * 0.013f + 1.0f) * rng.nextFloat();
        pts.push_back({m + sd, m - sd});
    }
    scope_->fill(pts);
    simulating_ = false;
}
