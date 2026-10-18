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
        const juce::String info = timeText(s, false, bpm) + (s.mode == pat::dsp::DelaySettings::Mono || s.link ? juce::String() : "     " + timeText(s, true, bpm));
        bool same = n == numTaps_ && s.freeze == freeze_ && info == info_;
        for (int i = 0; same && i < n; ++i)
            same = std::abs(t[static_cast<std::size_t>(i)].seconds - taps_[static_cast<std::size_t>(i)].seconds) < 1e-6 &&
                   std::abs(t[static_cast<std::size_t>(i)].left - taps_[static_cast<std::size_t>(i)].left) < 1e-4f &&
                   std::abs(t[static_cast<std::size_t>(i)].right - taps_[static_cast<std::size_t>(i)].right) < 1e-4f;
        if (same) return;
        taps_ = t;
        numTaps_ = n;
        freeze_ = s.freeze;
        info_ = info;
        repaint();
    }
    // Live echo level of each side (lights the walls).
    void setLevels(float l, float r, float dt) {
        const float fall = settings().neonFixed() ? 0.0f : std::exp(-dt * 5.0f);
        const auto toLevel = [](float v) { return juce::jlimit(0.0f, 1.0f, 1.0f + juce::Decibels::gainToDecibels(v, -60.0f) / 48.0f); };
        const float nl = juce::jmax(toLevel(l), levelL_ * fall), nr = juce::jmax(toLevel(r), levelR_ * fall);
        if (std::abs(nl - levelL_) > 0.004f || std::abs(nr - levelR_) > 0.004f) {
            levelL_ = nl;
            levelR_ = nr;
            repaint();
        }
    }
    void shoot(float level) {
        Shot* oldest = &shots_[0];
        for (auto& s : shots_) {
            if (!s.active) {
                oldest = &s;
                break;
            }
            if (s.age > oldest->age) oldest = &s;
        }
        *oldest = {true, 0.0f, juce::jlimit(0.3f, 1.0f, level * 2.5f)};
        repaint();
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
        g.setColour(colours::grid);
        g.drawHorizontalLine(juce::roundToInt(st.getBottom()), st.getX(), st.getRight());
        for (int k = 1; k < 4; ++k) g.drawVerticalLine(juce::roundToInt(st.getX() + st.getWidth() * k / 4.0f), st.getY() + 26.0f, st.getBottom());
        // Wall tracks (the live level fills them).
        for (const auto& w : {wall(true), wall(false)}) {
            g.setColour(colours::raised);
            g.fillRoundedRectangle(w, 3.0f);
        }
        g.setFont(font(12.0f, true));
        g.setColour(kLeft);
        g.drawText("L", wall(true).withY(st.getBottom() + 2.0f).withHeight(14.0f).expanded(6.0f, 0.0f), juce::Justification::centred);
        g.setColour(kRight);
        g.drawText("R", wall(false).withY(st.getBottom() + 2.0f).withHeight(14.0f).expanded(6.0f, 0.0f), juce::Justification::centred);
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
        const auto st = stageArea();
        // Times of the echoes.
        g.setColour(colours::dim);
        g.setFont(font(12.5f, true));
        g.drawText(info_, st.withHeight(22.0f), juce::Justification::centred);
        if (freeze_) {
            g.setColour(colours::ice);
            g.drawText("FROZEN", st.withHeight(22.0f).translated(0, 20.0f), juce::Justification::centred);
        }
        // Walls light up with the echoes heard on each side.
        for (int side = 0; side < 2; ++side) {
            const auto w = wall(side == 0);
            const float lv = side == 0 ? levelL_ : levelR_;
            const auto col = side == 0 ? kLeft : kRight;
            const auto fill = w.withTrimmedTop(w.getHeight() * (1.0f - lv));
            if (glow() && lv > 0.05f) {
                g.setColour(col.withAlpha(0.18f * lv));
                g.fillRoundedRectangle(fill.expanded(6.0f, 4.0f), 6.0f);
            }
            g.setColour(col.withAlpha(0.35f + 0.65f * lv));
            g.fillRoundedRectangle(fill, 3.0f);
        }

        // Echo map: one bar per repeat, left side up and right side down (heights in dB).
        const auto m = mapArea();
        const double span = mapSpan();
        const float h = m.getHeight() * 0.5f - 2.0f;
        const auto db = [](float v) { return v > 0.0f ? juce::jlimit(0.05f, 1.0f, 1.0f + 20.0f * std::log10(v) / 42.0f) : 0.0f; };
        for (int i = 0; i < numTaps_; ++i) {
            const auto& t = taps_[static_cast<std::size_t>(i)];
            const float x = m.getX() + static_cast<float>(t.seconds / span) * m.getWidth();
            if (x > m.getRight()) break;
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
        g.setFont(font(9.5f));
        for (int k = 1; k <= 4; ++k) {
            const double sec = span * k / 4.0;
            g.drawText(sec < 1.0 ? juce::String(juce::roundToInt(sec * 1000.0)) + " ms" : juce::String(sec, 2) + " s",
                       juce::Rectangle<float>(m.getX() + m.getWidth() * k / 4.0f - 52.0f, m.getBottom() + 1.0f, 50.0f, 12.0f),
                       juce::Justification::centredRight);
        }
        if (!animated() || numTaps_ == 0) return;

        // Balls: every hit you play travels from echo to echo, landing on the side it is heard.
        const float floorY = st.getBottom() - 12.0f;
        const auto sideX = [&](const pat::dsp::DelayTap& t) {
            if (t.left > 0.0f && t.right > 0.0f) return st.getCentreX();
            return t.left > 0.0f ? wall(true).getRight() + 14.0f : wall(false).getX() - 14.0f;
        };
        for (const auto& s : shots_) {
            if (!s.active) continue;
            int k = 0;
            while (k < numTaps_ && taps_[static_cast<std::size_t>(k)].seconds <= s.age) ++k;
            const double t0 = k == 0 ? 0.0 : taps_[static_cast<std::size_t>(k) - 1].seconds;
            const float x0 = k == 0 ? st.getCentreX() : sideX(taps_[static_cast<std::size_t>(k) - 1]);
            const float lvl0 = k == 0 ? 1.0f : juce::jmax(taps_[static_cast<std::size_t>(k) - 1].left, taps_[static_cast<std::size_t>(k) - 1].right);
            if (k >= numTaps_) {  // the last echo fades where it landed
                const float a = juce::jlimit(0.0f, 1.0f, 1.0f - static_cast<float>(s.age - t0) / 0.4f);
                drawBall(g, {x0, floorY}, lvl0 * s.level * a);
                continue;
            }
            const auto& next = taps_[static_cast<std::size_t>(k)];
            const float x1 = sideX(next);
            const float f = static_cast<float>((s.age - t0) / juce::jmax(1e-4, next.seconds - t0));
            const float lvl = juce::jmax(next.left, next.right);
            const float arc = (st.getHeight() - 40.0f) * (0.3f + 0.7f * s.level) * 4.0f * f * (1.0f - f) * (k == 0 ? 1.0f : 0.5f + 0.5f * lvl0);
            drawBall(g, {x0 + (x1 - x0) * f, floorY - arc}, juce::jmax(0.15f, lvl * s.level));
            if (k > 0 && s.age - t0 < 0.15) {  // landing ring
                const float a = 1.0f - static_cast<float>((s.age - t0) / 0.15);
                g.setColour(colourAt(x0).withAlpha(0.7f * a));
                g.drawEllipse(juce::Rectangle<float>(20.0f + 40.0f * (1.0f - a), 20.0f + 40.0f * (1.0f - a)).withCentre({x0, floorY}), 2.0f);
            }
        }
    }

private:
    struct Shot {
        bool active = false;
        float age = 0, level = 1;
    };
    static juce::String timeText(const pat::dsp::DelaySettings& s, bool right, double bpm) {
        const double sec = pat::dsp::DelayEngine::timeSeconds(s, right, bpm);
        const bool useR = right && !s.link;
        juce::String t = (right ? "R  " : (s.mode == pat::dsp::DelaySettings::Mono || s.link ? "" : "L  "));
        if (s.sync) t << pat::dsp::DelayEngine::divisionName(useR ? s.divisionR : s.divisionL) << "  =  ";
        return t + juce::String(juce::roundToInt(sec * 1000.0)) + " ms";
    }
    juce::Rectangle<float> stageArea() const {
        auto r = getLocalBounds().toFloat().reduced(14.0f, 10.0f);
        return r.removeFromTop(r.getHeight() * 0.64f);
    }
    juce::Rectangle<float> mapArea() const {
        auto r = getLocalBounds().toFloat().reduced(40.0f, 10.0f);
        r.removeFromTop(r.getHeight() * 0.64f + 30.0f);
        return r.withTrimmedBottom(12.0f);
    }
    juce::Rectangle<float> wall(bool left) const {
        const auto st = stageArea().withTrimmedTop(26.0f).withTrimmedBottom(2.0f);
        return left ? st.withWidth(8.0f) : st.withX(st.getRight() - 8.0f).withWidth(8.0f);
    }
    double mapSpan() const {
        const double last = numTaps_ > 0 ? taps_[static_cast<std::size_t>(numTaps_) - 1].seconds : 1.0;
        return juce::jlimit(0.5, 8.0, last * 1.08);
    }
    juce::Colour colourAt(float x) const {
        const auto st = stageArea();
        const float pos = juce::jlimit(-1.0f, 1.0f, (x - st.getCentreX()) / (st.getWidth() * 0.5f));
        return pos < 0 ? kAccent.interpolatedWith(kLeft, -pos) : kAccent.interpolatedWith(kRight, pos);
    }
    void drawBall(juce::Graphics& g, juce::Point<float> p, float level) {
        const auto col = colourAt(p.x);
        const float rad = 5.0f + 10.0f * juce::jlimit(0.0f, 1.0f, level);
        if (glow()) {
            g.setColour(col.withAlpha(0.22f * level + 0.05f));
            g.fillEllipse(juce::Rectangle<float>(rad * 4.0f, rad * 4.0f).withCentre(p));
        }
        g.setColour(col.withAlpha(0.4f + 0.6f * juce::jlimit(0.0f, 1.0f, level)));
        g.fillEllipse(juce::Rectangle<float>(rad * 2.0f, rad * 2.0f).withCentre(p));
    }

    std::array<pat::dsp::DelayTap, kMaxTaps> taps_{};
    int numTaps_ = 0;
    bool freeze_ = false;
    juce::String info_;
    float levelL_ = 0, levelR_ = 0;
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
    hint_.setText("Play something: every hit bounces between the walls with the real echo times; the walls light up with the echoes.",
                  juce::dontSendNotification);
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
    // Every new hit detected on the audio side throws a ball; the walls follow the echo levels.
    const int hits = proc_.onsets.load();
    if (hits != lastOnsets_) {
        lastOnsets_ = hits;
        stage_->shoot(proc_.onsetLevel.load());
    }
    stage_->setLevels(proc_.echoL.load(), proc_.echoR.load(), dt);
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
    static bool probed = false;
    if (!probed) {  // diagnostic: every preset on a private instance, a click in, echoes out
        probed = true;
        PatPatPatProcessor probe;
        probe.prepareToPlay(48000.0, 512);
        juce::String txt;
        juce::AudioBuffer<float> buf(2, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < static_cast<int>(probe.presets->entries().size()); ++i) {
            probe.presets->load(i);
            probe.prepareToPlay(48000.0, 512);
            juce::String hits;
            float lastL = 0, lastR = 0;
            int echoes = 0;
            for (int b = 0; b < 300 && echoes < 6; ++b) {  // 3.2 s
                buf.clear();
                if (b < 3)  // a 30 ms note at 440 Hz, peak 0.5
                    for (int k = 0; k < 512 && b * 512 + k < 1440; ++k) {
                        const float v = 0.5f * std::sin(static_cast<float>(b * 512 + k) * 0.0576f);
                        buf.setSample(0, k, v);
                        buf.setSample(1, k, v);
                    }
                probe.processBlock(buf, midi);
                const float pl = buf.getMagnitude(0, 0, 512), pr = buf.getMagnitude(1, 0, 512);
                if (b > 3 && (pl > 0.02f && pl > lastL * 1.5f)) { hits << "L@" << juce::String(b * 512 / 48.0, 0) << "ms(" << juce::String(pl, 2) << ") "; ++echoes; }
                if (b > 3 && (pr > 0.02f && pr > lastR * 1.5f)) { hits << "R@" << juce::String(b * 512 / 48.0, 0) << "ms(" << juce::String(pr, 2) << ") "; ++echoes; }
                lastL = pl;
                lastR = pr;
            }
            txt << probe.presets->entries()[static_cast<std::size_t>(i)].name << ": " << (hits.isEmpty() ? juce::String("NO ECHOES") : hits) << "\n";
        }
        juce::File(juce::SystemStats::getEnvironmentVariable("PATSUITE_SNAPSHOT", {})).getChildFile("PatPatPat-echoes.txt").replaceWithText(txt);
    }
    stage_->setTaps(proc_.settings(), 120.0);
    stage_->shoot(0.8f);
    for (float t = 0; t < 0.62f + seconds * 0.01f; t += 0.02f) stage_->advance(0.02f);
    stage_->shoot(0.5f);
    for (int i = 0; i < 7; ++i) stage_->advance(0.02f);
    stage_->setLevels(0.4f, 0.15f, 1.0f);
    simulating_ = false;
}
