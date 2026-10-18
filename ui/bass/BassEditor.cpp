#include "ui/bass/BassEditor.h"

#include <cmath>

using namespace pat::ui;
using pat::dsp::BassEngine;

namespace {
const juce::Colour kAccent = colours::violet;
const juce::Colour kOsc = colours::cyan;
const juce::Colour kSub = colours::pink;
const juce::Colour kFilter = colours::gold;
const juce::Colour kLfo = colours::lime;
const juce::Colour kFx = colours::orange;
constexpr int kScopeLen = 1200;
}  // namespace

// ------------------------------------------------------------------------------------------- scope
class BassEditor::Scope final : public LayeredCanvas {
public:
    Scope(BassProcessor& p, PerfSettings& s) : LayeredCanvas(s), proc_(p), ring_(4096, 0.0f), view_(kScopeLen, 0.0f) {}
    void pull() {
        std::array<float, 2048> tmp{};
        proc_.scope.skipToNewest(4096);
        const auto n = proc_.scope.pop(tmp.data(), tmp.size());
        for (std::size_t i = 0; i < n; ++i) {
            ring_[static_cast<std::size_t>(head_)] = tmp[i];
            head_ = (head_ + 1) % static_cast<int>(ring_.size());
        }
        if (n == 0 && !simulated_) return;
        capture();
        repaint();
    }
    void simulate() {
        simulated_ = true;
        for (int i = 0; i < static_cast<int>(ring_.size()); ++i) {
            const double t = i / 48000.0;
            ring_[static_cast<std::size_t>(i)] = static_cast<float>(0.55 * std::tanh(2.5 * std::sin(2.0 * 3.14159 * 55.0 * t)) +
                                                                  0.2 * std::sin(2.0 * 3.14159 * 110.0 * t + 0.6 * std::sin(2.0 * 3.14159 * 3.0 * t)));
        }
        head_ = 0;
        capture();
        repaint();
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(colours::grid);
        for (int k = 1; k < 8; ++k) g.drawVerticalLine(juce::roundToInt(r.getWidth() * k / 8.0f), r.getY() + 4.0f, r.getBottom() - 4.0f);
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine(juce::roundToInt(r.getHeight() * k / 4.0f), r.getX() + 4.0f, r.getRight() - 4.0f);
        g.setColour(colours::faint);
        g.setFont(font(10.0f, true));
        g.drawText("OUTPUT", r.reduced(10.0f, 6.0f), juce::Justification::topLeft);
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }
    void paintDynamic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat().reduced(8.0f, 10.0f);
        juce::Path p;
        float pk = 0.05f;
        for (float v : view_) pk = std::max(pk, std::abs(v));
        const float scale = 0.45f * r.getHeight() / std::max(0.35f, pk);  // gentle auto zoom
        for (int i = 0; i < kScopeLen; ++i) {
            const float x = r.getX() + r.getWidth() * static_cast<float>(i) / (kScopeLen - 1);
            const float y = r.getCentreY() - view_[static_cast<std::size_t>(i)] * scale;
            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        strokeNeon(g, p, kAccent.interpolatedWith(kOsc, 0.35f), 2.0f, glow());
    }

private:
    // Start the view at a rising zero crossing so the wave stands still.
    void capture() {
        const int size = static_cast<int>(ring_.size());
        int start = (head_ - kScopeLen - 1200 + size) % size;
        for (int k = 0; k < 1200; ++k) {
            const int a = (start + k) % size, b = (start + k + 1) % size;
            if (ring_[static_cast<std::size_t>(a)] < 0.0f && ring_[static_cast<std::size_t>(b)] >= 0.0f) {
                start = b;
                break;
            }
        }
        for (int i = 0; i < kScopeLen; ++i) view_[static_cast<std::size_t>(i)] = ring_[static_cast<std::size_t>((start + i) % size)];
    }
    BassProcessor& proc_;
    std::vector<float> ring_, view_;
    int head_ = 0;
    bool simulated_ = false;
};

// ------------------------------------------------------------------------------------------- shapes
class BassEditor::ShapeView final : public juce::Component {
public:
    ShapeView(BassProcessor& p, PerfSettings& s) : proc_(p), settings_(s) { setInterceptsMouseClicks(false, false); }
    void update() {
        const float vals[7] = {proc_.value("a_shape"), proc_.value("a_pw"), proc_.value("b_on"), proc_.value("b_shape"), proc_.value("sub_on"),
                               proc_.lfoValue.load(), proc_.cutoffNow.load()};
        bool changed = false;
        for (int i = 0; i < 7; ++i) changed |= std::abs(vals[i] - last_[i]) > (i == 6 ? 5.0f : 0.002f);
        if (!changed) return;
        std::copy(vals, vals + 7, last_);
        repaint();
    }
    void paint(juce::Graphics& g) override {
        drawPanel(g, getLocalBounds(), "WAVES", kAccent);
        auto r = getLocalBounds().toFloat().reduced(12.0f).withTrimmedTop(22.0f);
        auto waves = r.removeFromTop(r.getHeight() * 0.62f);
        const auto draw = [&](float shape, float pw, juce::Colour c, float yOff, float amp) {
            juce::Path p;
            for (int i = 0; i <= 200; ++i) {
                const double ph = 2.0 * i / 200.0;  // two cycles
                const float x = waves.getX() + waves.getWidth() * static_cast<float>(i) / 200.0f;
                const float y = waves.getCentreY() + yOff - BassEngine::shapeValue(shape, pw, ph) * waves.getHeight() * amp;
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            strokeNeon(g, p, c, 1.8f, !settings_.neonFixed() && settings_.profile().glow);
        };
        if (last_[4] > 0.5f) {  // sub: one cycle across (an octave down)
            juce::Path p;
            for (int i = 0; i <= 200; ++i) {
                const float x = waves.getX() + waves.getWidth() * static_cast<float>(i) / 200.0f;
                const float y = waves.getCentreY() - static_cast<float>(std::sin(2.0 * 3.14159265 * i / 200.0)) * waves.getHeight() * 0.42f;
                if (i == 0) p.startNewSubPath(x, y);
                else p.lineTo(x, y);
            }
            g.setColour(kSub.withAlpha(0.6f));
            g.strokePath(p, juce::PathStrokeType(1.4f));
        }
        if (last_[2] > 0.5f) draw(last_[3], last_[1], kOsc.withAlpha(0.55f), 0.0f, 0.3f);
        draw(last_[0], last_[1], kOsc, 0.0f, 0.36f);
        g.setFont(font(10.0f, true));
        g.setColour(kOsc);
        g.drawText("A", waves.withHeight(12.0f), juce::Justification::topLeft);
        if (last_[2] > 0.5f) g.drawText("  +B", waves.withHeight(12.0f), juce::Justification::topLeft);
        if (last_[4] > 0.5f) {
            g.setColour(kSub);
            g.drawText("SUB", waves.withHeight(12.0f), juce::Justification::topRight);
        }
        // LFO and cutoff.
        r.removeFromTop(6.0f);
        auto lfoRow = r.removeFromTop(16.0f);
        g.setColour(colours::raised);
        g.fillRoundedRectangle(lfoRow, 4.0f);
        const float lfo = juce::jlimit(-1.0f, 1.0f, last_[5]);
        const float cx = lfoRow.getCentreX(), w = lfoRow.getWidth() * 0.5f * lfo;
        g.setColour(kLfo);
        g.fillRoundedRectangle(juce::Rectangle<float>(std::min(cx, cx + w), lfoRow.getY(), std::abs(w), lfoRow.getHeight()), 4.0f);
        g.setColour(colours::text);
        g.drawText("LFO", lfoRow.reduced(6.0f, 0.0f), juce::Justification::centredLeft);
        g.setColour(kFilter);
        g.drawText("CUTOFF " + (last_[6] >= 1000.0f ? juce::String(last_[6] / 1000.0f, 1) + "k" : juce::String(juce::roundToInt(last_[6]))),
                   r.removeFromTop(18.0f), juce::Justification::centredLeft);
    }

private:
    BassProcessor& proc_;
    PerfSettings& settings_;
    float last_[7] = {-1, -1, -1, -1, -1, -9, -1};
};

// ------------------------------------------------------------------------------------------- panels
// Draws the section frames behind the controls.
class BassEditor::Panels final : public juce::Component {
public:
    explicit Panels(std::vector<Section>& s) : sections_(s) { setInterceptsMouseClicks(false, false); }
    void paint(juce::Graphics& g) override {
        for (const auto& s : sections_)
            if (!s.bounds.isEmpty()) drawPanel(g, s.bounds, s.title, s.colour);
    }

private:
    std::vector<Section>& sections_;
};

// ------------------------------------------------------------------------------------------- editor
NeonKnob* BassEditor::knob(const char* id, const char* label, juce::Colour c, bool simple) {
    auto k = std::make_unique<NeonKnob>(proc_.state, id, label, c, settings(), simple ? NeonKnob::Size::Big : NeonKnob::Size::Small);
    auto* raw = k.get();
    content().addAndMakeVisible(*raw);
    (simple ? simpleKnobs_ : knobs_).push_back(std::move(k));
    return raw;
}

juce::Component* BassEditor::combo(const char* id, const char* label) {
    struct Labeled final : juce::Component {
        juce::Label label;
        juce::ComboBox box;
        void resized() override {
            auto r = getLocalBounds().reduced(2, 0);
            label.setBounds(r.removeFromTop(16));
            box.setBounds(r.removeFromTop(26));
        }
    };
    auto c = std::make_unique<Labeled>();
    c->label.setText(label, juce::dontSendNotification);
    c->label.setFont(font(10.0f, true));
    c->label.setColour(juce::Label::textColourId, colours::dim);
    c->label.setJustificationType(juce::Justification::centred);
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(proc_.state.getParameter(id))) c->box.addItemList(p->choices, 1);
    c->addAndMakeVisible(c->label);
    c->addAndMakeVisible(c->box);
    comboAtt_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.state, id, c->box));
    auto* raw = c.get();
    content().addAndMakeVisible(*raw);
    others_.push_back(std::move(c));
    return raw;
}

juce::Component* BassEditor::toggle(const char* id, const char* label, juce::Colour c) {
    auto t = std::make_unique<juce::ToggleButton>(label);
    t->setColour(juce::ToggleButton::tickColourId, c);
    buttonAtt_.push_back(std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.state, id, *t));
    auto* raw = t.get();
    content().addAndMakeVisible(*raw);
    others_.push_back(std::move(t));
    return raw;
}

BassEditor::BassEditor(BassProcessor& p)
    : PluginFrame(p, p.state, "PatBass", "bass synth", kAccent, 1200, 780),
      proc_(p),
      keyboard_(p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard) {
    scope_ = std::make_unique<Scope>(p, settings());
    shape_ = std::make_unique<ShapeView>(p, settings());
    panels_ = std::make_unique<Panels>(sections_);
    content().addAndMakeVisible(*panels_);
    content().addAndMakeVisible(*scope_);
    content().addAndMakeVisible(*shape_);
    content().addAndMakeVisible(keyboard_);
    keyboard_.setAvailableRange(24, 84);
    keyboard_.setLowestVisibleKey(24);
    keyboard_.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, kAccent.withAlpha(0.7f));
    keyboard_.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, kAccent.withAlpha(0.3f));

    // Simple view: the twelve knobs that matter most.
    knob("a_shape", "SHAPE", kOsc, true);
    knob("sub_level", "SUB", kSub, true);
    knob("punch", "PUNCH", kSub, true);
    knob("glide", "GLIDE", kAccent, true);
    knob("drive", "DRIVE", kFx, true);
    knob("glue", "GLUE", kFx, true);
    knob("cutoff", "CUTOFF", kFilter, true);
    knob("reso", "RESONANCE", kFilter, true);
    knob("f_env", "FILTER ENV", kFilter, true);
    knob("lfo_cut", "WOBBLE", kLfo, true);
    knob("lfo_div", "WOBBLE RATE", kLfo, true);
    knob("volume", "VOLUME", kAccent, true);

    // Pro view: every section.
    sections_.push_back({"OSC A", kOsc,
                         {knob("a_shape", "SHAPE", kOsc), knob("a_pw", "PW", kOsc), knob("a_fm", "FM GROWL", kOsc), combo("a_fmratio", "FM RATIO"),
                          knob("unison", "UNISON", kOsc), knob("detune", "DETUNE", kOsc), knob("spread", "SPREAD", kOsc), knob("a_level", "LEVEL", kOsc)},
                         4});
    sections_.push_back({"OSC B", kOsc,
                         {toggle("b_on", "ON", kOsc), knob("b_shape", "SHAPE", kOsc), knob("b_oct", "OCTAVE", kOsc), knob("b_semi", "SEMI", kOsc),
                          knob("b_fine", "FINE", kOsc), knob("b_level", "LEVEL", kOsc)},
                         3});
    sections_.push_back({"SUB / PUNCH", kSub,
                         {toggle("sub_on", "SUB", kSub), combo("sub_oct", "OCTAVE"), combo("sub_shape", "SHAPE"), knob("sub_level", "SUB LEVEL", kSub),
                          knob("noise", "NOISE", kSub), knob("punch", "PUNCH", kSub), knob("punch_time", "PUNCH TIME", kSub), knob("click", "CLICK", kSub)},
                         4});
    sections_.push_back({"FILTER", kFilter,
                         {combo("f_type", "TYPE"), knob("cutoff", "CUTOFF", kFilter), knob("reso", "RESO", kFilter), knob("f_drive", "DRIVE", kFilter),
                          knob("f_env", "ENV AMT", kFilter), knob("keytrack", "KEY TRACK", kFilter), knob("vowel", "VOWEL", kFilter)},
                         4});
    sections_.push_back({"ENVELOPES  (filter / amp)", kFilter,
                         {knob("fe_a", "F ATTACK", kFilter), knob("fe_d", "F DECAY", kFilter), knob("fe_s", "F SUSTAIN", kFilter), knob("fe_r", "F RELEASE", kFilter),
                          knob("ae_a", "ATTACK", kAccent), knob("ae_d", "DECAY", kAccent), knob("ae_s", "SUSTAIN", kAccent), knob("ae_r", "RELEASE", kAccent)},
                         4});
    sections_.push_back({"VOICE", kAccent,
                         {toggle("mono", "MONO", kAccent), toggle("legato", "LEGATO", kAccent), knob("glide", "GLIDE", kAccent), knob("bend", "BEND", kAccent),
                          knob("velocity", "VELOCITY", kAccent)},
                         2, {}, 3});
    sections_.push_back({"LFO (WOBBLE)", kLfo,
                         {combo("lfo_shape", "SHAPE"), toggle("lfo_sync", "SYNC", kLfo), combo("lfo_div", "RATE"), knob("lfo_rate", "HZ", kLfo),
                          toggle("lfo_retrig", "RETRIG", kLfo), knob("lfo_cut", "CUTOFF", kLfo), knob("lfo_pitch", "PITCH", kLfo),
                          knob("lfo_shapeamt", "SHAPE", kLfo), knob("lfo_vol", "VOLUME", kLfo), knob("lfo_vowel", "VOWEL", kLfo)},
                         5});
    sections_.push_back({"FX", kFx,
                         {combo("dist_type", "DISTORTION"), knob("drive", "DRIVE", kFx), knob("dist_mix", "MIX", kFx), toggle("clean_lows", "CLEAN", kFx),
                          knob("split", "BELOW", kFx), knob("glue", "GLUE", kFx), knob("width", "WIDTH", kFx), knob("volume", "VOLUME", kFx)},
                         4});

    clock().add([this](float) {
        if (simulating_) return;
        scope_->pull();
        shape_->update();
    });
    setPresets(*p.presets);
    finishConstruction();
}

BassEditor::~BassEditor() = default;

void BassEditor::viewChanged(bool pro) {
    for (auto& k : simpleKnobs_) k->setVisible(!pro);
    for (auto& s : sections_)
        for (auto* c : s.items) c->setVisible(pro);
    panels_->setVisible(pro);
}

void BassEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(14, 10);
    keyboard_.setBounds(r.removeFromBottom(54));
    keyboard_.setKeyWidth(static_cast<float>(r.getWidth()) / 36.0f);  // 36 white keys (C1..C6)
    r.removeFromBottom(8);
    const bool pro = proView();
    auto top = r.removeFromTop(pro ? 150 : 250);
    shape_->setBounds(top.removeFromRight(300));
    top.removeFromRight(10);
    scope_->setBounds(top);
    r.removeFromTop(10);
    panels_->setBounds(content().getLocalBounds());
    for (auto& s : sections_) s.bounds = {};
    if (!pro) {
        const int cols = 6, rows = 2;
        const int kw = r.getWidth() / cols, kh = r.getHeight() / rows;
        for (int i = 0; i < static_cast<int>(simpleKnobs_.size()); ++i)
            simpleKnobs_[static_cast<std::size_t>(i)]->setBounds(r.getX() + (i % cols) * kw, r.getY() + (i / cols) * kh, kw, kh);
        return;
    }
    // Two rows of four sections; widths follow how many columns each needs.
    const int rowH = (r.getHeight() - 10) / 2;
    const int order[2][4] = {{0, 1, 2, 3}, {4, 5, 6, 7}};
    for (int row = 0; row < 2; ++row) {
        auto line = r.removeFromTop(rowH);
        if (row == 0) r.removeFromTop(10);
        int totalCols = 0;
        const auto weight = [this](int k) {
            const auto& s = sections_[static_cast<std::size_t>(k)];
            return s.weight > 0 ? s.weight : s.columns;
        };
        for (int k : order[row]) totalCols += weight(k);
        const float unit = static_cast<float>(line.getWidth() - 30) / static_cast<float>(totalCols);
        for (int k : order[row]) {
            auto& s = sections_[static_cast<std::size_t>(k)];
            const int w = juce::roundToInt(unit * static_cast<float>(weight(k)));
            s.bounds = line.removeFromLeft(w);
            line.removeFromLeft(10);
            auto inner = s.bounds.reduced(6, 6).withTrimmedTop(20);
            const int n = static_cast<int>(s.items.size());
            const int rowsHere = (n + s.columns - 1) / s.columns;
            const int cw = inner.getWidth() / s.columns, ch = inner.getHeight() / std::max(1, rowsHere);
            for (int i = 0; i < n; ++i) {
                auto cell = juce::Rectangle<int>(inner.getX() + (i % s.columns) * cw, inner.getY() + (i / s.columns) * ch, cw, ch);
                auto* c = s.items[static_cast<std::size_t>(i)];
                if (dynamic_cast<juce::ToggleButton*>(c) != nullptr) c->setBounds(cell.withSizeKeepingCentre(cw + 16, 26));  // text may use the padding
                else if (dynamic_cast<NeonKnob*>(c) != nullptr) c->setBounds(cell);
                else c->setBounds(cell.withSizeKeepingCentre(cw - 4, 46));
            }
        }
    }
    panels_->toBack();
    panels_->repaint();
}

void BassEditor::simulateForSnapshot(float) {
    simulating_ = true;
    static bool probed = false;
    if (!probed) {  // diagnostic: peak of every preset holding C2 for 1.5 s (to even out levels)
        probed = true;
        BassProcessor probe;
        juce::String txt;
        juce::AudioBuffer<float> buf(2, 512);
        for (int i = 0; i < static_cast<int>(probe.presets->entries().size()); ++i) {
            probe.presets->load(i);
            probe.prepareToPlay(48000.0, 512);
            float pk = 0;
            for (int b = 0; b < 141; ++b) {
                juce::MidiBuffer midi;
                if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 36, 0.9f), 0);
                if (b == 140) midi.addEvent(juce::MidiMessage::noteOff(1, 36), 0);
                buf.clear();
                probe.processBlock(buf, midi);
                pk = std::max(pk, buf.getMagnitude(0, 512));
            }
            txt << probe.presets->entries()[static_cast<std::size_t>(i)].name << "|" << juce::String(juce::Decibels::gainToDecibels(pk, -120.0f), 2) << "\n";
        }
        juce::File(juce::SystemStats::getEnvironmentVariable("PATSUITE_SNAPSHOT", {})).getChildFile("PatBass-levels.txt").replaceWithText(txt);
    }
    scope_->simulate();
    shape_->update();
}
