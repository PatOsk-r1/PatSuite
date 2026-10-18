#include "ui/delay/PatPatPatEditor.h"

#include <array>

using namespace pat::ui;
using namespace patpatpat;

namespace {
const juce::Colour kAccent = colours::orange;
const juce::Colour kLeft = colours::cyan;
const juce::Colour kRight = colours::pink;
constexpr int kMaxTaps = 24;
constexpr int kMaxShots = 4;
}  // namespace

// ------------------------------------------------------------------------------------------- stage
class PatPatPatEditor::Stage final : public LayeredCanvas {
public:
    explicit Stage(PerfSettings& s) : LayeredCanvas(s) { setInterceptsMouseClicks(false, false); }

    void setTaps(const pat::dsp::DelaySettings& s, double bpm) {
        std::array<pat::dsp::DelayTap, kMaxTaps> t{};
        const int n = pat::dsp::DelayEngine::taps(s, bpm, t.data(), kMaxTaps);
        bool same = n == numTaps_ && s.mode == mode_ && s.freeze == freeze_;
        for (int i = 0; same && i < n; ++i)
            same = std::abs(t[static_cast<std::size_t>(i)].seconds - taps_[static_cast<std::size_t>(i)].seconds) < 1e-6 &&
                   std::abs(t[static_cast<std::size_t>(i)].left - taps_[static_cast<std::size_t>(i)].left) < 1e-4f &&
                   std::abs(t[static_cast<std::size_t>(i)].right - taps_[static_cast<std::size_t>(i)].right) < 1e-4f;
        if (same) return;
        taps_ = t;
        numTaps_ = n;
        mode_ = s.mode;
        freeze_ = s.freeze;
        repaint();
    }
    void shoot(float level) {
        for (auto& s : shots_)
            if (!s.active) {
                s = {true, 0.0f, juce::jlimit(0.2f, 1.0f, level * 2.0f)};
                return;
            }
    }
    void advance(float dt) {
        bool any = false;
        const double end = numTaps_ > 0 ? taps_[static_cast<std::size_t>(numTaps_) - 1].seconds + 0.4 : 0.0;
        for (auto& s : shots_)
            if (s.active) {
                s.age += dt;
                if (s.age > end || s.age > 12.0f) s.active = false;
                any = true;
            }
        if (any) repaint();
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        const auto st = stageArea();
        // Left and right walls.
        g.setColour(kLeft.withAlpha(0.7f));
        g.fillRoundedRectangle(st.getX() - 6.0f, st.getY(), 4.0f, st.getHeight(), 2.0f);
        g.setColour(kRight.withAlpha(0.7f));
        g.fillRoundedRectangle(st.getRight() + 2.0f, st.getY(), 4.0f, st.getHeight(), 2.0f);
        g.setFont(font(11.0f, true));
        g.setColour(kLeft);
        g.drawText("L", juce::Rectangle<float>(st.getX() - 30.0f, st.getCentreY() - 8.0f, 18.0f, 16.0f), juce::Justification::centred);
        g.setColour(kRight);
        g.drawText("R", juce::Rectangle<float>(st.getRight() + 12.0f, st.getCentreY() - 8.0f, 18.0f, 16.0f), juce::Justification::centred);
        g.setColour(colours::grid);
        g.drawHorizontalLine(juce::roundToInt(st.getBottom()), st.getX(), st.getRight());
        const auto m = mapArea();
        g.setColour(colours::line);
        g.drawHorizontalLine(juce::roundToInt(m.getCentreY()), m.getX(), m.getRight());
        g.setColour(colours::faint);
        g.setFont(font(10.0f, true));
        g.drawText("ECHO MAP", m.withHeight(14.0f).translated(0, -16.0f), juce::Justification::centredLeft);
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }

    void paintDynamic(juce::Graphics& g) override {
        const auto m = mapArea();
        const double span = mapSpan();
        // Echo map: one bar per repeat, left side up and right side down.
        g.setFont(font(9.5f));
        for (int i = 0; i < numTaps_; ++i) {
            const auto& t = taps_[static_cast<std::size_t>(i)];
            const float x = m.getX() + static_cast<float>(t.seconds / span) * m.getWidth();
            if (x > m.getRight()) break;
            const float h = m.getHeight() * 0.5f - 2.0f;
            // Heights in dB (down to -42 dB) so quiet repeats stay visible.
            const auto db = [](float v) { return v > 0.0f ? juce::jlimit(0.05f, 1.0f, 1.0f + 20.0f * std::log10(v) / 42.0f) : 0.0f; };
            if (t.left > 0.0f) {
                g.setColour(kLeft.withAlpha(0.35f + 0.65f * t.left));
                g.fillRoundedRectangle(x - 2.0f, m.getCentreY() - h * db(t.left), 4.0f, h * db(t.left), 2.0f);
            }
            if (t.right > 0.0f) {
                g.setColour(kRight.withAlpha(0.35f + 0.65f * t.right));
                g.fillRoundedRectangle(x - 2.0f, m.getCentreY(), 4.0f, h * db(t.right), 2.0f);
            }
        }
        g.setColour(colours::faint);
        for (int k = 1; k <= 4; ++k) {
            const double sec = span * k / 4.0;
            g.drawText(sec < 1.0 ? juce::String(juce::roundToInt(sec * 1000.0)) + " ms" : juce::String(sec, 2) + " s",
                       juce::Rectangle<float>(m.getX() + m.getWidth() * k / 4.0f - 52.0f, m.getBottom() + 1.0f, 50.0f, 12.0f),
                       juce::Justification::centredRight);
        }
        if (freeze_) {
            g.setColour(colours::ice);
            g.setFont(font(13.0f, true));
            g.drawText("FROZEN", stageArea().withHeight(20.0f), juce::Justification::centredTop);
        }
        if (!animated() || numTaps_ == 0) return;

        // Bouncing balls: each one travels from echo to echo, landing on the side it is heard.
        const auto st = stageArea();
        const auto sideX = [&](const pat::dsp::DelayTap& t) {
            if (t.left > 0.0f && t.right > 0.0f) return st.getCentreX();
            return t.left > 0.0f ? st.getX() + 8.0f : st.getRight() - 8.0f;
        };
        for (const auto& s : shots_) {
            if (!s.active) continue;
            int k = 0;
            while (k < numTaps_ && taps_[static_cast<std::size_t>(k)].seconds <= s.age) ++k;
            const double t0 = k == 0 ? 0.0 : taps_[static_cast<std::size_t>(k) - 1].seconds;
            const float x0 = k == 0 ? st.getCentreX() : sideX(taps_[static_cast<std::size_t>(k) - 1]);
            const float lvl0 = k == 0 ? 1.0f : juce::jmax(taps_[static_cast<std::size_t>(k) - 1].left, taps_[static_cast<std::size_t>(k) - 1].right);
            if (k >= numTaps_) {  // last echo: fade out where it landed
                const float a = juce::jlimit(0.0f, 1.0f, 1.0f - static_cast<float>(s.age - t0) / 0.4f);
                drawBall(g, {x0, st.getBottom() - 10.0f}, lvl0 * s.level * a, x0);
                continue;
            }
            const auto& next = taps_[static_cast<std::size_t>(k)];
            const float x1 = sideX(next);
            const float f = static_cast<float>((s.age - t0) / juce::jmax(1e-4, next.seconds - t0));
            const float x = x0 + (x1 - x0) * f;
            const float lvl = juce::jmax(next.left, next.right);
            const float arc = st.getHeight() * (0.25f + 0.65f * lvl) * 4.0f * f * (1.0f - f);
            drawBall(g, {x, st.getBottom() - 10.0f - arc}, lvl * s.level, x);
            // Landing flash on the wall.
            if (k > 0 && s.age - t0 < 0.12) {
                const float a = 1.0f - static_cast<float>((s.age - t0) / 0.12);
                g.setColour((x0 < st.getCentreX() - 1 ? kLeft : x0 > st.getCentreX() + 1 ? kRight : kAccent).withAlpha(0.6f * a * lvl0));
                g.drawEllipse(juce::Rectangle<float>(40.0f, 40.0f).withCentre({x0, st.getBottom() - 10.0f}), 2.0f);
            }
        }
    }

private:
    struct Shot {
        bool active = false;
        float age = 0, level = 1;
    };
    juce::Rectangle<float> stageArea() const {
        auto r = getLocalBounds().toFloat().reduced(40.0f, 12.0f);
        return r.removeFromTop(r.getHeight() * 0.62f);
    }
    juce::Rectangle<float> mapArea() const {
        auto r = getLocalBounds().toFloat().reduced(40.0f, 12.0f);
        r.removeFromTop(r.getHeight() * 0.62f + 22.0f);
        return r.withTrimmedBottom(12.0f);
    }
    double mapSpan() const {
        // Window that shows the echoes down to about -40 dB (at least 0.5 s, at most 8 s).
        const double last = numTaps_ > 0 ? taps_[static_cast<std::size_t>(numTaps_) - 1].seconds : 1.0;
        return juce::jlimit(0.5, 8.0, last * 1.08);
    }
    void drawBall(juce::Graphics& g, juce::Point<float> p, float level, float x) {
        const auto st = stageArea();
        const float pos = juce::jlimit(-1.0f, 1.0f, (x - st.getCentreX()) / (st.getWidth() * 0.5f));
        const auto col = pos < 0 ? kAccent.interpolatedWith(kLeft, -pos) : kAccent.interpolatedWith(kRight, pos);
        const float rad = 4.0f + 9.0f * juce::jlimit(0.0f, 1.0f, level);
        if (glow()) {
            g.setColour(col.withAlpha(0.22f * level + 0.05f));
            g.fillEllipse(juce::Rectangle<float>(rad * 4.0f, rad * 4.0f).withCentre(p));
        }
        g.setColour(col.withAlpha(0.35f + 0.65f * juce::jlimit(0.0f, 1.0f, level)));
        g.fillEllipse(juce::Rectangle<float>(rad * 2.0f, rad * 2.0f).withCentre(p));
    }

    std::array<pat::dsp::DelayTap, kMaxTaps> taps_{};
    int numTaps_ = 0, mode_ = -1;
    bool freeze_ = false;
    std::array<Shot, kMaxShots> shots_{};
};

// ------------------------------------------------------------------------------------------- editor
PatPatPatEditor::PatPatPatEditor(PatPatPatProcessor& p)
    : PluginFrame(p, p.state, "PatPatPat", "ping-pong delay", kAccent, 1000, 620),
      proc_(p),
      mode_(p.state, kMode, {"MONO", "STEREO", "PING-PONG", "CROSS"}, kAccent),
      character_(p.state, kCharacter, {"CLEAN", "TAPE", "ANALOG", "LO-FI"}, kAccent),
      divL_(p.state, kDivL, "TIME L", kLeft, settings(), NeonKnob::Size::Big),
      timeL_(p.state, kTimeL, "TIME L", kLeft, settings(), NeonKnob::Size::Big),
      divR_(p.state, kDivR, "TIME R", kRight, settings(), NeonKnob::Size::Big),
      timeR_(p.state, kTimeR, "TIME R", kRight, settings(), NeonKnob::Size::Big),
      feedback_(p.state, kFeedback, "FEEDBACK", kAccent, settings(), NeonKnob::Size::Big),
      mix_(p.state, kMix, "MIX", kAccent, settings(), NeonKnob::Size::Big),
      lowCut_(p.state, kLowCut, "LOW CUT", kAccent, settings()),
      highCut_(p.state, kHighCut, "HIGH CUT", kAccent, settings()),
      drive_(p.state, kDrive, "DRIVE", kAccent, settings()),
      modRate_(p.state, kModRate, "MOD RATE", kAccent, settings()),
      modDepth_(p.state, kModDepth, "MOD DEPTH", kAccent, settings()),
      duck_(p.state, kDuck, "DUCKING", kAccent, settings()),
      width_(p.state, kWidth, "WIDTH", kAccent, settings()),
      diffusion_(p.state, kDiffusion, "DIFFUSION", kAccent, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      syncAttachment_(p.state, kSync, sync_),
      linkAttachment_(p.state, kLink, link_),
      freezeAttachment_(p.state, kFreeze, freeze_) {
    stage_ = std::make_unique<Stage>(settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{
             stage_.get(), &mode_, &character_, &divL_, &timeL_, &divR_, &timeR_, &feedback_, &mix_, &lowCut_, &highCut_, &drive_, &modRate_,
             &modDepth_, &duck_, &width_, &diffusion_, &output_, &sync_, &link_, &freeze_, &modeLabel_, &characterLabel_, &hint_})
        content().addAndMakeVisible(c);
    for (auto* t : {&sync_, &link_}) t->setColour(juce::ToggleButton::tickColourId, kAccent);
    freeze_.setClickingTogglesState(true);
    freeze_.setColour(juce::TextButton::buttonOnColourId, colours::ice.withAlpha(0.8f));
    freeze_.setTooltip("Hold the echoes you hear now forever (new sound is not added)");
    sync_.setTooltip("On: times are note lengths locked to the song. Off: times in milliseconds.");
    link_.setTooltip("Right time follows the left one");
    for (auto* l : {&modeLabel_, &characterLabel_, &hint_}) {
        l->setFont(font(11.5f, true));
        l->setColour(juce::Label::textColourId, colours::dim);
    }
    modeLabel_.setText("MODE", juce::dontSendNotification);
    characterLabel_.setText("CHARACTER", juce::dontSendNotification);
    hint_.setFont(font(12.0f));
    hint_.setJustificationType(juce::Justification::centred);
    hint_.setText("Play something: every hit bounces between left and right with the real echo times.", juce::dontSendNotification);
    duck_.setTooltip("Echoes get quieter while you play and come back in the gaps");
    diffusion_.setTooltip("Smears the echoes into a soft cloud");

    clock().add([this](float dt) { tick(dt); });
    setPresets(*p.presets);
    finishConstruction();
}

PatPatPatEditor::~PatPatPatEditor() = default;

float PatPatPatEditor::param(const char* id) const { return proc_.state.getRawParameterValue(id)->load(); }

void PatPatPatEditor::tick(float dt) {
    const bool sync = param(kSync) > 0.5f;
    const bool link = param(kLink) > 0.5f;
    const bool pro = proView();
    divL_.setVisible(sync);
    timeL_.setVisible(!sync);
    divR_.setVisible(pro && sync);
    timeR_.setVisible(pro && !sync);
    divR_.setEnabled(!link);
    timeR_.setEnabled(!link);
    stage_->setTaps(proc_.settings(), proc_.bpm.load());
    if (simulating_) return;
    // A new hit: the input level jumps well above its recent average.
    const float in = proc_.inputLevel.load();
    avgIn_ += juce::jmin(1.0f, dt * 3.0f) * (in - avgIn_);
    sinceShot_ += dt;
    if (in > 0.05f && in > avgIn_ * 1.6f && sinceShot_ > 0.2f) {
        stage_->shoot(in);
        sinceShot_ = 0.0f;
    }
    stage_->advance(dt);
}

void PatPatPatEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&character_, &characterLabel_, &link_, &lowCut_, &highCut_, &drive_, &modRate_,
                                                                       &modDepth_, &duck_, &width_, &diffusion_, &output_})
        c->setVisible(pro);
    hint_.setVisible(!pro);
    tick(0.0f);
}

void PatPatPatEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    const bool pro = proView();
    stage_->setBounds(r.removeFromTop(pro ? 230 : 300));
    r.removeFromTop(12);
    if (!pro) hint_.setBounds(r.removeFromBottom(22));
    auto left = r.removeFromLeft(290);
    r.removeFromLeft(16);
    modeLabel_.setBounds(left.removeFromTop(18));
    mode_.setBounds(left.removeFromTop(30));
    left.removeFromTop(8);
    if (pro) {
        characterLabel_.setBounds(left.removeFromTop(18));
        character_.setBounds(left.removeFromTop(30));
        left.removeFromTop(8);
    }
    auto toggles = left.removeFromTop(26);
    sync_.setBounds(toggles.removeFromLeft(120));
    if (pro) link_.setBounds(toggles.removeFromLeft(100));
    left.removeFromTop(10);
    freeze_.setBounds(left.removeFromTop(34).withWidth(130));

    if (pro) {
        auto row1 = r.removeFromTop(140);
        const int kw = row1.getWidth() / 4;
        const auto timeL = row1.removeFromLeft(kw).reduced(2, 2), timeR = row1.removeFromLeft(kw).reduced(2, 2);
        divL_.setBounds(timeL);
        timeL_.setBounds(timeL);
        divR_.setBounds(timeR);
        timeR_.setBounds(timeR);
        feedback_.setBounds(row1.removeFromLeft(kw).reduced(2, 2));
        mix_.setBounds(row1.reduced(2, 2));
        r.removeFromTop(4);
        auto row2 = r.removeFromTop(112);
        const int kw2 = row2.getWidth() / 9;
        for (auto* k : {&lowCut_, &highCut_, &drive_, &modRate_, &modDepth_, &duck_, &width_, &diffusion_, &output_})
            k->setBounds(row2.removeFromLeft(kw2).reduced(1, 2));
    } else {
        const int kw = r.getWidth() / 3;
        const auto t = r.removeFromLeft(kw).reduced(4, 0);
        divL_.setBounds(t);
        timeL_.setBounds(t);
        feedback_.setBounds(r.removeFromLeft(kw).reduced(4, 0));
        mix_.setBounds(r.reduced(4, 0));
    }
}

void PatPatPatEditor::simulateForSnapshot(float seconds) {
    simulating_ = true;
    stage_->setTaps(proc_.settings(), 120.0);
    stage_->shoot(0.8f);
    for (float t = 0; t < 0.62f + seconds * 0.01f; t += 0.02f) stage_->advance(0.02f);
    stage_->shoot(0.5f);
    for (int i = 0; i < 7; ++i) stage_->advance(0.02f);
    simulating_ = false;
}
