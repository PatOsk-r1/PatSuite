#include "ui/pan/PatanEditor.h"

using namespace pat::ui;
using namespace patan;
using pat::dsp::CurvePoint;
using pat::dsp::PanCurve;

namespace {
const juce::Colour kAccent = colours::gold;
const juce::Colour kLeft = colours::cyan;
const juce::Colour kRight = colours::pink;
}  // namespace

// ------------------------------------------------------------------------------------------- curve
class PatanEditor::CurveEditor final : public LayeredCanvas {
public:
    CurveEditor(PatanProcessor& p, PerfSettings& s) : LayeredCanvas(s), proc_(p) {}

    int grid = 8;
    bool snap = true;

    void setMode(int m) {
        if (m == mode_) return;
        mode_ = m;
        invalidateStatic();
        repaint();
    }
    void setPlayhead(double phase, float value, bool show) {
        if (std::abs(phase - phase_) < 1.0e-4 && std::abs(value - value_) < 1.0e-4f && show == showHead_ && version_ == proc_.curveVersion()) return;
        phase_ = phase;
        value_ = value;
        showHead_ = show;
        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override {
        auto& c = proc_.curve();
        drag_ = hitPoint(e.position);
        bendDrag_ = drag_ < 0 ? hitBend(e.position) : -1;
        if (e.mods.isPopupMenu()) {
            if (drag_ >= 0) showPointMenu(drag_);
            drag_ = -1;
            return;
        }
        if (bendDrag_ >= 0) {
            bendStart_ = c.points()[static_cast<std::size_t>(bendDrag_)].bend;
            return;
        }
        if (drag_ < 0) {  // new point where the user clicked
            drag_ = c.add(snapX(toX(e.position.x), e.mods), toY(e.position.y));
            proc_.curveChanged();
        }
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        auto& c = proc_.curve();
        if (bendDrag_ >= 0) {
            const float dy = static_cast<float>(e.getDistanceFromDragStartY()) / area().getHeight();
            c.setBend(bendDrag_, bendStart_ + dy * (e.mods.isShiftDown() ? 0.5f : 2.0f));
            proc_.curveChanged();
            repaint();
            return;
        }
        if (drag_ < 0) return;
        c.move(drag_, snapX(toX(e.position.x), e.mods), toY(e.position.y));
        proc_.curveChanged();
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override {
        drag_ = bendDrag_ = -1;
    }
    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const int i = hitPoint(e.position);
        if (i > 0) {
            proc_.curve().remove(i);
            proc_.curveChanged();
        } else if (const int b = hitBend(e.position); b >= 0) {
            proc_.curve().setBend(b, 0.0f);  // straighten
            proc_.curveChanged();
        }
        repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const int h = hitPoint(e.position), b = h < 0 ? hitBend(e.position) : -1;
        if (h != hover_ || b != hoverBend_) {
            hover_ = h;
            hoverBend_ = b;
            setMouseCursor(h >= 0 || b >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::CrosshairCursor);
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override {
        hover_ = hoverBend_ = -1;
        repaint();
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff07090f));
        g.fillRoundedRectangle(r, 6.0f);
        const auto a = area();
        g.setColour(colours::grid);
        for (int i = 1; i < grid; ++i) g.drawVerticalLine(juce::roundToInt(a.getX() + a.getWidth() * i / grid), a.getY(), a.getBottom());
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine(juce::roundToInt(a.getY() + a.getHeight() * i / 4.0f), a.getX(), a.getRight());
        g.setColour(colours::line);
        g.drawHorizontalLine(juce::roundToInt(a.getCentreY()), a.getX(), a.getRight());
        g.drawRect(a, 1.0f);
        // Axis labels depend on the mode.
        const char* top = mode_ == 1 ? "100%" : mode_ == 2 ? "WIDE" : "R";
        const char* mid = mode_ == 1 ? "50%" : mode_ == 2 ? "1x" : "C";
        const char* bottom = mode_ == 1 ? "0%" : mode_ == 2 ? "MONO" : "L";
        g.setFont(font(10.5f, true));
        g.setColour(mode_ == 0 ? kRight.withAlpha(0.8f) : colours::dim);
        g.drawText(top, juce::Rectangle<float>(0, a.getY() - 6, a.getX() - 4, 12), juce::Justification::centredRight);
        g.setColour(colours::dim);
        g.drawText(mid, juce::Rectangle<float>(0, a.getCentreY() - 6, a.getX() - 4, 12), juce::Justification::centredRight);
        g.setColour(mode_ == 0 ? kLeft.withAlpha(0.8f) : colours::dim);
        g.drawText(bottom, juce::Rectangle<float>(0, a.getBottom() - 6, a.getX() - 4, 12), juce::Justification::centredRight);
        g.setColour(colours::faint);
        g.setFont(font(10.0f));
        for (int i = 0; i < grid; i += juce::jmax(1, grid / 4))
            g.drawText(juce::String(i + 1), juce::Rectangle<float>(a.getX() + a.getWidth() * i / grid + 4, a.getBottom() + 2, 30, 14),
                       juce::Justification::centredLeft);
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }

    void paintDynamic(juce::Graphics& g) override {
        version_ = proc_.curveVersion();
        const auto& c = proc_.curve();
        const auto a = area();
        // The curve, sampled finely so holds and bends look exact.
        const int steps = juce::jmax(64, static_cast<int>(a.getWidth()));
        juce::Path line, fill;
        for (int i = 0; i <= steps; ++i) {
            const float x = static_cast<float>(i) / steps;
            const float px = a.getX() + x * a.getWidth();
            const float py = toPy(c.valueAt(juce::jmin(x, 0.99999f)));
            if (i == 0) {
                line.startNewSubPath(px, py);
                fill.startNewSubPath(px, a.getCentreY());
            }
            line.lineTo(px, py);
            fill.lineTo(px, py);
        }
        fill.lineTo(a.getRight(), a.getCentreY());
        fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(kRight.withAlpha(0.20f), 0, a.getY(), kLeft.withAlpha(0.20f), 0, a.getBottom(), false));
        g.fillPath(fill);
        strokeNeon(g, line, kAccent, 2.2f, glow());

        // Bend handles (diamonds) in the middle of straight / bent segments.
        const auto& pts = c.points();
        for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
            if (pts[static_cast<std::size_t>(i)].type != CurvePoint::Bend) continue;
            const auto h = bendHandle(i);
            juce::Path d;
            d.addRectangle(-3.5f, -3.5f, 7.0f, 7.0f);
            d.applyTransform(juce::AffineTransform::rotation(0.785398f).translated(h));
            g.setColour(i == hoverBend_ || i == bendDrag_ ? colours::text : kAccent.withAlpha(0.55f));
            g.fillPath(d);
        }
        // Points.
        for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
            const auto p = toScreen(pts[static_cast<std::size_t>(i)]);
            const bool active = i == hover_ || i == drag_;
            const float rad = active ? 6.5f : 5.0f;
            if (glow() && active) {
                g.setColour(kAccent.withAlpha(0.25f));
                g.fillEllipse(juce::Rectangle<float>(rad * 4, rad * 4).withCentre(p));
            }
            g.setColour(juce::Colour(0xff07090f));
            g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(p));
            g.setColour(pts[static_cast<std::size_t>(i)].type == CurvePoint::Hold ? colours::text : kAccent);
            g.drawEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(p), 2.0f);
        }
        // Playhead.
        if (showHead_) {
            const float x = a.getX() + static_cast<float>(phase_) * a.getWidth();
            g.setColour(colours::text.withAlpha(0.35f));
            g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
            const auto dot = juce::Point<float>(x, toPy(value_));
            const auto col = value_ < 0 ? kLeft.interpolatedWith(kAccent, value_ + 1.0f) : kAccent.interpolatedWith(kRight, value_);
            if (glow()) {
                g.setColour(col.withAlpha(0.3f));
                g.fillEllipse(juce::Rectangle<float>(22, 22).withCentre(dot));
            }
            g.setColour(col);
            g.fillEllipse(juce::Rectangle<float>(10, 10).withCentre(dot));
        }
    }

private:
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced(10.0f, 12.0f).withTrimmedLeft(30.0f).withTrimmedBottom(8.0f); }
    float toX(float px) const { return juce::jlimit(0.0f, 0.999f, (px - area().getX()) / area().getWidth()); }
    float toY(float py) const { return juce::jlimit(-1.0f, 1.0f, 1.0f - 2.0f * (py - area().getY()) / area().getHeight()); }
    float toPy(float y) const { return area().getY() + (1.0f - y) * 0.5f * area().getHeight(); }
    juce::Point<float> toScreen(const CurvePoint& p) const { return {area().getX() + p.x * area().getWidth(), toPy(p.y)}; }
    float snapX(float x, const juce::ModifierKeys& m) const {
        if (!snap || m.isAltDown()) return x;  // Alt: place freely
        return juce::jlimit(0.0f, 0.999f, std::round(x * grid) / grid);
    }
    juce::Point<float> bendHandle(int i) const {
        const auto& pts = proc_.curve().points();
        const auto& a = pts[static_cast<std::size_t>(i)];
        const float nx = i + 1 < static_cast<int>(pts.size()) ? pts[static_cast<std::size_t>(i) + 1].x : 1.0f;
        const float mx = 0.5f * (a.x + nx);
        return {area().getX() + mx * area().getWidth(), toPy(proc_.curve().valueAt(mx))};
    }
    int hitPoint(juce::Point<float> pos) const {
        const auto& pts = proc_.curve().points();
        for (int i = static_cast<int>(pts.size()) - 1; i >= 0; --i)
            if (toScreen(pts[static_cast<std::size_t>(i)]).getDistanceFrom(pos) < 9.0f) return i;
        return -1;
    }
    int hitBend(juce::Point<float> pos) const {
        const auto& pts = proc_.curve().points();
        for (int i = 0; i < static_cast<int>(pts.size()); ++i)
            if (pts[static_cast<std::size_t>(i)].type == CurvePoint::Bend && bendHandle(i).getDistanceFrom(pos) < 7.0f) return i;
        return -1;
    }
    void showPointMenu(int i) {
        const int type = proc_.curve().points()[static_cast<std::size_t>(i)].type;
        juce::PopupMenu m;
        m.addItem(1, "Bend (straight / curved)", true, type == CurvePoint::Bend);
        m.addItem(2, "Smooth", true, type == CurvePoint::Smooth);
        m.addItem(3, "Hold (step)", true, type == CurvePoint::Hold);
        m.addSeparator();
        m.addItem(4, "Delete point", i > 0);
        juce::Component::SafePointer<CurveEditor> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe, i](int r) {
            if (!safe || r == 0) return;
            auto& c = safe->proc_.curve();
            if (r == 4) c.remove(i);
            else c.setType(i, r - 1);
            safe->proc_.curveChanged();
            safe->repaint();
        });
    }

    PatanProcessor& proc_;
    int mode_ = 0, drag_ = -1, bendDrag_ = -1, hover_ = -1, hoverBend_ = -1, version_ = -1;
    float bendStart_ = 0;
    double phase_ = 0;
    float value_ = 0;
    bool showHead_ = false;
};

// ------------------------------------------------------------------------------------------- stereo view
// Where the sound is right now: a neon ball on the L-R line (pan), a level bar (volume) or an
// opening arc (width), with output meters for each side.
class PatanEditor::StereoView final : public juce::Component {
public:
    explicit StereoView(PerfSettings& s) : settings_(s) { setInterceptsMouseClicks(false, false); }
    void update(float pos, int mode, float pl, float pr, float dt) {
        const float k = settings_.neonFixed() ? 1.0f : juce::jmin(1.0f, dt * 20.0f);
        shown_ += k * (pos - shown_);
        meterL_ = juce::jmax(pl, meterL_ * (settings_.neonFixed() ? 0.0f : std::exp(-dt * 6.0f)));
        meterR_ = juce::jmax(pr, meterR_ * (settings_.neonFixed() ? 0.0f : std::exp(-dt * 6.0f)));
        mode_ = mode;
        repaint();
    }
    void paint(juce::Graphics& g) override {
        drawPanel(g, getLocalBounds(), mode_ == 1 ? "LEVEL" : mode_ == 2 ? "WIDTH" : "POSITION", kAccent);
        auto r = getLocalBounds().toFloat().reduced(16.0f, 0.0f).withTrimmedTop(34.0f).withTrimmedBottom(12.0f);
        const bool glow = !settings_.neonFixed() && settings_.profile().glow;
        const float cy = r.getY() + 22.0f;
        const auto col = shown_ < 0 ? kLeft.interpolatedWith(kAccent, shown_ + 1.0f) : kAccent.interpolatedWith(kRight, shown_);
        g.setColour(colours::line);
        g.fillRoundedRectangle(r.getX(), cy - 2.0f, r.getWidth(), 4.0f, 2.0f);
        g.setFont(font(11.0f, true));
        if (mode_ == 2) {  // width: an arc that opens
            const float w = juce::jlimit(0.0f, 1.0f, 0.5f + 0.5f * shown_) * r.getWidth() * 0.5f;
            juce::Path p;
            p.startNewSubPath(r.getCentreX() - w, cy);
            p.quadraticTo(r.getCentreX(), cy - 20.0f, r.getCentreX() + w, cy);
            strokeNeon(g, p, kAccent, 3.0f, glow);
        } else {
            const float x = mode_ == 1 ? r.getX() + (0.5f + 0.5f * shown_) * r.getWidth() : r.getCentreX() + shown_ * r.getWidth() * 0.5f;
            if (mode_ == 1) {
                g.setColour(kAccent.withAlpha(0.7f));
                g.fillRoundedRectangle(r.getX(), cy - 2.0f, x - r.getX(), 4.0f, 2.0f);
            }
            if (glow) {
                g.setColour(col.withAlpha(0.25f));
                g.fillEllipse(juce::Rectangle<float>(34, 34).withCentre({x, cy}));
            }
            g.setColour(col);
            g.fillEllipse(juce::Rectangle<float>(16, 16).withCentre({x, cy}));
        }
        g.setColour(kLeft);
        g.drawText("L", juce::Rectangle<float>(r.getX(), cy + 8.0f, 20.0f, 14.0f), juce::Justification::centredLeft);
        g.setColour(kRight);
        g.drawText("R", juce::Rectangle<float>(r.getRight() - 20.0f, cy + 8.0f, 20.0f, 14.0f), juce::Justification::centredRight);
        // Output meters, one per side.
        const auto meter = [&](float v, float y, juce::Colour c) {
            const float db = juce::Decibels::gainToDecibels(v, -60.0f);
            const float w = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f) * r.getWidth();
            g.setColour(colours::raised);
            g.fillRoundedRectangle(r.getX(), y, r.getWidth(), 5.0f, 2.5f);
            g.setColour(db > -0.1f ? colours::hot : c);
            g.fillRoundedRectangle(r.getX(), y, w, 5.0f, 2.5f);
        };
        meter(meterL_, r.getBottom() - 16.0f, kLeft);
        meter(meterR_, r.getBottom() - 7.0f, kRight);
    }

private:
    PerfSettings& settings_;
    float shown_ = 0, meterL_ = 0, meterR_ = 0;
    int mode_ = 0;
};

// ------------------------------------------------------------------------------------------- editor
PatanEditor::PatanEditor(PatanProcessor& p)
    : PluginFrame(p, p.state, "Patan", "auto panner", kAccent, 1000, 620),
      proc_(p),
      mode_(p.state, kMode, {"PAN", "VOLUME", "WIDTH"}, kAccent),
      law_(p.state, kLaw, {"BAL", "-3", "-4.5", "-6"}, kAccent),
      division_(p.state, kDivision, "RATE", kAccent, settings(), NeonKnob::Size::Big),
      rate_(p.state, kRate, "RATE", kAccent, settings(), NeonKnob::Size::Big),
      depth_(p.state, kDepth, "DEPTH", kAccent, settings(), NeonKnob::Size::Big),
      phase_(p.state, kPhase, "PHASE", kAccent, settings()),
      smooth_(p.state, kSmooth, "SMOOTH", kAccent, settings()),
      center_(p.state, kCenter, "CENTER", kAccent, settings()),
      width_(p.state, kWidth, "WIDTH", kAccent, settings()),
      mix_(p.state, kMix, "MIX", kAccent, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      syncAttachment_(p.state, kSync, sync_),
      invertAttachment_(p.state, kInvert, invert_) {
    curve_ = std::make_unique<CurveEditor>(p, settings());
    stereo_ = std::make_unique<StereoView>(settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{curve_.get(), stereo_.get(), &mode_, &law_, &division_, &rate_, &depth_, &phase_,
                                                                       &smooth_, &center_, &width_, &mix_, &output_, &sync_, &invert_, &snap_, &grid_,
                                                                       &modeLabel_, &lawLabel_, &hint_})
        content().addAndMakeVisible(c);

    for (int s = 0; s < PanCurve::NumShapes; ++s) {
        static const char* shortNames[] = {"Sine", "Tri", "Square", "Saw Up", "Saw Dn", "Steps 4", "Steps 8", "Bounce", "Gate", "Ramp", "Random"};
        auto* b = shapes_.add(new juce::TextButton(shortNames[s]));
        b->setTooltip(juce::String("Replace the curve with: ") + PanCurve::shapeName(s));
        b->onClick = [this, s] {
            proc_.curve().setPoints(PanCurve::shape(s, static_cast<unsigned>(juce::Random::getSystemRandom().nextInt(1 << 20))));
            proc_.curveChanged();
        };
        content().addAndMakeVisible(b);
    }
    grid_.addItem("Grid 4", 4);
    grid_.addItem("Grid 8", 8);
    grid_.addItem("Grid 16", 16);
    grid_.addItem("Grid 32", 32);
    grid_.setSelectedId(8, juce::dontSendNotification);
    grid_.setTooltip("Grid lines (and snapping) across one cycle");
    grid_.onChange = [this] {
        curve_->grid = grid_.getSelectedId();
        curve_->invalidateStatic();
        curve_->repaint();
    };
    snap_.setToggleState(true, juce::dontSendNotification);
    snap_.setTooltip("New and moved points snap to the grid (hold Alt to place freely)");
    snap_.onClick = [this] { curve_->snap = snap_.getToggleState(); };
    for (auto* t : {&sync_, &invert_, &snap_}) t->setColour(juce::ToggleButton::tickColourId, kAccent);
    sync_.setTooltip("On: the cycle is a note length locked to the song. Off: free rate in Hz.");
    invert_.setTooltip("Mirror the curve: left becomes right");
    for (auto* l : {&modeLabel_, &lawLabel_, &hint_}) {
        l->setFont(font(11.5f, true));
        l->setColour(juce::Label::textColourId, colours::dim);
    }
    modeLabel_.setText("MODE", juce::dontSendNotification);
    lawLabel_.setText("PAN LAW", juce::dontSendNotification);
    hint_.setFont(font(12.0f));
    hint_.setJustificationType(juce::Justification::centred);
    hint_.setText("Click to add points, drag to move, double-click to delete. Drag a diamond to bend a segment; right-click a point for steps.",
                  juce::dontSendNotification);
    division_.setTooltip("Length of one cycle, locked to the song");
    rate_.setTooltip("Cycles per second");
    depth_.setTooltip("How far the movement goes");

    clock().add([this](float dt) { tick(dt); });
    setPresets(*p.presets);
    finishConstruction();
}

PatanEditor::~PatanEditor() = default;

float PatanEditor::param(const char* id) const { return proc_.state.getRawParameterValue(id)->load(); }

void PatanEditor::tick(float dt) {
    const bool sync = param(kSync) > 0.5f;
    if (division_.isVisible() != sync) {
        division_.setVisible(sync);
        rate_.setVisible(!sync);
    }
    const int mode = juce::roundToInt(param(kMode));
    curve_->setMode(mode);
    center_.setEnabled(mode == 0);
    law_.setEnabled(mode == 0);
    if (simulating_) return;
    // Playhead follows the cycle; the dot shows the raw curve position at that point.
    const double ph = std::fmod(proc_.phase.load() + param(kPhase) / 360.0, 1.0);
    curve_->setPlayhead(ph, proc_.curve().valueAt(static_cast<float>(ph)) * (param(kInvert) > 0.5f ? -1.0f : 1.0f), true);
    stereo_->update(proc_.position.load() * param(kDepth) + (mode == 0 ? param(kCenter) : 0.0f), mode, proc_.peakL.load(), proc_.peakR.load(), dt);
}

void PatanEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&phase_, &smooth_, &center_, &width_, &mix_, &output_, &law_, &lawLabel_, &invert_})
        c->setVisible(pro);
    hint_.setVisible(!pro);
}

void PatanEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(16, 12);
    auto right = r.removeFromRight(252);
    r.removeFromRight(16);
    const bool pro = proView();
    curve_->setBounds(r.removeFromTop(pro ? 330 : 420));
    r.removeFromTop(8);
    auto row = r.removeFromTop(26);
    snap_.setBounds(row.removeFromRight(64));
    grid_.setBounds(row.removeFromRight(84).reduced(2, 0));
    row.removeFromRight(6);
    const int bw = row.getWidth() / shapes_.size();
    for (auto* b : shapes_) b->setBounds(row.removeFromLeft(bw).reduced(1, 0));
    r.removeFromTop(10);
    if (pro) {
        auto knobs = r.removeFromTop(118);
        const int kw = knobs.getWidth() / 6;
        for (auto* k : {&phase_, &smooth_, &center_, &width_, &mix_, &output_}) k->setBounds(knobs.removeFromLeft(kw).reduced(2, 4));
    } else {
        hint_.setBounds(r.removeFromTop(24));
    }

    stereo_->setBounds(right.removeFromTop(118));
    right.removeFromTop(10);
    modeLabel_.setBounds(right.removeFromTop(18));
    mode_.setBounds(right.removeFromTop(30));
    right.removeFromTop(8);
    sync_.setBounds(right.removeFromTop(24));
    const int kh = pro ? 128 : 150;
    auto rateArea = right.removeFromTop(kh);
    division_.setBounds(rateArea);
    rate_.setBounds(rateArea);
    depth_.setBounds(right.removeFromTop(kh));
    if (pro) {
        right.removeFromTop(4);
        lawLabel_.setBounds(right.removeFromTop(18));
        law_.setBounds(right.removeFromTop(28));
        right.removeFromTop(6);
        invert_.setBounds(right.removeFromTop(24));
    }
}

void PatanEditor::simulateForSnapshot(float seconds) {
    simulating_ = true;
    const double ph = std::fmod(0.31 + seconds * 0.01, 1.0);
    const float v = proc_.curve().valueAt(static_cast<float>(ph));
    curve_->setPlayhead(ph, v, true);
    stereo_->update(v, juce::roundToInt(param(kMode)), 0.5f, 0.3f, 1.0f);
    simulating_ = false;
}
