#include "ui/sidechain/SidechainEditor.h"

using namespace pat::ui;
using namespace sidechain;

namespace {
const juce::Colour kAccent = colours::pink;  // plugin accent
const juce::Colour kKick = colours::pink;    // trigger waveform
const juce::Colour kGain = colours::cyan;    // gain curve
constexpr int kHistory = 1200;               // graph points kept (128 samples each, about 3 s)
}  // namespace

// ------------------------------------------------------------------------------------------- graph
class SidechainEditor::PumpGraph final : public LayeredCanvas {
public:
    explicit PumpGraph(PerfSettings& s) : LayeredCanvas(s), history_(kHistory) {}

    // Appends new points; returns true when something new arrived.
    bool add(const pat::dsp::GraphPoint* pts, int n) {
        for (int i = 0; i < n; ++i) {
            history_[static_cast<std::size_t>(head_)] = pts[i];
            head_ = (head_ + 1) % kHistory;
        }
        if (n > 0) repaint();
        return n > 0;
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        const float mid = r.getCentreY();
        g.setColour(colours::grid);
        for (int i = 1; i < 8; ++i) g.drawVerticalLine(juce::roundToInt(r.getWidth() * i / 8.0f), r.getY(), r.getBottom());
        g.drawHorizontalLine(juce::roundToInt(mid), r.getX(), r.getRight());
        g.setColour(colours::line);
        g.drawHorizontalLine(juce::roundToInt(mid + 8.0f), r.getX(), r.getRight());  // gain = 1 line
        g.setFont(font(10.5f, true));
        g.setColour(kKick.withAlpha(0.6f));
        g.drawText("KICK / TRIGGER", r.reduced(10.0f, 6.0f), juce::Justification::topLeft);
        g.setColour(kGain.withAlpha(0.6f));
        g.drawText("GAIN", r.reduced(10.0f, 6.0f), juce::Justification::bottomLeft);
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }

    void paintDynamic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat().reduced(2.0f);
        const float mid = r.getCentreY();
        const float topH = mid - r.getY() - 6.0f;
        const float gainTop = mid + 8.0f, gainH = r.getBottom() - gainTop - 4.0f;
        // Low level: fewer points; the curve is still exact at that resolution.
        const int step = settings().profile().spectrumBands > 0 ? 3 : 1;
        const int count = kHistory / step;
        const float dx = r.getWidth() / static_cast<float>(count - 1);
        juce::Path kickTop, kickFill, gain;
        for (int i = 0; i < count; ++i) {
            const auto& p = history_[static_cast<std::size_t>((head_ + i * step) % kHistory)];
            const float x = r.getX() + dx * static_cast<float>(i);
            const float k = juce::jlimit(0.0f, 1.0f, p.trigger) * topH;
            const float gy = gainTop + (1.0f - juce::jlimit(0.0f, 1.0f, p.gain)) * gainH;
            if (i == 0) {
                kickTop.startNewSubPath(x, mid - 6.0f - k);
                kickFill.startNewSubPath(x, mid - 6.0f);
                gain.startNewSubPath(x, gy);
            }
            kickTop.lineTo(x, mid - 6.0f - k);
            kickFill.lineTo(x, mid - 6.0f - k);
            gain.lineTo(x, gy);
        }
        kickFill.lineTo(r.getRight(), mid - 6.0f);
        kickFill.closeSubPath();
        g.setColour(kKick.withAlpha(0.18f));
        g.fillPath(kickFill);
        strokeNeon(g, kickTop, kKick, 1.4f, glow());
        juce::Path gainFill(gain);
        gainFill.lineTo(r.getRight(), gainTop);
        gainFill.lineTo(r.getX(), gainTop);
        gainFill.closeSubPath();
        g.setColour(kGain.withAlpha(0.12f));
        g.fillPath(gainFill);
        strokeNeon(g, gain, kGain, 2.0f, glow());
    }

private:
    std::vector<pat::dsp::GraphPoint> history_;
    int head_ = 0;
};

// ------------------------------------------------------------------------------------------- pulse ring
class SidechainEditor::PulseRing final : public juce::Component {
public:
    explicit PulseRing(PerfSettings& s) : settings_(s) { setInterceptsMouseClicks(false, false); }
    void trigger() {
        pulse_ = 1.0f;
        repaint();
    }
    void setLabel(const juce::String& top, const juce::String& bottom) {
        if (top == top_ && bottom == bottom_) return;
        top_ = top;
        bottom_ = bottom;
        repaint();
    }
    void advance(float dt) {
        if (pulse_ <= 0.0f) return;
        // Animated: the ring expands and fades over 0.35 s. Neon fixed: one steady flash per tick.
        pulse_ = settings_.neonFixed() ? 0.0f : juce::jmax(0.0f, pulse_ - dt / 0.35f);
        repaint();
    }
    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        const auto c = r.getCentre();
        const float base = juce::jmin(r.getWidth(), r.getHeight()) * 0.32f;
        const bool neonFixed = settings_.neonFixed();
        const float p = neonFixed ? (flash_ ? 1.0f : 0.0f) : pulse_;
        const bool glow = !neonFixed && settings_.profile().glow;
        if (!neonFixed && pulse_ > 0.0f) {  // expanding echo ring
            const float er = base * (1.0f + 0.5f * (1.0f - pulse_));
            g.setColour(kAccent.withAlpha(0.6f * pulse_));
            g.drawEllipse(juce::Rectangle<float>(er * 2, er * 2).withCentre(c), 2.0f);
        }
        juce::Path ring;
        ring.addEllipse(juce::Rectangle<float>(base * 2, base * 2).withCentre(c));
        strokeNeon(g, ring, kAccent.withMultipliedBrightness(0.55f + 0.45f * p), 3.0f + 2.0f * p, glow);
        g.setColour(colours::text);
        g.setFont(font(15.0f, true));
        g.drawText(top_, juce::Rectangle<float>(c.x - base, c.y - 18.0f, base * 2, 20.0f), juce::Justification::centred);
        g.setColour(colours::dim);
        g.setFont(font(11.0f, true));
        g.drawText(bottom_, juce::Rectangle<float>(c.x - base, c.y + 2.0f, base * 2, 16.0f), juce::Justification::centred);
    }
    void setFlash(bool on) {
        if (flash_ != on) {
            flash_ = on;
            repaint();
        }
    }

private:
    PerfSettings& settings_;
    float pulse_ = 0;
    bool flash_ = false;
    juce::String top_, bottom_;
};

// ------------------------------------------------------------------------------------------- editor

SidechainEditor::SidechainEditor(SidechainProcessor& p)
    : PluginFrame(p, p.state, "PatChain", "pump & ducking", kAccent, 940, 600),
      proc_(p),
      pump_(p.state, kPump, "PUMP", kAccent, settings(), NeonKnob::Size::Big),
      rate_(p.state, kRate, {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T", "1/8 D"}, kAccent),
      shape_(p.state, kShape, {"CLASSIC", "SMOOTH", "PUNCHY", "LONG", "CUT", "BOUNCE", "SWELL", "BREATHE"}, kAccent),
      trigger_(p.state, kTrigger, {"AUTO", "TEMPO", "SIDECHAIN"}, kGain),
      threshold_(p.state, kThreshold, "THRESHOLD", kAccent, settings()),
      attack_(p.state, kAttack, "ATTACK", kAccent, settings()),
      release_(p.state, kRelease, "RELEASE", kAccent, settings()),
      knee_(p.state, kKnee, "KNEE", kAccent, settings()),
      ratio_(p.state, kRatio, "RATIO", kAccent, settings()),
      scLow_(p.state, kScLow, "SC LOW CUT", kGain, settings()),
      scHigh_(p.state, kScHigh, "SC HIGH CUT", kGain, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      listenAttachment_(p.state, kListen, listen_) {
    graph_ = std::make_unique<PumpGraph>(settings());
    ring_ = std::make_unique<PulseRing>(settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{graph_.get(), ring_.get(), &pump_, &rate_, &trigger_, &threshold_, &attack_,
                                                                       &release_, &knee_, &ratio_, &scLow_, &scHigh_, &output_, &listen_, &rateLabel_, &shape_, &shapeLabel_,
                                                                       &triggerLabel_, &hint_, &scStatus_, &help_})
        content().addAndMakeVisible(c);
    listen_.setColour(juce::ToggleButton::tickColourId, kGain);
    listen_.setTooltip("Hear the filtered sidechain signal (what triggers the ducking)");
    rate_.setColumns(5);
    shape_.setColumns(4);
    for (auto* l : {&rateLabel_, &shapeLabel_, &triggerLabel_, &hint_}) {
        l->setFont(font(11.5f, true));
        l->setColour(juce::Label::textColourId, colours::dim);
    }
    rateLabel_.setText("RHYTHM", juce::dontSendNotification);
    shapeLabel_.setText("SHAPE (TEMPO)", juce::dontSendNotification);
    triggerLabel_.setText("TRIGGER", juce::dontSendNotification);
    scStatus_.setFont(font(11.0f, true));
    scStatus_.setJustificationType(juce::Justification::centred);
    scStatus_.setMinimumHorizontalScale(0.8f);
    help_.setColour(juce::TextButton::buttonColourId, colours::raised);
    help_.setTooltip(juce::String::fromUTF8("Cómo mandar el bombo a PatChain en FL Studio"));
    help_.onClick = [this] {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon, juce::String::fromUTF8("Conectar el bombo (FL Studio)"),
            juce::String::fromUTF8(
                "Igual que con el Fruity Limiter, el bombo tiene que llegar al plugin:\n\n"
                "1. En el mixer, seleccioná la pista del bombo.\n"
                "2. En la pista donde está PatChain, clic derecho en la flechita de ruteo de abajo > "
                "\"Sidechain to this track\".\n"
                "3. Abrí las opciones de PatChain (el engranaje arriba a la izquierda de la ventana del plugin) > "
                "pestaña \"Processing\".\n"
                "4. En las conexiones de entrada, a la entrada 2 (Sidechain) asignale la pista del bombo.\n\n"
                "Cuando llega, este cartel dice \"BOMBO: llega señal\" y la pista baja con cada golpe. "
                "PUMP regula cuánto baja; en Pro, THRESHOLD, RATIO, ATTACK y RELEASE funcionan como en el Fruity Limiter."));
    };
    hint_.setFont(font(12.0f));
    hint_.setJustificationType(juce::Justification::centred);
    hint_.setText("Pick a shape and a rhythm (no routing needed), or route your kick. PUMP sets how deep it ducks.", juce::dontSendNotification);

    clock().add([this](float dt) {
        if (simulating_) return;
        proc_.graph.skipToNewest(kHistory);
        const auto n = proc_.graph.pop(buf_.data(), buf_.size());
        graph_->add(buf_.data(), static_cast<int>(n));
        const int t = proc_.triggers.load();
        const bool hit = t != lastTriggers_;
        lastTriggers_ = t;
        if (hit) ring_->trigger();
        ring_->setFlash(hit);
        ring_->advance(dt);
        const bool sc = proc_.usingSidechain.load();
        const int rate = rate_.selected();
        ring_->setLabel(sc ? "KICK" : juce::String(pat::dsp::SidechainEngine::divisionName(rate)), sc ? "sidechain" : "tempo");
        // Is the kick arriving? (the most common problem: the sidechain is not connected in the host)
        const int trigger = juce::roundToInt(proc_.state.getRawParameterValue(kTrigger)->load());
        const bool signal = proc_.sidechainSignal.load();
        juce::String status;
        juce::Colour col = colours::dim;
        if (signal) {
            status = juce::String::fromUTF8("BOMBO: llega señal");
            col = colours::lime;
        } else if (trigger == 2) {
            status = juce::String::fromUTF8("NO LLEGA EL BOMBO");
            col = colours::hot;
        } else if (trigger == 0) {
            status = juce::String::fromUTF8("Sin bombo: bombea a tempo");
        } else {
            status = "Modo tempo";
        }
        if (scStatus_.getText() != status) {
            scStatus_.setText(status, juce::dontSendNotification);
            scStatus_.setColour(juce::Label::textColourId, col);
        }
    });
    setPresets(*p.presets);
    finishConstruction();
}

SidechainEditor::~SidechainEditor() = default;

void SidechainEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&trigger_, &triggerLabel_, &threshold_, &attack_, &release_, &knee_, &ratio_,
                                                                       &scLow_, &scHigh_, &output_, &listen_})
        c->setVisible(pro);
    hint_.setVisible(!pro);
}

void SidechainEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    auto top = r.removeFromTop(proView() ? 250 : 290);
    auto ringCol = top.removeFromRight(200);
    help_.setBounds(ringCol.removeFromBottom(26).withSizeKeepingCentre(150, 24));
    scStatus_.setBounds(ringCol.removeFromBottom(22));
    ring_->setBounds(ringCol);
    top.removeFromRight(12);
    graph_->setBounds(top);
    r.removeFromTop(12);
    if (proView()) {
        auto left = r.removeFromLeft(240);
        pump_.setBounds(left.removeFromTop(100));
        rateLabel_.setBounds(left.removeFromTop(18));
        rate_.setBounds(left.removeFromTop(56));
        left.removeFromTop(4);
        shapeLabel_.setBounds(left.removeFromTop(18));
        shape_.setBounds(left.removeFromTop(56));
        r.removeFromLeft(16);
        auto row1 = r.removeFromTop(118);
        const int kw = row1.getWidth() / 5;
        for (auto* k : {&threshold_, &attack_, &release_, &knee_, &ratio_}) k->setBounds(row1.removeFromLeft(kw).reduced(2, 4));
        r.removeFromTop(6);
        auto row2 = r.removeFromTop(118);
        auto trig = row2.removeFromLeft(kw * 2).reduced(4, 30);
        triggerLabel_.setBounds(trig.removeFromTop(18));
        trigger_.setBounds(trig.removeFromTop(30));
        scLow_.setBounds(row2.removeFromLeft(kw).reduced(2, 4));
        scHigh_.setBounds(row2.removeFromLeft(kw).reduced(2, 4));
        auto last = row2.reduced(2, 4);
        output_.setBounds(last.removeFromTop(86));
        listen_.setBounds(last.withSizeKeepingCentre(last.getWidth() + 40, 24));
    } else {
        hint_.setBounds(r.removeFromBottom(22));
        auto leftCol = r.removeFromLeft(320).withSizeKeepingCentre(320, 90);
        auto rightCol = r.removeFromRight(320).withSizeKeepingCentre(320, 90);
        shapeLabel_.setBounds(leftCol.removeFromTop(18));
        shape_.setBounds(leftCol.removeFromTop(64));
        rateLabel_.setBounds(rightCol.removeFromTop(18));
        rate_.setBounds(rightCol.removeFromTop(64));
        pump_.setBounds(r);
    }
}

void SidechainEditor::simulateForSnapshot(float seconds) {
    simulating_ = true;
    // Synthetic four-on-the-floor kick at 128 bpm with the matching gain curve.
    std::vector<pat::dsp::GraphPoint> pts;
    const double perPoint = 128.0 / 48000.0, beat = 60.0 / 128.0;
    for (int i = 0; i < kHistory; ++i) {
        const double t = std::fmod(i * perPoint, beat);
        const float kick = static_cast<float>(std::exp(-t * 22.0) * std::abs(std::sin(t * 2.0 * 3.14159 * 55.0)));
        const float g = static_cast<float>(1.0 - 0.75 * std::pow(std::max(0.0, 1.0 - t / 0.25), 2.0));
        pts.push_back({kick, g});
    }
    graph_->add(pts.data(), static_cast<int>(pts.size()));
    ring_->setLabel("KICK", "sidechain");
    ring_->trigger();
    ring_->advance(seconds * 0.05f);
    simulating_ = false;
}
