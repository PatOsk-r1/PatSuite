#include "ui/master/MasterEditor.h"

#include <vector>

using namespace pat::ui;
using namespace master;

namespace {
const juce::Colour kAccent = colours::gold;
const juce::Colour kGr = colours::hot;
constexpr int kHistory = 600;  // points of ~21 ms: about 13 s
constexpr float kRangeDb = 48.0f;

juce::String lufsText(float v) { return v <= pat::dsp::LoudnessMeter::kSilence + 0.05f ? juce::String("--") : juce::String(v, 1); }
}  // namespace

// ------------------------------------------------------------------------------------------- history
class MasterEditor::History final : public LayeredCanvas {
public:
    explicit History(PerfSettings& s) : LayeredCanvas(s), pts_(kHistory) {}
    void add(const HistoryPoint* p, int n) {
        for (int i = 0; i < n; ++i) {
            pts_[static_cast<std::size_t>(head_)] = p[i];
            head_ = (head_ + 1) % kHistory;
        }
        if (n > 0) repaint();
    }
    void setCeiling(float db) {
        if (db != ceiling_) {
            ceiling_ = db;
            repaint();
        }
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        const auto a = area();
        g.setFont(font(10.0f));
        for (float d = 0.0f; d >= -kRangeDb; d -= 6.0f) {
            const float y = yFor(d);
            g.setColour(d == 0.0f ? colours::line : colours::grid);
            g.drawHorizontalLine(juce::roundToInt(y), a.getX(), a.getRight());
            g.setColour(colours::faint);
            g.drawText(juce::String(juce::roundToInt(d)), juce::Rectangle<float>(0, y - 6.0f, a.getX() - 4.0f, 12.0f), juce::Justification::centredRight);
        }
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }
    void paintDynamic(juce::Graphics& g) override {
        const auto a = area();
        const float dx = a.getWidth() / static_cast<float>(kHistory - 1);
        juce::Path in, out, gr;
        in.startNewSubPath(a.getX(), a.getBottom());
        out.startNewSubPath(a.getX(), a.getBottom());
        for (int i = 0; i < kHistory; ++i) {
            const auto& p = pts_[static_cast<std::size_t>((head_ + i) % kHistory)];
            const float x = a.getX() + dx * static_cast<float>(i);
            in.lineTo(x, yFor(toDb(p.in)));
            out.lineTo(x, yFor(toDb(p.out)));
            const float gy = a.getY() + juce::jlimit(0.0f, 1.0f, -p.gr / 12.0f) * a.getHeight() * 0.5f;
            if (i == 0) gr.startNewSubPath(x, gy);
            else gr.lineTo(x, gy);
        }
        in.lineTo(a.getRight(), a.getBottom());
        in.closeSubPath();
        out.lineTo(a.getRight(), a.getBottom());
        out.closeSubPath();
        g.setColour(colours::dim.withAlpha(0.25f));
        g.fillPath(in);
        g.setColour(kAccent.withAlpha(0.35f));
        g.fillPath(out);
        // Limiting: red from the top (deeper = further down).
        juce::Path grFill = gr;
        grFill.lineTo(a.getRight(), a.getY());
        grFill.lineTo(a.getX(), a.getY());
        grFill.closeSubPath();
        g.setColour(kGr.withAlpha(0.25f));
        g.fillPath(grFill);
        strokeNeon(g, gr, kGr, 1.6f, glow());
        // Ceiling line.
        juce::Path c;
        c.startNewSubPath(a.getX(), yFor(ceiling_));
        c.lineTo(a.getRight(), yFor(ceiling_));
        strokeNeon(g, c, kAccent, 1.4f, glow());
        g.setColour(kAccent);
        g.setFont(font(10.5f, true));
        g.drawText("CEILING " + juce::String(ceiling_, 1), juce::Rectangle<float>(a.getRight() - 120.0f, yFor(ceiling_) + 2.0f, 116.0f, 14.0f),
                   juce::Justification::centredRight);
        g.setColour(kGr);
        g.drawText("LIMITING", juce::Rectangle<float>(a.getX() + 6.0f, a.getY() + 4.0f, 100.0f, 14.0f), juce::Justification::centredLeft);
    }

private:
    static float toDb(float v) { return 20.0f * std::log10(std::max(v, 1e-5f)); }
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced(10.0f).withTrimmedLeft(24.0f); }
    float yFor(float db) const { return area().getY() + juce::jlimit(0.0f, 1.0f, -db / kRangeDb) * area().getHeight(); }

    std::vector<HistoryPoint> pts_;
    int head_ = 0;
    float ceiling_ = -1.0f;
};

// ------------------------------------------------------------------------------------------- loudness
class MasterEditor::LoudnessPanel final : public juce::Component {
public:
    LoudnessPanel(MasterProcessor& p, PerfSettings& s) : proc_(p), settings_(s) { setInterceptsMouseClicks(false, false); }
    void update() {
        const float v[6] = {proc_.integrated.load(), proc_.shortTerm.load(), proc_.momentary.load(), proc_.range.load(), proc_.truePeak.load(),
                            proc_.grDb.load()};
        bool changed = proc_.target() != target_;
        for (int i = 0; i < 6; ++i) changed |= std::abs(v[i] - values_[i]) > 0.04f;
        if (!changed) return;
        std::copy(v, v + 6, values_);
        target_ = proc_.target();
        repaint();
    }
    void simulate(float integrated, float st, float m, float lra, float tp, float gr) {
        const float v[6] = {integrated, st, m, lra, tp, gr};
        std::copy(v, v + 6, values_);
        target_ = proc_.target();
        repaint();
    }
    void paint(juce::Graphics& g) override {
        drawPanel(g, getLocalBounds(), "LOUDNESS (LUFS)", kAccent);
        auto r = getLocalBounds().reduced(14, 10).withTrimmedTop(24);
        const auto& t = kTargets[target_];
        const float integrated = values_[0];
        // Big integrated value, coloured by distance to the target.
        const bool measured = integrated > pat::dsp::LoudnessMeter::kSilence + 0.05f;
        const float diff = integrated - t.lufs;
        const auto col = !measured ? colours::dim : std::abs(diff) <= 1.0f ? colours::lime : (std::abs(diff) <= 3.0f ? colours::gold : colours::hot);
        auto big = r.removeFromTop(52);
        g.setColour(col);
        g.setFont(font(42.0f, true));
        g.drawText(lufsText(integrated), big.removeFromLeft(big.getWidth() * 2 / 3), juce::Justification::centredLeft);
        g.setColour(colours::dim);
        g.setFont(font(12.0f, true));
        g.drawText("INTEGRATED", big, juce::Justification::centredLeft);
        // Target line.
        g.setFont(font(12.5f));
        g.setColour(colours::text);
        juce::String targetLine = juce::String(t.name) + ":  " + juce::String(t.lufs, 0) + " LUFS, peaks " + juce::String(t.ceilingDb, 1) + " dBTP";
        g.drawText(targetLine, r.removeFromTop(18), juce::Justification::centredLeft);
        g.setColour(col);
        const juce::String advice = !measured ? juce::String::fromUTF8("Dale play a la canción entera para medir")
                                    : std::abs(diff) <= 1.0f ? juce::String::fromUTF8("En el objetivo")
                                    : diff > 0 ? juce::String::fromUTF8("Pasado por ") + juce::String(diff, 1) + " LU: bajá GAIN"
                                               : juce::String::fromUTF8("Faltan ") + juce::String(-diff, 1) + " LU: subí GAIN (o MATCH)";
        g.drawText(advice, r.removeFromTop(18), juce::Justification::centredLeft);
        r.removeFromTop(4);
        const char* labels[4] = {"SHORT-TERM", "MOMENTARY", "RANGE (LRA)", "TRUE PEAK"};
        const juce::String vals[4] = {lufsText(values_[1]), lufsText(values_[2]), juce::String(values_[3], 1) + " LU",
                                      values_[4] < -100.0f ? juce::String("--") : juce::String(values_[4], 1) + " dBTP"};
        for (int i = 0; i < 4; ++i) {
            auto row = r.removeFromTop(20);
            g.setColour(colours::dim);
            g.setFont(font(11.5f, true));
            g.drawText(labels[i], row.removeFromLeft(row.getWidth() / 2), juce::Justification::centredLeft);
            g.setColour(i == 3 && values_[4] > t.ceilingDb + 0.05f ? colours::hot : colours::text);
            g.setFont(font(14.0f, true));
            g.drawText(vals[i], row, juce::Justification::centredRight);
        }
        // Limiting now.
        r.removeFromTop(4);
        auto grRow = r.removeFromTop(16).toFloat();
        g.setColour(colours::raised);
        g.fillRoundedRectangle(grRow, 4.0f);
        const float depth = juce::jlimit(0.0f, 1.0f, -values_[5] / 12.0f);
        g.setColour(kGr);
        g.fillRoundedRectangle(grRow.withWidth(grRow.getWidth() * depth), 4.0f);
        g.setColour(colours::text);
        g.setFont(font(10.5f, true));
        g.drawText("LIMITING " + juce::String(values_[5], 1) + " dB", grRow, juce::Justification::centred);
    }

private:
    MasterProcessor& proc_;
    PerfSettings& settings_;
    float values_[6] = {-70, -70, -70, 0, -120, 0};
    int target_ = 0;
};

// ------------------------------------------------------------------------------------------- editor
MasterEditor::MasterEditor(MasterProcessor& p)
    : PluginFrame(p, p.state, "PatMaster", "master limiter", kAccent, 1000, 620),
      proc_(p),
      gain_(p.state, kGain, "GAIN", kAccent, settings(), NeonKnob::Size::Big),
      ceiling_(p.state, kCeiling, "CEILING", kAccent, settings(), NeonKnob::Size::Big),
      release_(p.state, kRelease, "RELEASE", kAccent, settings()),
      link_(p.state, kLink, "STEREO LINK", kAccent, settings()),
      clip_(p.state, kClip, "SOFT CLIP", kAccent, settings()),
      autoReleaseAtt_(p.state, kAutoRelease, autoRelease_),
      truePeakAtt_(p.state, kTruePeak, truePeak_) {
    history_ = std::make_unique<History>(settings());
    loudness_ = std::make_unique<LoudnessPanel>(p, settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{history_.get(), loudness_.get(), &gain_, &ceiling_, &release_, &link_, &clip_,
                                                                       &autoRelease_, &truePeak_, &target_, &match_, &reset_, &hint_})
        content().addAndMakeVisible(c);
    for (int i = 0; i < kNumTargets; ++i) target_.addItem(juce::String("Target: ") + kTargets[i].name, i + 1);
    target_.setSelectedId(p.target() + 1, juce::dontSendNotification);
    target_.onChange = [this] { proc_.setTarget(target_.getSelectedId() - 1); };
    match_.setTooltip(juce::String::fromUTF8("Mueve GAIN y CEILING para que la canción quede en el objetivo (primero dale play un rato)"));
    match_.onClick = [this] {
        if (!proc_.matchTarget())
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "PatMaster",
                                                   juce::String::fromUTF8("Primero dale play a la canción (mejor entera) para que PatMaster la mida."));
    };
    reset_.setTooltip(juce::String::fromUTF8("Empieza a medir de nuevo"));
    reset_.onClick = [this] { proc_.resetMeter(); };
    for (auto* b : {&match_, &reset_}) b->setColour(juce::TextButton::buttonOnColourId, kAccent);
    for (auto* t : {&autoRelease_, &truePeak_}) t->setColour(juce::ToggleButton::tickColourId, kAccent);
    gain_.setTooltip("How much louder the song goes into the limiter");
    ceiling_.setTooltip("The highest the output may reach");
    clip_.setTooltip("Rounds the loudest peaks before limiting: louder, a little grit");
    truePeak_.setTooltip("Also catches peaks between samples (what streaming platforms measure)");
    hint_.setFont(font(12.0f));
    hint_.setColour(juce::Label::textColourId, colours::dim);
    hint_.setJustificationType(juce::Justification::centred);
    hint_.setText(juce::String::fromUTF8("Elegí la plataforma, dale play a la canción y tocá MATCH TARGET. Ajustá GAIN a gusto."),
                  juce::dontSendNotification);

    std::vector<HistoryPoint> buf(512);
    clock().add([this, buf](float) mutable {
        if (simulating_) return;
        proc_.history.skipToNewest(kHistory);
        const auto n = proc_.history.pop(buf.data(), buf.size());
        history_->add(buf.data(), static_cast<int>(n));
        history_->setCeiling(proc_.state.getRawParameterValue(kCeiling)->load());
        loudness_->update();
        release_.setEnabled(proc_.state.getRawParameterValue(kAutoRelease)->load() < 0.5f);
        if (target_.getSelectedId() != proc_.target() + 1) target_.setSelectedId(proc_.target() + 1, juce::dontSendNotification);
    });
    setPresets(*p.presets);
    finishConstruction();
}

MasterEditor::~MasterEditor() = default;

void MasterEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&release_, &link_, &clip_, &autoRelease_, &truePeak_}) c->setVisible(pro);
    hint_.setVisible(!pro);
}

void MasterEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    auto top = r.removeFromTop(330);
    auto right = top.removeFromRight(330);
    top.removeFromRight(12);
    history_->setBounds(top);
    auto buttons = right.removeFromBottom(32);
    right.removeFromBottom(6);
    auto targetRow = right.removeFromBottom(28);
    right.removeFromBottom(6);
    loudness_->setBounds(right);
    target_.setBounds(targetRow);
    reset_.setBounds(buttons.removeFromRight(90));
    buttons.removeFromRight(8);
    match_.setBounds(buttons);
    r.removeFromTop(12);
    if (proView()) {
        const int kw = r.getWidth() / 6;
        gain_.setBounds(r.removeFromLeft(kw));
        ceiling_.setBounds(r.removeFromLeft(kw));
        auto toggles = r.removeFromRight(kw + 20).reduced(0, 40);
        autoRelease_.setBounds(toggles.removeFromTop(28));
        toggles.removeFromTop(10);
        truePeak_.setBounds(toggles.removeFromTop(28));
        const int sw = r.getWidth() / 3;
        for (auto* k : {&release_, &link_, &clip_}) k->setBounds(r.removeFromLeft(sw).reduced(4, 20));
    } else {
        hint_.setBounds(r.removeFromBottom(22));
        auto centre = r.withSizeKeepingCentre(480, r.getHeight());
        gain_.setBounds(centre.removeFromLeft(240));
        ceiling_.setBounds(centre);
    }
}

void MasterEditor::simulateForSnapshot(float) {
    simulating_ = true;
    std::vector<HistoryPoint> pts(kHistory);
    for (int i = 0; i < kHistory; ++i) {
        const float beat = std::fmod(static_cast<float>(i) * 0.021f, 0.47f);
        const float in = 0.5f + 0.9f * std::exp(-beat * 18.0f) + 0.05f * std::sin(static_cast<float>(i) * 0.7f);
        const float out = std::min(in, 0.89f);
        pts[static_cast<std::size_t>(i)] = {in, out, 20.0f * std::log10(out / std::max(in, out))};
    }
    history_->add(pts.data(), kHistory);
    loudness_->simulate(-13.6f, -12.9f, -11.8f, 5.2f, -1.1f, -3.4f);
}
