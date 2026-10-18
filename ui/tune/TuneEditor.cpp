#include "ui/tune/TuneEditor.h"

#include "dsp/tune/Scales.h"

using namespace pat::ui;
using namespace tune;

namespace {
const juce::Colour kAccent = colours::violet;
constexpr float kLowNote = 36.0f, kHighNote = 84.0f;  // C2 .. C6
constexpr int kHistory = 600;                          // about 3 s of analysis steps

float noteOf(float hz) { return 69.0f + 12.0f * std::log2(hz / 440.0f); }
bool isBlack(int note) {
    const int n = ((note % 12) + 12) % 12;
    return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
}
juce::String noteName(float note) {
    const int n = juce::roundToInt(note);
    return juce::String(pat::dsp::keyName(n)) + juce::String(n / 12 - 1);
}
}  // namespace

// ------------------------------------------------------------------------------------------- piano roll
class TuneEditor::PianoRoll final : public LayeredCanvas {
public:
    PianoRoll(PerfSettings& s, TuneProcessor& p) : LayeredCanvas(s), proc_(p), history_(kHistory) {}

    void tick() {
        // Rebuild the scale lanes (static layer) only when the tuning changes.
        const int scale = juce::roundToInt(proc_.state.getRawParameterValue(kScale)->load());
        const int key = juce::roundToInt(proc_.state.getRawParameterValue(kKey)->load());
        const float a4 = proc_.state.getRawParameterValue(kA4)->load();
        const auto mask = proc_.customMask();
        if (scale != scale_ || key != key_ || a4 != a4_ || mask != mask_) {
            scale_ = scale;
            key_ = key;
            a4_ = a4;
            mask_ = mask;
            table_ = pat::dsp::buildTable(scale, key, mask, a4);
            invalidateStatic();
            repaint();
        }
        pat::dsp::TunePoint buf[512];
        std::size_t n;
        bool fresh = false;
        proc_.history.skipToNewest(kHistory);
        while ((n = proc_.history.pop(buf, 512)) > 0) {
            for (std::size_t i = 0; i < n; ++i) push(buf[i]);
            fresh = true;
        }
        if (fresh || !settings().profile().repaintOnlyOnChange) repaint();
    }
    void push(const pat::dsp::TunePoint& p) {
        history_[static_cast<std::size_t>(head_)] = p;
        head_ = (head_ + 1) % kHistory;
    }

protected:
    float yFor(float note) const {
        const auto r = getLocalBounds().toFloat();
        return r.getBottom() - (note - kLowNote) / (kHighNote - kLowNote) * r.getHeight();
    }
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        const float rowH = r.getHeight() / (kHighNote - kLowNote);
        const float keysW = 46.0f;
        // Keyboard strip and faint semitone rows.
        for (int n = static_cast<int>(kLowNote); n < static_cast<int>(kHighNote); ++n) {
            const float y = yFor(static_cast<float>(n) + 0.5f);
            g.setColour(isBlack(n) ? juce::Colour(0xff0b0d14) : juce::Colour(0xff0e111a));
            g.fillRect(keysW, y, r.getWidth() - keysW, rowH);
            g.setColour(isBlack(n) ? juce::Colour(0xff15171f) : juce::Colour(0xffcfd4de));
            g.fillRect(2.0f, y + 0.5f, isBlack(n) ? keysW * 0.62f : keysW - 4.0f, rowH - 1.0f);
            if (n % 12 == 0) {
                g.setColour(colours::background);
                g.setFont(font(9.5f, true));
                g.drawText("C" + juce::String(n / 12 - 1), juce::Rectangle<float>(4.0f, y, keysW - 8.0f, rowH), juce::Justification::centredRight);
            }
        }
        // Scale lanes: every allowed pitch, at its exact cents (quarter tones sit between rows).
        if (table_.size() > 0) {
            const float refNote = noteOf(static_cast<float>(table_.referenceHz()));
            for (float oct = -4; oct <= 4; ++oct)
                for (int i = 0; i < table_.size(); ++i) {
                    const float note = refNote + static_cast<float>(table_.degree(i) / 100.0) + oct * static_cast<float>(table_.periodCents() / 100.0);
                    if (note < kLowNote || note > kHighNote) continue;
                    const float y = yFor(note);
                    const bool root = i == 0;
                    g.setColour(kAccent.withAlpha(root ? 0.55f : 0.28f));
                    g.fillRect(keysW, y - (root ? 1.0f : 0.5f), r.getWidth() - keysW, root ? 2.0f : 1.0f);
                }
        }
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }
    void paintDynamic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        const float keysW = 46.0f;
        const float dx = (r.getWidth() - keysW - 8.0f) / static_cast<float>(kHistory - 1);
        juce::Path sung, corrected;
        bool sungOpen = false, corrOpen = false;
        for (int i = 0; i < kHistory; ++i) {
            const auto& p = history_[static_cast<std::size_t>((head_ + i) % kHistory)];
            const float x = keysW + dx * static_cast<float>(i);
            if (p.detectedHz > 0) {
                const float y = yFor(noteOf(p.detectedHz));
                if (!sungOpen) sung.startNewSubPath(x, y);
                else sung.lineTo(x, y);
                sungOpen = true;
            } else {
                sungOpen = false;
            }
            if (p.outputHz > 0) {
                const float y = yFor(noteOf(p.outputHz));
                if (!corrOpen) corrected.startNewSubPath(x, y);
                else corrected.lineTo(x, y);
                corrOpen = true;
            } else {
                corrOpen = false;
            }
        }
        g.setColour(colours::dim.withAlpha(0.6f));
        g.strokePath(sung, juce::PathStrokeType(1.2f));
        strokeNeon(g, corrected, kAccent, 2.4f, glow());
        // Current note readout.
        const auto& now = history_[static_cast<std::size_t>((head_ + kHistory - 1) % kHistory)];
        g.setFont(font(26.0f, true));
        if (now.detectedHz > 0) {
            const float note = noteOf(now.targetHz > 0 ? now.targetHz : now.detectedHz);
            const float dev = 100.0f * (noteOf(now.detectedHz) - note);
            g.setColour(colours::text);
            g.drawText(noteName(note), r.reduced(14.0f, 8.0f), juce::Justification::topRight);
            g.setFont(font(12.0f, true));
            g.setColour(std::abs(dev) < 10.0f ? colours::lime : colours::gold);
            g.drawText((dev > 0 ? "+" : "") + juce::String(juce::roundToInt(dev)) + " ct", r.reduced(14.0f, 40.0f).withHeight(16.0f).translated(0, 0),
                       juce::Justification::topRight);
        } else {
            g.setColour(colours::faint);
            g.drawText("--", r.reduced(14.0f, 8.0f), juce::Justification::topRight);
        }
    }

private:
    TuneProcessor& proc_;
    std::vector<pat::dsp::TunePoint> history_;
    int head_ = 0;
    pat::TuningTable table_;
    int scale_ = -1, key_ = -1;
    float a4_ = 0;
    std::uint32_t mask_ = 0xffffffffu;
};

// ------------------------------------------------------------------------------------------- confidence
class TuneEditor::ConfidenceMeter final : public juce::Component, public juce::SettableTooltipClient {
public:
    ConfidenceMeter(PerfSettings& s, TuneProcessor& p) : settings_(s), proc_(p) {
        setTooltip("Pitch detection confidence. Below the line, PatTune leaves the voice alone (breaths, consonants, noise).");
    }
    void update(float clarity, float dt) {
        const float before = shown_;
        shown_ = (settings_.neonFixed() || clarity > shown_) ? clarity : shown_ - dt * 1.5f;
        shown_ = juce::jlimit(0.0f, 1.0f, shown_);
        if (std::abs(before - shown_) > 0.01f) repaint();
    }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        const auto labelArea = r.removeFromBottom(16.0f);
        const auto bar = r.withSizeKeepingCentre(14.0f, r.getHeight());
        g.setColour(colours::raised);
        g.fillRoundedRectangle(bar, 4.0f);
        const float gate = proc_.state.getRawParameterValue(kGate)->load();
        const auto fill = bar.withTop(bar.getBottom() - bar.getHeight() * shown_);
        g.setColour(shown_ >= gate ? colours::lime : colours::gold.withAlpha(0.7f));
        g.fillRoundedRectangle(fill, 4.0f);
        const float yGate = bar.getBottom() - bar.getHeight() * gate;
        g.setColour(colours::text);
        g.fillRect(bar.getX() - 5.0f, yGate - 1.0f, bar.getWidth() + 10.0f, 2.0f);
        g.setColour(colours::dim);
        g.setFont(font(10.0f, true));
        g.drawText("CONF.", labelArea, juce::Justification::centred);
    }

private:
    PerfSettings& settings_;
    TuneProcessor& proc_;
    float shown_ = 0;
};

// ------------------------------------------------------------------------------------------- scale editor
// 24 quarter-tone cells above the key: click to switch notes on/off. Editing a preset scale turns
// it into a custom copy first.
class TuneEditor::ScaleEditor final : public juce::Component, public juce::SettableTooltipClient {
public:
    ScaleEditor(TuneEditor& ed) : ed_(ed) { setTooltip("Draw your own scale: click the quarter-tone steps to switch them on or off."); }
    void tick() {
        const auto t = currentTable();
        std::uint32_t mask = 0;
        for (int i = 0; i < t.size(); ++i) {
            const int step = juce::roundToInt(t.degree(i) / 50.0);
            if (std::abs(t.degree(i) - step * 50.0) < 1.0 && step < 24) mask |= 1u << step;
        }
        if (mask != shown_) {
            shown_ = mask;
            repaint();
        }
    }
    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        const float w = r.getWidth() / 24.0f;
        const int key = juce::roundToInt(ed_.param(kKey));
        for (int i = 0; i < 24; ++i) {
            const auto cell = juce::Rectangle<float>(r.getX() + w * i, r.getY(), w, r.getHeight()).reduced(1.5f);
            const bool on = (shown_ >> i) & 1u;
            const bool quarter = i % 2 == 1;
            g.setColour(on ? kAccent.withAlpha(quarter ? 0.5f : 0.8f) : (quarter ? juce::Colour(0xff0b0d14) : colours::raised));
            g.fillRoundedRectangle(cell, 3.0f);
            g.setColour(on ? kAccent : colours::line);
            g.drawRoundedRectangle(cell, 3.0f, 1.0f);
            g.setColour(on ? colours::text : colours::dim);
            g.setFont(font(quarter ? 8.5f : 10.0f, true));
            const juce::String name = juce::String(pat::dsp::keyName(key + i / 2)) + (quarter ? "+" : "");
            g.drawText(name, cell, juce::Justification::centred, false);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        const int step = juce::jlimit(0, 23, static_cast<int>(e.position.x / (static_cast<float>(getWidth()) / 24.0f)));
        std::uint32_t mask = shown_;
        mask ^= 1u << step;
        ed_.proc_.setCustomMask(mask);
        ed_.setParam(kScale, static_cast<float>(pat::dsp::customScaleIndex()));
        shown_ = mask;
        repaint();
    }

private:
    pat::TuningTable currentTable() const {
        return pat::dsp::buildTable(juce::roundToInt(ed_.param(kScale)), 0, ed_.proc_.customMask(), 440.0);
    }
    TuneEditor& ed_;
    std::uint32_t shown_ = 0;
};

// ------------------------------------------------------------------------------------------- editor

TuneEditor::TuneEditor(TuneProcessor& p)
    : PluginFrame(p, p.state, "PatTune", "pitch correction", kAccent, 980, 640),
      proc_(p),
      speedBig_(p.state, kSpeed, "SPEED", kAccent, settings(), NeonKnob::Size::Big),
      speed_(p.state, kSpeed, "RETUNE", kAccent, settings()),
      humanize_(p.state, kHumanize, "HUMANIZE", kAccent, settings()),
      formant_(p.state, kFormant, "FORMANT", kAccent, settings()),
      mix_(p.state, kMix, "MIX", kAccent, settings()),
      gate_(p.state, kGate, "GATE", kAccent, settings()),
      a4_(p.state, kA4, "A4", kAccent, settings()) {
    roll_ = std::make_unique<PianoRoll>(settings(), p);
    confidence_ = std::make_unique<ConfidenceMeter>(settings(), p);
    scaleEditor_ = std::make_unique<ScaleEditor>(*this);
    for (juce::Component* c : std::initializer_list<juce::Component*>{roll_.get(), confidence_.get(), scaleEditor_.get(), &key_, &scale_, &major_, &minor_,
                                                                       &auto_, &speedBig_, &speed_, &humanize_, &formant_, &mix_, &gate_, &a4_, &keyLabel_,
                                                                       &scaleLabel_, &hint_})
        content().addAndMakeVisible(c);

    for (int k = 0; k < 12; ++k) key_.addItem(pat::dsp::keyName(k), k + 1);
    juce::String group;
    for (int s = 0; s < pat::dsp::numScales(); ++s) {
        if (group != pat::dsp::scale(s).group) {
            group = pat::dsp::scale(s).group;
            scale_.addSectionHeading(group);
        }
        scale_.addItem(pat::dsp::scale(s).name, s + 1);
    }
    keyAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, kKey, key_);
    scaleAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state, kScale, scale_);
    key_.setTooltip("Key of the song");
    scale_.setTooltip("Scale: Western, Arabic maqamat (quarter tones), Japanese, or your own drawing");

    for (auto* b : {&major_, &minor_, &auto_}) {
        b->setColour(juce::TextButton::buttonOnColourId, kAccent);
        b->setClickingTogglesState(false);
    }
    major_.onClick = [this] { setParam(kScale, 1.0f); };
    minor_.onClick = [this] { setParam(kScale, 2.0f); };
    if (auto* prm = p.state.getParameter(kAuto)) {
        autoAttachment_ = std::make_unique<juce::ParameterAttachment>(*prm, [this](float v) { auto_.setToggleState(v > 0.5f, juce::dontSendNotification); });
        autoAttachment_->sendInitialUpdate();
    }
    auto_.onClick = [this] { autoAttachment_->setValueAsCompleteGesture(auto_.getToggleState() ? 0.0f : 1.0f); };
    auto_.setTooltip("Auto: correct the pitch to the scale (off = the voice passes through)");

    for (auto* l : {&keyLabel_, &scaleLabel_, &hint_}) {
        l->setFont(font(11.5f, true));
        l->setColour(juce::Label::textColourId, colours::dim);
    }
    keyLabel_.setText("KEY", juce::dontSendNotification);
    scaleLabel_.setText("SCALE", juce::dontSendNotification);
    hint_.setFont(font(12.0f));
    hint_.setJustificationType(juce::Justification::centred);
    hint_.setText("Pick the key of your song, turn AUTO on, and set how fast notes snap (0 = robotic).", juce::dontSendNotification);

    clock().add([this](float dt) {
        if (simulating_) return;
        roll_->tick();
        confidence_->update(proc_.clarity.load(), dt);
        if (scaleEditor_->isVisible()) scaleEditor_->tick();
        const int sc = juce::roundToInt(param(kScale));
        major_.setToggleState(sc == 1, juce::dontSendNotification);
        minor_.setToggleState(sc == 2, juce::dontSendNotification);
    });
    finishConstruction();
}

TuneEditor::~TuneEditor() = default;

float TuneEditor::param(const char* id) const { return proc_.state.getRawParameterValue(id)->load(); }

void TuneEditor::setParam(const char* id, float value) {
    if (auto* p = proc_.state.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
}

void TuneEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&scale_, &scaleLabel_, scaleEditor_.get(), &speed_, &humanize_, &formant_, &mix_,
                                                                       &gate_, &a4_, confidence_.get()})
        c->setVisible(pro);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&major_, &minor_, &speedBig_, &hint_}) c->setVisible(!pro);
}

void TuneEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    if (!proView()) {
        roll_->setBounds(r.removeFromTop(340));
        r.removeFromTop(12);
        hint_.setBounds(r.removeFromBottom(20));
        auto left = r.removeFromLeft(r.getWidth() / 3).reduced(20, 0);
        keyLabel_.setBounds(left.removeFromTop(18));
        key_.setBounds(left.removeFromTop(34));
        left.removeFromTop(10);
        auto mm = left.removeFromTop(34);
        major_.setBounds(mm.removeFromLeft(mm.getWidth() / 2).reduced(2, 0));
        minor_.setBounds(mm.reduced(2, 0));
        auto right = r.removeFromRight(r.getWidth() / 2).reduced(30, 30);
        auto_.setBounds(right.withSizeKeepingCentre(160, 56));
        speedBig_.setBounds(r);
        return;
    }
    auto top = r.removeFromTop(300);
    confidence_->setBounds(top.removeFromRight(50));
    top.removeFromRight(10);
    roll_->setBounds(top);
    r.removeFromTop(10);
    auto bar = r.removeFromTop(34);
    keyLabel_.setBounds(bar.removeFromLeft(36));
    key_.setBounds(bar.removeFromLeft(80).reduced(0, 2));
    bar.removeFromLeft(16);
    scaleLabel_.setBounds(bar.removeFromLeft(48));
    scale_.setBounds(bar.removeFromLeft(220).reduced(0, 2));
    bar.removeFromLeft(16);
    auto_.setBounds(bar.removeFromLeft(110).reduced(0, 1));
    r.removeFromTop(10);
    scaleEditor_->setBounds(r.removeFromTop(40));
    r.removeFromTop(8);
    const int kw = r.getWidth() / 6;
    for (auto* k : {&speed_, &humanize_, &formant_, &mix_, &gate_, &a4_}) k->setBounds(r.removeFromLeft(kw).reduced(4, 0));
}

void TuneEditor::simulateForSnapshot(float seconds) {
    simulating_ = true;
    roll_->tick();
    // A sung phrase drifting around A3 and C4 with vibrato; the correction pulls it onto the scale.
    for (int i = 0; i < kHistory; ++i) {
        const float t = static_cast<float>(i) / 200.0f;
        const float target = i < 200 ? 220.0f : (i < 400 ? 261.63f : 246.94f);
        const float sung = target * std::exp2((18.0f * std::sin(t * 31.0f) + (i % 200 < 30 ? 60.0f - 2.0f * (i % 200) : 0.0f)) / 1200.0f);
        const float out = target * std::exp2((4.0f * std::sin(t * 31.0f)) / 1200.0f);
        roll_->push({i % 200 > 190 ? 0.0f : sung, target, i % 200 > 190 ? 0.0f : out, 0.9f});
    }
    roll_->repaint();
    for (float t = 0; t < seconds; t += 0.1f) confidence_->update(0.86f, 0.1f);
    scaleEditor_->tick();
    simulating_ = false;
}
