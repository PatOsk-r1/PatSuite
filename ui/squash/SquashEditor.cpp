#include "ui/squash/SquashEditor.h"

using namespace pat::ui;
using namespace squash;

namespace {
const juce::Colour kAccent = colours::mint;
const juce::Colour kDown = colours::coral;
const juce::Colour kUp = colours::ice;
const juce::Colour kBand[3] = {colours::pink, colours::violet, colours::cyan};
const char* kBandTitle[3] = {"LOW", "MID", "HIGH"};
constexpr float kTopDb = 0.0f, kBottomDb = -80.0f;
}  // namespace

// ------------------------------------------------------------------------------------------- bands
class SquashEditor::BandsView final : public LayeredCanvas {
public:
    BandsView(SquashProcessor& p, PerfSettings& s) : LayeredCanvas(s), proc_(p) {}

    void update(float dt) {
        bool changed = false;
        const float fall = settings().neonFixed() ? 0.0f : std::exp(-dt * 6.0f);
        for (int b = 0; b < 3; ++b) {
            const float lvl = simLevel_[b] > -200.0f ? simLevel_[b] : proc_.bandLevelDb[static_cast<std::size_t>(b)].load();
            const float gain = simLevel_[b] > -200.0f ? simGain_[b] : proc_.bandGainDb[static_cast<std::size_t>(b)].load();
            const float shownLvl = std::max(lvl, kBottomDb + (level_[b] - kBottomDb) * fall);
            if (std::abs(shownLvl - level_[b]) > 0.05f || std::abs(gain - gain_[b]) > 0.05f) changed = true;
            level_[b] = shownLvl;
            gain_[b] += (settings().neonFixed() ? 1.0f : juce::jmin(1.0f, dt * 15.0f)) * (gain - gain_[b]);
        }
        // Parameters can change from presets or automation.
        for (int b = 0; b < 3; ++b)
            for (int f : {DownThr, UpThr}) {
                const float v = proc_.state.getRawParameterValue(bandId(b, f))->load();
                if (v != thr_[b][f == DownThr ? 0 : 1]) {
                    thr_[b][f == DownThr ? 0 : 1] = v;
                    changed = true;
                }
            }
        const float lo = proc_.state.getRawParameterValue(kLowCross)->load(), hi = proc_.state.getRawParameterValue(kHighCross)->load();
        if (lo != lowHz_ || hi != highHz_) {
            lowHz_ = lo;
            highHz_ = hi;
            changed = true;
        }
        if (changed) repaint();
    }
    void simulate(int band, float levelDb, float gainDb) {
        simLevel_[band] = levelDb;
        simGain_[band] = gainDb;
    }

    void mouseDown(const juce::MouseEvent& e) override {
        drag_ = hit(e.position);
        if (drag_.band < 0) return;
        if (auto* p = param(drag_)) p->beginChangeGesture();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (drag_.band < 0) return;
        if (auto* p = param(drag_)) {
            const float db = juce::jlimit(drag_.down ? -60.0f : -80.0f, 0.0f, dbForY(e.position.y));
            p->setValueNotifyingHost(p->convertTo0to1(db));
        }
    }
    void mouseUp(const juce::MouseEvent&) override {
        if (drag_.band >= 0)
            if (auto* p = param(drag_)) p->endChangeGesture();
        drag_ = {};
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const auto h = hit(e.position);
        if (auto* p = param(h)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->getDefaultValue());
            p->endChangeGesture();
        }
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const auto h = hit(e.position);
        setMouseCursor(h.band >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
        if (h.band != hover_.band || h.down != hover_.down) {
            hover_ = h;
            repaint();
        }
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setFont(font(10.0f));
        for (int b = 0; b < 3; ++b) {
            const auto c = column(b);
            g.setColour(colours::panel);
            g.fillRoundedRectangle(c.expanded(4.0f, 22.0f).withTrimmedBottom(18.0f), 5.0f);
            for (float d = kTopDb; d >= kBottomDb; d -= 12.0f) {
                g.setColour(colours::grid);
                g.drawHorizontalLine(juce::roundToInt(yForDb(d)), c.getX(), c.getRight());
                if (b == 0) {
                    g.setColour(colours::faint);
                    g.drawText(juce::String(juce::roundToInt(d)), juce::Rectangle<float>(0, yForDb(d) - 6.0f, c.getX() - 8.0f, 12.0f),
                               juce::Justification::centredRight);
                }
            }
        }
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }

    void paintDynamic(juce::Graphics& g) override {
        for (int b = 0; b < 3; ++b) {
            const auto c = column(b);
            const auto col = kBand[b];
            // Title with the band range.
            g.setColour(col);
            g.setFont(font(12.5f, true));
            const juce::String range = b == 0 ? "< " + hzText(lowHz_) : b == 1 ? hzText(lowHz_) + " - " + hzText(highHz_) : "> " + hzText(highHz_);
            g.drawText(juce::String(kBandTitle[b]) + "   " + range, c.withHeight(18.0f).translated(0, -20.0f), juce::Justification::centredLeft);

            const float yDown = yForDb(thr_[b][0]), yUp = yForDb(thr_[b][1]);
            // Zones: turned down above the coral line, turned up below the ice line.
            g.setColour(kDown.withAlpha(0.10f));
            g.fillRect(juce::Rectangle<float>(c.getX(), c.getY(), c.getWidth(), yDown - c.getY()));
            g.setColour(kUp.withAlpha(0.08f));
            g.fillRect(juce::Rectangle<float>(c.getX(), yUp, c.getWidth(), yForDb(-70.0f) - yUp));

            // Level in (dim) and after squashing (neon).
            const float barW = c.getWidth() * 0.22f;
            const auto inBar = juce::Rectangle<float>(c.getCentreX() - barW - 4.0f, yForDb(level_[b]), barW, c.getBottom() - yForDb(level_[b]));
            g.setColour(colours::dim.withAlpha(0.35f));
            g.fillRoundedRectangle(inBar, 3.0f);
            const float outDb = juce::jlimit(kBottomDb, kTopDb, level_[b] + gain_[b]);
            const auto outBar = juce::Rectangle<float>(c.getCentreX() + 4.0f, yForDb(outDb), barW, c.getBottom() - yForDb(outDb));
            if (glow() && level_[b] > -70.0f) {
                g.setColour(col.withAlpha(0.18f));
                g.fillRoundedRectangle(outBar.expanded(4.0f, 2.0f), 5.0f);
            }
            g.setColour(col.withAlpha(0.85f));
            g.fillRoundedRectangle(outBar, 3.0f);
            // Gain change readout.
            const float gd = gain_[b];
            g.setFont(font(12.0f, true));
            g.setColour(gd > 0.05f ? kUp : gd < -0.05f ? kDown : colours::dim);
            g.drawText((gd > 0 ? "+" : "") + juce::String(gd, 1) + " dB", c.withY(c.getBottom() + 2.0f).withHeight(16.0f), juce::Justification::centred);

            // Threshold lines with handles.
            for (int k = 0; k < 2; ++k) {
                const bool down = k == 0;
                const float y = down ? yDown : yUp;
                const bool active = (hover_.band == b && hover_.down == down) || (drag_.band == b && drag_.down == down);
                const auto lc = down ? kDown : kUp;
                juce::Path line;
                line.startNewSubPath(c.getX(), y);
                line.lineTo(c.getRight(), y);
                strokeNeon(g, line, lc.withAlpha(active ? 1.0f : 0.8f), active ? 2.5f : 1.6f, glow() && active);
                g.setColour(lc);
                g.fillRoundedRectangle(juce::Rectangle<float>(14.0f, 8.0f).withCentre({c.getRight() - 12.0f, y}), 3.0f);
                g.setFont(font(9.5f, true));
                g.drawText(down ? "DOWN" : "UP", juce::Rectangle<float>(c.getX() + 3.0f, down ? y - 13.0f : y + 1.0f, 40.0f, 12.0f), juce::Justification::centredLeft);
            }
        }
    }

private:
    struct Hit {
        int band = -1;
        bool down = true;
    };
    static juce::String hzText(float hz) { return hz >= 1000.0f ? juce::String(hz / 1000.0f, 1) + "k" : juce::String(juce::roundToInt(hz)); }
    juce::Rectangle<float> column(int b) const {
        auto r = getLocalBounds().toFloat().reduced(10.0f).withTrimmedLeft(30.0f).withTrimmedTop(26.0f).withTrimmedBottom(22.0f);
        const float w = r.getWidth() / 3.0f;
        return juce::Rectangle<float>(r.getX() + w * static_cast<float>(b), r.getY(), w, r.getHeight()).reduced(14.0f, 0.0f);
    }
    float yForDb(float db) const {
        const auto c = column(0);
        return c.getY() + (kTopDb - juce::jlimit(kBottomDb, kTopDb, db)) / (kTopDb - kBottomDb) * c.getHeight();
    }
    float dbForY(float y) const {
        const auto c = column(0);
        return kTopDb - (y - c.getY()) / c.getHeight() * (kTopDb - kBottomDb);
    }
    Hit hit(juce::Point<float> p) const {
        for (int b = 0; b < 3; ++b) {
            const auto c = column(b);
            if (p.x < c.getX() || p.x > c.getRight()) continue;
            const float dd = std::abs(p.y - yForDb(thr_[b][0])), du = std::abs(p.y - yForDb(thr_[b][1]));
            if (std::min(dd, du) < 7.0f) return {b, dd <= du};
        }
        return {};
    }
    juce::RangedAudioParameter* param(const Hit& h) const {
        return h.band < 0 ? nullptr : proc_.state.getParameter(bandId(h.band, h.down ? DownThr : UpThr));
    }

    SquashProcessor& proc_;
    float level_[3] = {-120, -120, -120}, gain_[3] = {0, 0, 0};
    float simLevel_[3] = {-999, -999, -999}, simGain_[3] = {0, 0, 0};
    float thr_[3][2] = {};
    float lowHz_ = 0, highHz_ = 0;
    Hit drag_, hover_;
};

// ------------------------------------------------------------------------------------------- editor
SquashEditor::SquashEditor(SquashProcessor& p)
    : PluginFrame(p, p.state, "PatSquash", "multiband squash", kAccent, 1000, 620),
      proc_(p),
      depth_(p.state, kDepth, "DEPTH", kAccent, settings(), NeonKnob::Size::Big),
      time_(p.state, kTime, "TIME", kAccent, settings()),
      upward_(p.state, kUpward, "UPWARD", kUp, settings()),
      downward_(p.state, kDownward, "DOWNWARD", kDown, settings()),
      input_(p.state, kInput, "INPUT", kAccent, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      lowCross_(p.state, kLowCross, "LOW SPLIT", kAccent, settings()),
      highCross_(p.state, kHighCross, "HIGH SPLIT", kAccent, settings()) {
    bands_ = std::make_unique<BandsView>(p, settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{bands_.get(), &depth_, &time_, &upward_, &downward_, &input_, &output_,
                                                                       &lowCross_, &highCross_, &hint_})
        content().addAndMakeVisible(c);
    const char* labels[3] = {"DOWN RATIO", "UP RATIO", "GAIN"};
    const int fields[3] = {DownRatio, UpRatio, Gain};
    for (int b = 0; b < 3; ++b)
        for (int k = 0; k < 3; ++k) {
            bandKnobs_.push_back(std::make_unique<NeonKnob>(p.state, bandId(b, fields[k]), labels[k], kBand[b], settings(), NeonKnob::Size::Small));
            content().addAndMakeVisible(*bandKnobs_.back());
        }
    depth_.setTooltip("How much of the squashed sound you hear (0 = untouched)");
    time_.setTooltip("Speed of the compression: lower = faster and more aggressive");
    upward_.setTooltip("Lifts the quiet parts of every band (detail, air, tails)");
    downward_.setTooltip("Pushes down the loud parts of every band");
    hint_.setFont(font(12.0f));
    hint_.setColour(juce::Label::textColourId, colours::dim);
    hint_.setJustificationType(juce::Justification::centred);
    hint_.setText("Drag the coral (DOWN) and blue (UP) lines in each band. DEPTH blends the effect in.", juce::dontSendNotification);

    clock().add([this](float dt) {
        if (!simulating_) bands_->update(dt);
    });
    setPresets(*p.presets);
    finishConstruction();
}

SquashEditor::~SquashEditor() = default;

void SquashEditor::viewChanged(bool pro) {
    for (auto& k : bandKnobs_) k->setVisible(pro);
    lowCross_.setVisible(pro);
    highCross_.setVisible(pro);
    hint_.setVisible(!pro);
}

void SquashEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    const bool pro = proView();
    auto top = r.removeFromTop(pro ? 290 : 330);
    bands_->setBounds(top);
    r.removeFromTop(8);
    if (pro) {
        // Band knobs under each column (the columns start after the dB scale).
        auto row = r.removeFromTop(92);
        const auto cols = row.withTrimmedLeft(40).withTrimmedRight(10);
        const int cw = cols.getWidth() / 3;
        for (int b = 0; b < 3; ++b) {
            auto c = juce::Rectangle<int>(cols.getX() + cw * b, row.getY(), cw, row.getHeight()).reduced(10, 0);
            const int kw = c.getWidth() / 3;
            for (int k = 0; k < 3; ++k) bandKnobs_[static_cast<std::size_t>(b * 3 + k)]->setBounds(c.removeFromLeft(kw));
        }
        r.removeFromTop(6);
        auto g = r.removeFromTop(120);
        const int kw = g.getWidth() / 9;
        depth_.setBounds(g.removeFromLeft(kw * 2));
        for (auto* k : {&time_, &upward_, &downward_, &input_, &output_, &lowCross_}) k->setBounds(g.removeFromLeft(kw).reduced(2, 8));
        highCross_.setBounds(g.reduced(2, 8));
    } else {
        hint_.setBounds(r.removeFromBottom(22));
        auto mid = r.withSizeKeepingCentre(220, r.getHeight());
        depth_.setBounds(mid);
        auto left = r.removeFromLeft((r.getWidth() - 220) / 2).reduced(10, 20);
        auto right = r.removeFromRight(left.getWidth() + 20).reduced(10, 20);
        const int lw = left.getWidth() / 3;
        for (auto* k : {&time_, &upward_, &downward_}) k->setBounds(left.removeFromLeft(lw));
        const int rw = right.getWidth() / 2;
        input_.setBounds(right.removeFromLeft(rw));
        output_.setBounds(right);
    }
}

void SquashEditor::simulateForSnapshot(float) {
    simulating_ = true;
    bands_->simulate(0, -14.0f, -6.5f);
    bands_->simulate(1, -26.0f, -1.8f);
    bands_->simulate(2, -52.0f, 7.2f);
    bands_->update(1.0f);
}
