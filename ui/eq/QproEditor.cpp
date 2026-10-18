#include "ui/eq/QproEditor.h"

#include "common/ui/KeyboardFocus.h"

using namespace pat::ui;
using namespace qpro;
using pat::dsp::EqBand;
using pat::dsp::EqDesign;
using pat::dsp::SpectrumAnalyzer;

namespace {
const juce::Colour kAccent = colours::ice;
const juce::Colour kPost = colours::ice;
const juce::Colour kSc = colours::orange;
const juce::Colour kCompare = colours::violet;
constexpr float kMinHz = 10.0f, kMaxHz = 30000.0f;
constexpr int kPoints = 360;

float pointHz(int k) { return kMinHz * std::pow(kMaxHz / kMinHz, static_cast<float>(k) / (kPoints - 1)); }

bool sameBand(const EqBand& a, const EqBand& b) {
    return a.used == b.used && a.active == b.active && a.type == b.type && a.freq == b.freq && a.gain == b.gain && a.q == b.q && a.slope == b.slope &&
           a.place == b.place && a.dynamic == b.dynamic && a.dynRange == b.dynRange;
}

juce::String hzLabel(float hz) {
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz" : juce::String(hz, hz < 100.0f ? 1 : 0) + " Hz";
}
}  // namespace

juce::Colour QproEditor::bandColour(int band) {
    return juce::Colour::fromHSV(std::fmod(0.52f + static_cast<float>(band) * 0.618034f, 1.0f), 0.72f, 1.0f, 1.0f);
}

juce::String QproEditor::noteName(float hz) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const float midi = 69.0f + 12.0f * std::log2(std::max(hz, 1.0f) / 440.0f);
    const int n = juce::roundToInt(midi);
    const int cents = juce::roundToInt((midi - static_cast<float>(n)) * 100.0f);
    const int idx = ((n % 12) + 12) % 12;
    return juce::String(names[idx]) + juce::String(n / 12 - 1) + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String(cents) + "c");
}

// ------------------------------------------------------------------------------------------- display
class QproEditor::Display final : public LayeredCanvas {
public:
    Display(QproEditor& ed, QproProcessor& p, PerfSettings& s) : LayeredCanvas(s), ed_(ed), proc_(p) {}

    float rangeDb = 12.0f;
    bool showPre = true, showPost = true, showSc = true, piano = false;
    int compareId = -1;  // -1 off, 0 sidechain, > 0 another PatQpro
    bool testSignal = false;

    void setRange(float r) {
        rangeDb = r;
        invalidateStatic();
        repaint();
    }
    void setPiano(bool on) {
        piano = on;
        invalidateStatic();
        repaint();
    }

    // Once per frame: refresh curves that changed and repaint.
    void refresh() {
        const auto g = proc_.global();
        const int mode = g.mode == pat::dsp::EqGlobal::Natural && proc_.sampleRate() >= 80000.0 ? 0 : g.mode;
        bool changed = mode != mode_ || g.gainScale != scale_ || proc_.sampleRate() != fs_;
        mode_ = mode;
        scale_ = g.gainScale;
        fs_ = proc_.sampleRate();
        for (int i = 0; i < kBands; ++i) {
            const auto b = proc_.band(i);
            if (changed || !sameBand(b, bands_[static_cast<std::size_t>(i)])) {
                bands_[static_cast<std::size_t>(i)] = b;
                computeCurve(i, b.gain * scale_, curve_[static_cast<std::size_t>(i)]);
                changed = true;
            }
            // Dynamic bands: where the gain is right now.
            const float dyn = b.used && b.active && b.dynamic ? proc_.dynamicGainDb(i) : 0.0f;
            if (std::abs(dyn - dyn_[static_cast<std::size_t>(i)]) > 0.02f || (changed && b.dynamic)) {
                dyn_[static_cast<std::size_t>(i)] = dyn;
                computeCurve(i, b.gain * scale_ + dyn, dynCurve_[static_cast<std::size_t>(i)]);
                changed = true;
            }
        }
        if (changed) {
            totalA_.fill(0.0f);
            totalB_.fill(0.0f);
            split_ = false;
            for (int i = 0; i < kBands; ++i) {
                const auto& b = bands_[static_cast<std::size_t>(i)];
                if (!b.used || !b.active) continue;
                const auto& c = b.dynamic ? dynCurve_[static_cast<std::size_t>(i)] : curve_[static_cast<std::size_t>(i)];
                const bool a = b.place != EqBand::Right && b.place != EqBand::Side;
                const bool bb = b.place != EqBand::Left && b.place != EqBand::Mid;
                split_ |= b.place != EqBand::Stereo;
                for (int k = 0; k < kPoints; ++k) {
                    if (a) totalA_[static_cast<std::size_t>(k)] += c[static_cast<std::size_t>(k)];
                    if (bb) totalB_[static_cast<std::size_t>(k)] += c[static_cast<std::size_t>(k)];
                }
            }
        }
        repaint();
    }

    void mouseMove(const juce::MouseEvent& e) override {
        mouse_ = e.position;
        const int h = hit(e.position);
        if (h != hover_) {
            hover_ = h;
            setMouseCursor(h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
        }
    }
    void mouseExit(const juce::MouseEvent&) override {
        hover_ = -1;
        mouse_ = {-100, -100};
    }

    void mouseDown(const juce::MouseEvent& e) override {
        mouse_ = e.position;
        const int h = hit(e.position);
        pendingCreate_ = false;
        if (e.mods.isPopupMenu()) {
            if (h >= 0) {
                ed_.select(h);
                bandMenu(h);
            }
            return;
        }
        if (h < 0) {
            pendingCreate_ = true;
            createAt_ = e.position;
            return;
        }
        const bool multi = e.mods.isCtrlDown() || e.mods.isShiftDown() || e.mods.isCommandDown();
        if (multi) ed_.select(h, true);
        else if (!ed_.selection().count(h)) ed_.select(h);
        else ed_.selectionKeep(h);  // keep the group, drag them all
        startDrag(e.position);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        mouse_ = e.position;
        if (pendingCreate_) {
            if (e.position.getDistanceFrom(createAt_) < 4.0f) return;
            pendingCreate_ = false;
            // Spectrum grab: the new band lands on the nearest peak of what is playing.
            const int b = createBand(createAt_, true);
            if (b < 0) return;
            startDrag(nodePos(b));
            dragOrigin_ = createAt_;
        }
        if (dragging_.empty()) return;
        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const auto a = area();
        const float dx = (e.position.x - dragOrigin_.x) / a.getWidth() * fine;
        const float dDb = (dbForY(e.position.y) - dbForY(dragOrigin_.y)) * fine;
        const float ratio = std::pow(kMaxHz / kMinHz, dx);
        const float scale = std::abs(scale_) > 0.05f ? scale_ : 1.0f;
        for (const auto& d : dragging_) {
            if (auto* p = proc_.state.getParameter(bandId(d.band, Freq))) p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(kMinHz, kMaxHz, d.freq * ratio)));
            if (EqBand::hasGain(bands_[static_cast<std::size_t>(d.band)].type))
                if (auto* p = proc_.state.getParameter(bandId(d.band, Gain))) p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(-30.0f, 30.0f, d.gain + dDb / scale)));
        }
    }

    void mouseUp(const juce::MouseEvent& e) override {
        if (pendingCreate_ && !e.mods.isPopupMenu()) ed_.select(-1);  // click on empty space
        pendingCreate_ = false;
        for (const auto& d : dragging_)
            for (int f : {Freq, Gain})
                if (auto* p = proc_.state.getParameter(bandId(d.band, f))) p->endChangeGesture();
        dragging_.clear();
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        const int h = hit(e.position);
        pendingCreate_ = false;
        if (h >= 0) {  // bypass / enable
            proc_.undo.beginNewTransaction("Bypass band");
            proc_.setParam(bandId(h, Active), bands_[static_cast<std::size_t>(h)].active ? 0.0f : 1.0f);
            return;
        }
        createBand(e.position, false);
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        int h = hit(e.position);
        if (h < 0) h = ed_.selected();
        if (h < 0) return;
        const float q = bands_[static_cast<std::size_t>(h)].q * std::pow(1.15f, (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.3f : 1.0f));
        proc_.setParam(bandId(h, Q), juce::jlimit(0.025f, 40.0f, q));
    }

    // Diagnostic snapshots: a believable mix spectrum and a sidechain.
    void makeTestSpectra(QproProcessor::Spectrum& pre, QproProcessor::Spectrum& post, QproProcessor::Spectrum& sc) const {
        for (int i = 0; i < SpectrumAnalyzer::kBins; ++i) {
            const float hz = SpectrumAnalyzer::binHz(i);
            const float base = -28.0f - 4.5f * std::log2(hz / 1000.0f) - 9.0f * std::exp(-std::pow(std::log2(hz / 60.0f), 2.0f)) * -1.0f;
            const float ripple = 3.0f * std::sin(static_cast<float>(i) * 0.9f) + 2.0f * std::sin(static_cast<float>(i) * 2.3f);
            const float peaks = 10.0f * std::exp(-std::pow(std::log2(hz / 220.0f) * 6.0f, 2.0f)) + 7.0f * std::exp(-std::pow(std::log2(hz / 3100.0f) * 5.0f, 2.0f));
            pre[static_cast<std::size_t>(i)] = base + ripple + peaks - (hz > 16000.0f ? (hz - 16000.0f) / 300.0f : 0.0f);
            double eq = 0;
            for (int b = 0; b < kBands; ++b)
                if (bands_[static_cast<std::size_t>(b)].used && bands_[static_cast<std::size_t>(b)].active)
                    eq += 20.0 * std::log10(std::max(1e-6, EqDesign::idealMagnitude(bands_[static_cast<std::size_t>(b)], bands_[static_cast<std::size_t>(b)].gain, hz)));
            post[static_cast<std::size_t>(i)] = pre[static_cast<std::size_t>(i)] + static_cast<float>(eq);
            sc[static_cast<std::size_t>(i)] = -30.0f - 6.0f * std::log2(hz / 1000.0f) + 12.0f * std::exp(-std::pow(std::log2(hz / 70.0f) * 2.0f, 2.0f)) -
                                              12.0f * std::log2(std::max(1.0f, hz / 400.0f));
        }
    }

protected:
    void paintStatic(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff06080d));
        g.fillRoundedRectangle(r, 6.0f);
        const auto a = area();
        g.setFont(font(10.0f));
        // Frequency grid.
        for (float hz : {20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 80.0f, 100.0f, 200.0f, 300.0f, 400.0f, 500.0f, 600.0f, 800.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f,
                         5000.0f, 6000.0f, 8000.0f, 10000.0f, 20000.0f}) {
            const bool major = hz == 20.0f || hz == 50.0f || hz == 100.0f || hz == 200.0f || hz == 500.0f || hz == 1000.0f || hz == 2000.0f || hz == 5000.0f ||
                               hz == 10000.0f || hz == 20000.0f;
            const float x = xForHz(hz);
            g.setColour(major ? colours::line.withAlpha(0.9f) : colours::grid);
            g.drawVerticalLine(juce::roundToInt(x), a.getY(), a.getBottom());
            if (major) {
                g.setColour(colours::faint);
                g.drawText(hz >= 1000.0f ? juce::String(juce::roundToInt(hz / 1000.0f)) + "k" : juce::String(juce::roundToInt(hz)),
                           juce::Rectangle<float>(x - 20.0f, a.getBottom() + (piano ? 15.0f : 2.0f), 40.0f, 12.0f), juce::Justification::centred);
            }
        }
        // EQ gain grid (left labels) and analyzer level (right labels).
        const float step = rangeDb <= 3.0f ? 1.0f : rangeDb <= 6.0f ? 2.0f : rangeDb <= 12.0f ? 3.0f : 10.0f;
        for (float d = -rangeDb; d <= rangeDb + 0.01f; d += step) {
            const float y = yForDb(d);
            g.setColour(std::abs(d) < 0.01f ? colours::line.brighter(0.3f) : colours::grid);
            g.drawHorizontalLine(juce::roundToInt(y), a.getX(), a.getRight());
            g.setColour(std::abs(d) < 0.01f ? colours::dim : colours::faint);
            g.drawText((d > 0 ? "+" : "") + juce::String(juce::roundToInt(d)), juce::Rectangle<float>(0, y - 6.0f, a.getX() - 4.0f, 12.0f),
                       juce::Justification::centredRight);
        }
        g.setColour(colours::faint.withAlpha(0.7f));
        for (int d = 0; d >= -84; d -= 12)
            g.drawText(juce::String(d), juce::Rectangle<float>(a.getRight() + 4.0f, ySpec(static_cast<float>(d), 1000.0f) - 6.0f, 30.0f, 12.0f),
                       juce::Justification::centredLeft);
        if (piano) {  // keyboard strip under the display
            const float top = a.getBottom() + 1.0f;
            for (int note = 12; note < 128; ++note) {
                const float lo = 440.0f * std::pow(2.0f, (note - 69.5f) / 12.0f), hi = 440.0f * std::pow(2.0f, (note - 68.5f) / 12.0f);
                const float x0 = xForHz(lo), x1 = xForHz(hi);
                if (x1 < a.getX() || x0 > a.getRight()) continue;
                const int k = note % 12;
                const bool black = k == 1 || k == 3 || k == 6 || k == 8 || k == 10;
                g.setColour(black ? juce::Colour(0xff10131b) : juce::Colour(0xffb9bfcc));
                g.fillRect(juce::Rectangle<float>(x0, top, std::max(1.0f, x1 - x0 - 0.5f), 12.0f).getIntersection(a.withHeight(200.0f).withY(top)));
                if (k == 0 && x1 - x0 > 2.0f) {
                    g.setColour(colours::background);
                    g.setFont(font(8.0f, true));
                    g.drawText("C" + juce::String(note / 12 - 1), juce::Rectangle<float>(x0, top, 18.0f, 12.0f), juce::Justification::centredLeft);
                }
            }
        }
        g.setColour(colours::line);
        g.drawRoundedRectangle(r.reduced(0.5f), 6.0f, 1.0f);
    }

    void paintDynamic(juce::Graphics& g) override {
        const auto a = area();
        g.saveState();
        g.reduceClipRegion(a.toNearestInt());
        QproProcessor::Spectrum tp{}, tq{}, ts{};
        if (testSignal) makeTestSpectra(tp, tq, ts);
        const auto& pre = testSignal ? tp : proc_.preSpectrum();
        const auto& post = testSignal ? tq : proc_.postSpectrum();
        const auto& sc = testSignal ? ts : proc_.scSpectrum();
        const bool scOn = testSignal || proc_.sidechainActive();

        if (showPre) drawSpectrum(g, pre, colours::dim.withAlpha(0.22f), juce::Colours::transparentBlack, 0.0f);
        if (showPost) drawSpectrum(g, post, kPost.withAlpha(0.14f), kPost.withAlpha(0.55f), 1.2f);
        if (showSc && scOn) drawSpectrum(g, sc, juce::Colours::transparentBlack, kSc.withAlpha(0.85f), 1.3f);

        // Compare + collisions.
        const QproProcessor::Spectrum* other = compareId == 0 ? (scOn ? &sc : nullptr) : compareId > 0 ? proc_.instanceSpectrum(compareId) : nullptr;
        if (testSignal && compareId != -1) other = &ts;
        if (other) {
            if (compareId > 0) drawSpectrum(g, *other, juce::Colours::transparentBlack, kCompare.withAlpha(0.9f), 1.3f);
            float clash[SpectrumAnalyzer::kBins];
            pat::dsp::Collision::compute(post.data(), other->data(), clash);
            for (int i = 0; i < SpectrumAnalyzer::kBins - 1; ++i) {
                if (clash[i] < 0.05f) continue;
                const float x0 = xForHz(SpectrumAnalyzer::binHz(i)), x1 = xForHz(SpectrumAnalyzer::binHz(i + 1));
                const float h = a.getHeight() * 0.22f * clash[i];
                g.setColour(colours::hot.withAlpha(0.25f + 0.5f * clash[i]));
                g.fillRect(juce::Rectangle<float>(x0, a.getBottom() - h, x1 - x0 + 0.6f, h));
            }
        }

        // Band curves.
        const int sel = ed_.selected();
        for (int i = 0; i < kBands; ++i) {
            const auto& b = bands_[static_cast<std::size_t>(i)];
            if (!b.used) continue;
            const auto col = bandColour(i).withAlpha(b.active ? 1.0f : 0.35f);
            juce::Path p = curvePath(curve_[static_cast<std::size_t>(i)]);
            juce::Path fill = p;
            fill.lineTo(a.getRight(), yForDb(0));
            fill.lineTo(a.getX(), yForDb(0));
            fill.closeSubPath();
            g.setColour(col.withAlpha(i == sel ? 0.20f : 0.08f));
            g.fillPath(fill);
            if (i == sel || ed_.selection().count(i)) {
                g.setColour(col.withAlpha(0.7f));
                g.strokePath(p, juce::PathStrokeType(1.2f));
            }
            if (b.dynamic && b.active) {  // how far the dynamic band moves right now
                juce::Path d = curvePath(dynCurve_[static_cast<std::size_t>(i)]);
                g.setColour(col.withAlpha(0.9f));
                g.strokePath(d, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved), juce::AffineTransform());
                const float x = xForHz(b.freq);
                const float y0 = yForDb(b.gain * scale_), y1 = yForDb(juce::jlimit(-rangeDb, rangeDb, (b.gain + b.dynRange) * scale_));
                g.setColour(col.withAlpha(0.5f));
                g.drawLine(x, y0, x, y1, 3.0f);
            }
        }
        // Total curve(s).
        if (split_) {
            juce::Path pb = curvePath(totalB_);
            g.setColour(colours::text.withAlpha(0.45f));
            const float dashes[] = {5.0f, 4.0f};
            juce::Path dashed;
            juce::PathStrokeType(1.5f).createDashedStroke(dashed, pb, dashes, 2);
            g.fillPath(dashed);
        }
        strokeNeon(g, curvePath(totalA_), colours::text.interpolatedWith(kAccent, 0.35f), 2.0f, glow());
        g.restoreState();

        // Nodes.
        for (int i = 0; i < kBands; ++i) {
            const auto& b = bands_[static_cast<std::size_t>(i)];
            if (!b.used) continue;
            const auto p = nodePos(i);
            const bool selected = ed_.selection().count(i) > 0;
            const auto col = bandColour(i);
            const float rad = selected || i == hover_ ? 9.0f : 7.5f;
            if (glow() && (selected || i == hover_)) {
                g.setColour(col.withAlpha(0.28f));
                g.fillEllipse(juce::Rectangle<float>(rad * 3.6f, rad * 3.6f).withCentre(p));
            }
            g.setColour(b.active ? col.withAlpha(selected ? 1.0f : 0.85f) : colours::raised);
            g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(p));
            g.setColour(b.active ? colours::background : col);
            if (!b.active) g.drawEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(p), 1.5f);
            g.setFont(font(10.0f, true));
            g.drawText(juce::String(i + 1), juce::Rectangle<float>(rad * 2, rad * 2).withCentre(p), juce::Justification::centred);
            if (b.place != EqBand::Stereo) {
                static const char* tags[] = {"", "L", "R", "M", "S"};
                g.setColour(col);
                g.setFont(font(9.0f, true));
                g.drawText(tags[b.place], juce::Rectangle<float>(14, 10).withCentre(p.translated(0, -rad - 7.0f)), juce::Justification::centred);
            }
        }

        // Readout: the hovered node, or the frequency and note under the mouse.
        if (hover_ >= 0) {
            const auto& b = bands_[static_cast<std::size_t>(hover_)];
            juce::String t = juce::String(EqBand::typeName(b.type)) + "   " + hzLabel(b.freq) + "  (" + noteName(b.freq) + ")";
            juce::String t2 = EqBand::hasGain(b.type) ? ((b.gain > 0 ? "+" : "") + juce::String(b.gain, 1) + " dB   ") : juce::String();
            t2 << "Q " << juce::String(b.q, 2);
            if (EqBand::hasSlope(b.type)) t2 << "   " << (b.slope == EqBand::kNumSlopes - 1 ? juce::String("brickwall") : juce::String(EqBand::slopeDb(b.slope)) + " dB/oct");
            if (b.dynamic) t2 << "   dyn " << juce::String(dyn_[static_cast<std::size_t>(hover_)], 1) << " dB";
            const auto p = nodePos(hover_);
            auto box = juce::Rectangle<float>(250.0f, 38.0f).withPosition(p.x + 14.0f, p.y - 46.0f);
            if (box.getRight() > a.getRight()) box.setX(p.x - 264.0f);
            if (box.getY() < a.getY()) box.setY(p.y + 14.0f);
            g.setColour(colours::panel.withAlpha(0.92f));
            g.fillRoundedRectangle(box, 5.0f);
            g.setColour(bandColour(hover_));
            g.drawRoundedRectangle(box, 5.0f, 1.0f);
            g.setColour(colours::text);
            g.setFont(font(11.5f, true));
            g.drawText(t, box.reduced(8.0f, 3.0f).removeFromTop(16.0f), juce::Justification::centredLeft);
            g.setColour(colours::dim);
            g.setFont(font(11.0f));
            g.drawText(t2, box.reduced(8.0f, 3.0f).removeFromBottom(16.0f), juce::Justification::centredLeft);
        } else if (a.contains(mouse_)) {
            const float hz = hzForX(mouse_.x);
            g.setColour(colours::dim);
            g.setFont(font(11.0f, true));
            g.drawText(hzLabel(hz) + "   " + noteName(hz) + "   " + (dbForY(mouse_.y) > 0 ? "+" : "") + juce::String(dbForY(mouse_.y), 1) + " dB",
                       juce::Rectangle<float>(a.getX() + 8.0f, a.getY() + 6.0f, 280.0f, 14.0f), juce::Justification::centredLeft);
        }
    }

private:
    struct Drag {
        int band;
        float freq, gain;
    };

    juce::Rectangle<float> area() const {
        auto r = getLocalBounds().toFloat();
        return r.withTrimmedLeft(34.0f).withTrimmedRight(34.0f).withTrimmedTop(6.0f).withTrimmedBottom(piano ? 30.0f : 18.0f);
    }
    float xForHz(float hz) const { return area().getX() + std::log(hz / kMinHz) / std::log(kMaxHz / kMinHz) * area().getWidth(); }
    float hzForX(float x) const { return kMinHz * std::pow(kMaxHz / kMinHz, (x - area().getX()) / area().getWidth()); }
    float yForDb(float db) const { return area().getCentreY() - db / rangeDb * area().getHeight() * 0.46f; }
    float dbForY(float y) const { return (area().getCentreY() - y) / (area().getHeight() * 0.46f) * rangeDb; }
    // Analyzer: 4.5 dB/oct tilt around 1 kHz, +6 dB at the top, -84 dB at the bottom.
    float ySpec(float db, float hz) const {
        const float t = db + 4.5f * std::log2(hz / 1000.0f);
        return area().getY() + (6.0f - t) / 90.0f * area().getHeight();
    }

    void computeCurve(int i, float gainDb, std::array<float, kPoints>& out) const {
        const auto& b = bands_[static_cast<std::size_t>(i)];
        if (!b.used || !b.active) {
            out.fill(0.0f);
            return;
        }
        if (mode_ == pat::dsp::EqGlobal::Linear) {
            for (int k = 0; k < kPoints; ++k) out[static_cast<std::size_t>(k)] = static_cast<float>(20.0 * std::log10(std::max(1e-6, EqDesign::idealMagnitude(b, gainDb, pointHz(k)))));
            return;
        }
        const double fs = mode_ == pat::dsp::EqGlobal::Natural ? 2.0 * fs_ : fs_;
        pat::dsp::Biquad s[EqDesign::kMaxSections];
        const int n = EqDesign::digital(b, gainDb, fs, s);
        for (int k = 0; k < kPoints; ++k) {
            const double hz = pointHz(k);
            out[static_cast<std::size_t>(k)] = hz >= fs * 0.5 ? out[static_cast<std::size_t>(std::max(0, k - 1))]
                                                              : static_cast<float>(20.0 * std::log10(std::max(1e-6, std::abs(EqDesign::digitalResponse(s, n, hz, fs)))));
        }
    }

    juce::Path curvePath(const std::array<float, kPoints>& c) const {
        juce::Path p;
        const float lo = yForDb(-rangeDb * 1.15f), hi = yForDb(rangeDb * 1.15f);
        for (int k = 0; k < kPoints; ++k) {
            const float x = xForHz(pointHz(k));
            const float y = juce::jlimit(hi, lo, yForDb(c[static_cast<std::size_t>(k)]));
            if (k == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        return p;
    }

    void drawSpectrum(juce::Graphics& g, const QproProcessor::Spectrum& s, juce::Colour fill, juce::Colour line, float width) const {
        const auto a = area();
        juce::Path p;
        for (int i = 0; i < SpectrumAnalyzer::kBins; ++i) {
            const float hz = SpectrumAnalyzer::binHz(i);
            const float x = xForHz(hz), y = juce::jlimit(a.getY(), a.getBottom(), ySpec(s[static_cast<std::size_t>(i)], hz));
            if (i == 0) p.startNewSubPath(x, y);
            else p.lineTo(x, y);
        }
        if (!fill.isTransparent()) {
            juce::Path f = p;
            f.lineTo(xForHz(SpectrumAnalyzer::binHz(SpectrumAnalyzer::kBins - 1)), a.getBottom());
            f.lineTo(xForHz(SpectrumAnalyzer::binHz(0)), a.getBottom());
            f.closeSubPath();
            g.setColour(fill);
            g.fillPath(f);
        }
        if (!line.isTransparent()) {
            g.setColour(line);
            g.strokePath(p, juce::PathStrokeType(width));
        }
    }

    juce::Point<float> nodePos(int i) const {
        const auto& b = bands_[static_cast<std::size_t>(i)];
        const float y = EqBand::hasGain(b.type) ? yForDb(juce::jlimit(-rangeDb, rangeDb, b.gain * scale_)) : yForDb(0.0f);
        return {xForHz(juce::jlimit(kMinHz, kMaxHz, b.freq)), y};
    }

    int hit(juce::Point<float> pos) const {
        for (int i = kBands - 1; i >= 0; --i)
            if (bands_[static_cast<std::size_t>(i)].used && nodePos(i).getDistanceFrom(pos) < 10.0f) return i;
        return -1;
    }

    void startDrag(juce::Point<float> origin) {
        dragOrigin_ = origin;
        dragging_.clear();
        proc_.undo.beginNewTransaction("Move band");
        for (int b : ed_.selection()) {
            dragging_.push_back({b, bands_[static_cast<std::size_t>(b)].freq, bands_[static_cast<std::size_t>(b)].gain});
            for (int f : {Freq, Gain})
                if (auto* p = proc_.state.getParameter(bandId(b, f))) p->beginChangeGesture();
        }
    }

    int createBand(juce::Point<float> pos, bool grab) {
        const int i = proc_.firstFreeBand();
        if (i < 0) return -1;
        float hz = juce::jlimit(kMinHz, kMaxHz, hzForX(pos.x));
        if (grab) {  // snap to the loudest spectrum point within a third of an octave
            const auto& post = proc_.postSpectrum();
            const int c = static_cast<int>(std::round(SpectrumAnalyzer::binForHz(hz)));
            const int w = static_cast<int>(SpectrumAnalyzer::kBins / 10.0f / 3.0f);
            int best = -1;
            float bestDb = -100.0f;
            for (int k = std::max(0, c - w); k <= std::min(SpectrumAnalyzer::kBins - 1, c + w); ++k)
                if (post[static_cast<std::size_t>(k)] > bestDb) {
                    bestDb = post[static_cast<std::size_t>(k)];
                    best = k;
                }
            if (best >= 0 && bestDb > -80.0f) hz = SpectrumAnalyzer::binHz(best);
        }
        EqBand b;
        b.used = true;
        b.freq = hz;
        b.type = hz < 30.0f ? EqBand::LowCut : hz > 16000.0f ? EqBand::HighCut : EqBand::Bell;
        b.gain = b.type == EqBand::Bell ? juce::jlimit(-30.0f, 30.0f, dbForY(pos.y) / (std::abs(scale_) > 0.05f ? scale_ : 1.0f)) : 0.0f;
        b.q = b.type == EqBand::Bell ? 1.0f : 0.7071f;
        proc_.undo.beginNewTransaction("Add band");
        proc_.setBand(i, b);
        bands_[static_cast<std::size_t>(i)] = b;
        ed_.select(i);
        return i;
    }

    void bandMenu(int i) {
        const auto b = bands_[static_cast<std::size_t>(i)];
        juce::PopupMenu types, slopes, places, m;
        for (int t = 0; t < EqBand::NumTypes; ++t) types.addItem(100 + t, EqBand::typeName(t), true, b.type == t);
        for (int s = 0; s < EqBand::kNumSlopes; ++s)
            slopes.addItem(200 + s, s == EqBand::kNumSlopes - 1 ? juce::String("Brickwall (linear phase)") : juce::String(EqBand::slopeDb(s)) + " dB/oct", true, b.slope == s);
        const char* placeNames[] = {"Stereo", "Left", "Right", "Mid", "Side"};
        for (int p = 0; p < 5; ++p) places.addItem(300 + p, placeNames[p], true, b.place == p);
        m.addSectionHeader("Band " + juce::String(i + 1));
        m.addSubMenu("Type", types);
        m.addSubMenu("Slope", slopes, EqBand::hasSlope(b.type));
        m.addSubMenu("Placement", places);
        m.addItem(1, "Dynamic", EqBand::hasGain(b.type), b.dynamic);
        m.addItem(2, "Bypass", true, !b.active);
        m.addItem(3, "Solo", true, proc_.solo.load() == i);
        m.addItem(4, "Invert gain", EqBand::hasGain(b.type));
        m.addSeparator();
        m.addItem(5, "Delete band");
        juce::Component::SafePointer<Display> safe(this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe, i, b](int r) {
            if (!safe || r == 0) return;
            auto& proc = safe->proc_;
            proc.undo.beginNewTransaction("Edit band");
            if (r >= 300) proc.setParam(bandId(i, Place), static_cast<float>(r - 300));
            else if (r >= 200) proc.setParam(bandId(i, Slope), static_cast<float>(r - 200));
            else if (r >= 100) proc.setParam(bandId(i, Type), static_cast<float>(r - 100));
            else if (r == 1) proc.setParam(bandId(i, Dyn), b.dynamic ? 0.0f : 1.0f);
            else if (r == 2) proc.setParam(bandId(i, Active), b.active ? 0.0f : 1.0f);
            else if (r == 3) proc.solo.store(proc.solo.load() == i ? -1 : i);
            else if (r == 4) proc.setParam(bandId(i, Gain), -b.gain);
            else if (r == 5) {
                if (proc.solo.load() == i) proc.solo.store(-1);
                proc.setBand(i, EqBand{});
                safe->ed_.select(-1);
            }
        });
    }

    QproEditor& ed_;
    QproProcessor& proc_;
    std::array<EqBand, kBands> bands_{};
    std::array<std::array<float, kPoints>, kBands> curve_{}, dynCurve_{};
    std::array<float, kBands> dyn_{};
    std::array<float, kPoints> totalA_{}, totalB_{};
    bool split_ = false;
    int mode_ = -1, hover_ = -1;
    float scale_ = 1.0f;
    double fs_ = 0;
    juce::Point<float> mouse_{-100, -100}, dragOrigin_, createAt_;
    bool pendingCreate_ = false;
    std::vector<Drag> dragging_;
};

// ------------------------------------------------------------------------------------------- match panel
class QproEditor::MatchPanel final : public juce::Component {
public:
    MatchPanel(QproEditor& ed, QproProcessor& p, PerfSettings& s)
        : ed_(ed), proc_(p), bands_("BANDS", kAccent, s, NeonKnob::Size::Small), amount_("AMOUNT", kAccent, s, NeonKnob::Size::Small) {
        for (juce::Component* c : std::initializer_list<juce::Component*>{&title_, &reference_, &learn_, &apply_, &close_, &status_, &bands_, &amount_})
            addAndMakeVisible(c);
        title_.setText("EQ MATCH", juce::dontSendNotification);
        title_.setFont(font(12.0f, true));
        title_.setColour(juce::Label::textColourId, kAccent);
        status_.setFont(font(11.0f));
        status_.setColour(juce::Label::textColourId, colours::dim);
        bands_.setRange(1.0, 24.0, 1.0);
        bands_.setValue(12.0, juce::dontSendNotification);
        bands_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v)); };
        amount_.setRange(0.0, 1.0);
        amount_.setValue(1.0, juce::dontSendNotification);
        amount_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
        learn_.setClickingTogglesState(false);
        learn_.setTooltip("Listen to the reference and to this track for a few seconds (play the loudest part)");
        apply_.setTooltip("Replace the bands with the ones that make this track sound like the reference (undo is available)");
        learn_.onClick = [this] {
            if (proc_.learning()) proc_.stopLearning();
            else proc_.startLearning(referenceId());
        };
        apply_.onClick = [this] {
            proc_.stopLearning();
            if (!proc_.applyMatch(juce::roundToInt(bands_.getValue()), static_cast<float>(amount_.getValue())))
                status_.setText("Learn first: press LEARN while the music plays.", juce::dontSendNotification);
        };
        close_.onClick = [this] {
            proc_.stopLearning();
            setVisible(false);
        };
        for (auto* b : {&learn_, &apply_, &close_}) b->setColour(juce::TextButton::buttonOnColourId, kAccent);
    }
    void refreshReferences() {
        const auto others = proc_.otherInstances();
        juce::StringArray names{"Sidechain input"};
        ids_ = {0};
        for (const auto& [id, name] : others) {
            names.add(name);
            ids_.push_back(id);
        }
        if (names != items_) {
            const int keep = referenceId();
            items_ = names;
            reference_.clear(juce::dontSendNotification);
            for (int i = 0; i < names.size(); ++i) reference_.addItem("Reference: " + names[i], i + 1);
            int sel = 1;
            for (int i = 0; i < static_cast<int>(ids_.size()); ++i)
                if (ids_[static_cast<std::size_t>(i)] == keep) sel = i + 1;
            reference_.setSelectedId(sel, juce::dontSendNotification);
        }
        learn_.setButtonText(proc_.learning() ? "STOP" : "LEARN");
        learn_.setToggleState(proc_.learning(), juce::dontSendNotification);
        if (proc_.learning()) status_.setText("Learning... " + juce::String(proc_.learnedSeconds(), 1) + " s", juce::dontSendNotification);
        else if (status_.getText().startsWith("Learning")) status_.setText("Ready: press APPLY.", juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {
        g.setColour(colours::panel.withAlpha(0.97f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.0f);
        g.setColour(kAccent.withAlpha(0.6f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8.0f, 1.0f);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(10, 8);
        auto top = r.removeFromTop(22);
        close_.setBounds(top.removeFromRight(26));
        title_.setBounds(top);
        r.removeFromTop(6);
        reference_.setBounds(r.removeFromTop(24));
        r.removeFromTop(6);
        auto row = r.removeFromTop(78);
        bands_.setBounds(row.removeFromLeft(80));
        amount_.setBounds(row.removeFromLeft(80));
        row.removeFromLeft(8);
        learn_.setBounds(row.removeFromTop(30));
        row.removeFromTop(8);
        apply_.setBounds(row.removeFromTop(30));
        status_.setBounds(r.removeFromTop(18));
    }

private:
    int referenceId() const {
        const int i = reference_.getSelectedId() - 1;
        return i >= 0 && i < static_cast<int>(ids_.size()) ? ids_[static_cast<std::size_t>(i)] : 0;
    }
    QproEditor& ed_;
    QproProcessor& proc_;
    juce::Label title_, status_;
    juce::ComboBox reference_;
    juce::TextButton learn_{"LEARN"}, apply_{"APPLY"}, close_{"X"};
    NeonKnob bands_, amount_;
    juce::StringArray items_;
    std::vector<int> ids_{0};
};

// ------------------------------------------------------------------------------------------- editor
QproEditor::QproEditor(QproProcessor& p)
    : PluginFrame(p, p.state, "PatQpro", "pro equalizer", kAccent, 1100, 700),
      proc_(p),
      freq_(p.state, bandId(0, Freq), "FREQ", kAccent, settings()),
      gain_(p.state, bandId(0, Gain), "GAIN", kAccent, settings()),
      q_(p.state, bandId(0, Q), "Q", kAccent, settings()),
      range_(p.state, bandId(0, Range), "RANGE", kAccent, settings()),
      threshold_(p.state, bandId(0, Threshold), "THRESHOLD", kAccent, settings()),
      mode_(p.state, kMode, {"ZERO LATENCY", "NATURAL", "LINEAR"}, kAccent),
      resolutionAtt_(p.state, kResolution, resolution_),
      scale_(p.state, kScale, "GAIN SCALE", kAccent, settings()),
      output_(p.state, kOutput, "OUTPUT", kAccent, settings()),
      pan_(p.state, kPan, "PAN", kAccent, settings()),
      attack_(p.state, kAttack, "ATTACK", kAccent, settings()),
      release_(p.state, kRelease, "RELEASE", kAccent, settings()),
      autoGainAtt_(p.state, kAutoGain, autoGain_),
      invertAtt_(p.state, kInvert, invert_) {
    display_ = std::make_unique<Display>(*this, p, settings());
    match_ = std::make_unique<MatchPanel>(*this, p, settings());
    for (juce::Component* c : std::initializer_list<juce::Component*>{
             display_.get(), &bandTitle_, &type_, &slope_, &place_, &freq_, &gain_, &q_, &range_, &threshold_, &active_, &dynamic_, &sidechain_, &solo_,
             &delete_, &noBand_, &mode_, &resolution_, &range_dB_, &compare_, &scale_, &output_, &pan_, &attack_, &release_, &autoGain_, &invert_, &pre_,
             &post_, &sc_, &piano_, &undo_, &redo_, &matchButton_, &modeLabel_, &analyzerLabel_, &compareLabel_})
        content().addAndMakeVisible(c);
    content().addChildComponent(*match_);

    // Resolution is filled before its attachment syncs (constructed above): set items now.
    resolution_.addItemList({"Low", "Medium", "High", "Very High"}, 1);
    resolution_.setSelectedItemIndex(juce::roundToInt(p.state.getRawParameterValue(kResolution)->load()), juce::dontSendNotification);
    resolution_.setTooltip("Linear phase length: higher = more precise lows, more latency");
    for (int t = 0; t < EqBand::NumTypes; ++t) type_.addItem(EqBand::typeName(t), t + 1);
    for (int s = 0; s < EqBand::kNumSlopes; ++s) slope_.addItem(s == EqBand::kNumSlopes - 1 ? juce::String("Brickwall") : juce::String(EqBand::slopeDb(s)) + " dB/oct", s + 1);
    place_.addItemList({"Stereo", "Left", "Right", "Mid", "Side"}, 1);
    range_dB_.addItemList({"Range 3 dB", "Range 6 dB", "Range 12 dB", "Range 30 dB"}, 1);
    compare_.setTooltip("Show another track (another PatQpro) or the sidechain, and mark in red where both fight");
    compare_.onChange = [this] {
        const int i = compare_.getSelectedId() - 1;
        display_->compareId = i <= 0 ? -1 : compareIds_[static_cast<std::size_t>(i - 1)];
        proc_.state.state.setProperty("compare", display_->compareId, nullptr);
    };

    // Display settings live in the plugin state.
    auto& st = p.state.state;
    const float r = static_cast<float>(st.getProperty("displayRange", 12.0f));
    display_->setRange(r);
    range_dB_.setSelectedId(r <= 3.0f ? 1 : r <= 6.0f ? 2 : r <= 12.0f ? 3 : 4, juce::dontSendNotification);
    range_dB_.onChange = [this] {
        const float v[] = {3.0f, 6.0f, 12.0f, 30.0f};
        const float rr = v[juce::jlimit(0, 3, range_dB_.getSelectedId() - 1)];
        display_->setRange(rr);
        proc_.state.state.setProperty("displayRange", rr, nullptr);
    };
    const auto bindToggle = [&st](juce::ToggleButton& b, const char* key, bool def, std::function<void(bool)> apply) {
        b.setToggleState(static_cast<bool>(st.getProperty(key, def)), juce::dontSendNotification);
        apply(b.getToggleState());
        b.onClick = [&b, &st, key, apply] {
            st.setProperty(key, b.getToggleState(), nullptr);
            apply(b.getToggleState());
        };
    };
    bindToggle(pre_, "showPre", true, [this](bool v) { display_->showPre = v; });
    bindToggle(post_, "showPost", true, [this](bool v) { display_->showPost = v; });
    bindToggle(sc_, "showSc", true, [this](bool v) { display_->showSc = v; });
    bindToggle(piano_, "piano", false, [this](bool v) { display_->setPiano(v); });
    display_->compareId = static_cast<int>(st.getProperty("compare", -1));

    for (auto* t : {&active_, &dynamic_, &sidechain_, &autoGain_, &invert_, &pre_, &post_, &sc_, &piano_}) t->setColour(juce::ToggleButton::tickColourId, kAccent);
    pre_.setTooltip("Show the input spectrum");
    post_.setTooltip("Show the output spectrum");
    sc_.setTooltip("Show the sidechain spectrum");
    piano_.setTooltip("Piano keyboard under the display");
    autoGain_.setTooltip("Keeps the loudness the same while you EQ");
    invert_.setTooltip("Flip the polarity of the output");
    dynamic_.setTooltip("The band moves its gain (up to RANGE) when the level passes THRESHOLD");
    sidechain_.setTooltip("The dynamic band listens to the sidechain input instead of the track");
    solo_.setClickingTogglesState(false);
    solo_.setTooltip("Hear only what this band works on");
    solo_.onClick = [this] {
        if (selected_ >= 0) proc_.solo.store(proc_.solo.load() == selected_ ? -1 : selected_);
    };
    delete_.onClick = [this] {
        if (selected_ < 0) return;
        proc_.undo.beginNewTransaction("Delete band");
        if (proc_.solo.load() == selected_) proc_.solo.store(-1);
        proc_.setBand(selected_, EqBand{});
        select(-1);
    };
    undo_.onClick = [this] { proc_.undo.undo(); };
    redo_.onClick = [this] { proc_.undo.redo(); };
    matchButton_.setTooltip("Make this track sound like a reference (sidechain or another PatQpro)");
    matchButton_.onClick = [this] {
        match_->refreshReferences();
        match_->setVisible(!match_->isVisible());
        match_->toFront(false);
    };
    for (auto* b : {&solo_, &delete_, &undo_, &redo_, &matchButton_}) b->setColour(juce::TextButton::buttonOnColourId, kAccent);
    for (auto* l : {&modeLabel_, &analyzerLabel_, &compareLabel_, &bandTitle_, &noBand_}) {
        l->setFont(font(11.5f, true));
        l->setColour(juce::Label::textColourId, colours::dim);
    }
    modeLabel_.setText("MODE", juce::dontSendNotification);
    analyzerLabel_.setText("ANALYZER", juce::dontSendNotification);
    compareLabel_.setText("COMPARE", juce::dontSendNotification);
    noBand_.setText("Double-click the display to add a band, or drag from an empty spot. Right-click a node for more.", juce::dontSendNotification);
    noBand_.setJustificationType(juce::Justification::centred);
    noBand_.setFont(font(12.0f));

    // Start with the first used band selected.
    for (int i = 0; i < kBands && selected_ < 0; ++i)
        if (p.band(i).used) select(i);
    bindBand();
    refreshCompare();

    clock().add([this](float dt) { tick(dt); });
    setPresets(*p.presets);
    finishConstruction();
}

QproEditor::~QproEditor() = default;

void QproEditor::select(int band, bool add) {
    if (band < 0) {
        selection_.clear();
        selected_ = -1;
    } else if (add) {
        if (selection_.count(band) && selection_.size() > 1) {
            selection_.erase(band);
            if (selected_ == band) selected_ = *selection_.begin();
        } else {
            selection_.insert(band);
            selected_ = band;
        }
    } else {
        selection_ = {band};
        selected_ = band;
    }
    bindBand();
}

void QproEditor::selectionKeep(int band) {
    selected_ = band;
    bindBand();
}

void QproEditor::bindBand() {
    const bool has = selected_ >= 0 && proc_.band(selected_).used;
    if (!has && selected_ >= 0) {
        selected_ = -1;
        selection_.clear();
    }
    for (juce::Component* c : std::initializer_list<juce::Component*>{&bandTitle_, &type_, &freq_, &gain_, &q_}) c->setVisible(has);
    const bool pro = proView();
    for (juce::Component* c : std::initializer_list<juce::Component*>{&slope_, &place_, &range_, &threshold_, &active_, &dynamic_, &sidechain_, &solo_, &delete_})
        c->setVisible(has && pro);
    noBand_.setVisible(!has);
    if (!has || selected_ == boundBand_) return;
    boundBand_ = selected_;
    const int b = selected_;
    typeAtt_.reset();
    slopeAtt_.reset();
    placeAtt_.reset();
    activeAtt_.reset();
    dynAtt_.reset();
    scAtt_.reset();
    typeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.state, bandId(b, Type), type_);
    slopeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.state, bandId(b, Slope), slope_);
    placeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc_.state, bandId(b, Place), place_);
    activeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.state, bandId(b, Active), active_);
    dynAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.state, bandId(b, Dyn), dynamic_);
    scAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.state, bandId(b, Sc), sidechain_);
    freq_.attach(proc_.state, bandId(b, Freq));
    gain_.attach(proc_.state, bandId(b, Gain));
    q_.attach(proc_.state, bandId(b, Q));
    range_.attach(proc_.state, bandId(b, Range));
    threshold_.attach(proc_.state, bandId(b, Threshold));
    const auto col = bandColour(b);
    for (auto* k : {&freq_, &gain_, &q_, &range_, &threshold_}) k->setAccent(col);
    bandTitle_.setText("BAND " + juce::String(b + 1), juce::dontSendNotification);
    bandTitle_.setColour(juce::Label::textColourId, col);
    releaseKeyboard(*this);
}

void QproEditor::refreshCompare() {
    const auto others = proc_.otherInstances();
    std::vector<int> ids = {0};
    juce::StringArray names{"Sidechain"};
    for (const auto& [id, name] : others) {
        ids.push_back(id);
        names.add(name);
    }
    if (ids == compareIds_ && compare_.getNumItems() == static_cast<int>(ids.size()) + 1) return;
    compareIds_ = ids;
    compare_.clear(juce::dontSendNotification);
    compare_.addItem("Off", 1);
    for (int i = 0; i < names.size(); ++i) compare_.addItem(names[i], i + 2);
    int sel = 1;
    for (int i = 0; i < static_cast<int>(ids.size()); ++i)
        if (ids[static_cast<std::size_t>(i)] == display_->compareId) sel = i + 2;
    if (sel == 1) display_->compareId = -1;
    compare_.setSelectedId(sel, juce::dontSendNotification);
}

void QproEditor::tick(float) {
    // While open, the analyzer follows the visual level (Neon fixed: 10 per second).
    const auto& prof = settings().profile();
    proc_.setAnalyzerRate(settings().frameRate(), prof.fftSize >= 4096 ? 8192 : 4096);
    if (selected_ >= 0 && !proc_.band(selected_).used) select(-1);
    if (selected_ >= 0) {  // grey out what this band type does not use
        const auto b = proc_.band(selected_);
        gain_.setEnabled(EqBand::hasGain(b.type));
        slope_.setEnabled(EqBand::hasSlope(b.type));
        dynamic_.setEnabled(EqBand::hasGain(b.type));
        for (juce::Component* c : std::initializer_list<juce::Component*>{&range_, &threshold_, &sidechain_}) c->setEnabled(b.dynamic && EqBand::hasGain(b.type));
    }
    display_->refresh();
    undo_.setEnabled(proc_.undo.canUndo());
    redo_.setEnabled(proc_.undo.canRedo());
    solo_.setToggleState(selected_ >= 0 && proc_.solo.load() == selected_, juce::dontSendNotification);
    const bool linear = juce::roundToInt(proc_.state.getRawParameterValue(kMode)->load()) == pat::dsp::EqGlobal::Linear;
    resolution_.setEnabled(linear);
    if (++compareTick_ % 20 == 0) refreshCompare();
    if (match_->isVisible()) match_->refreshReferences();
}

void QproEditor::viewChanged(bool pro) {
    for (juce::Component* c : std::initializer_list<juce::Component*>{&mode_, &resolution_, &range_dB_, &compare_, &scale_, &pan_, &attack_, &release_, &autoGain_,
                                                                       &invert_, &pre_, &post_, &sc_, &piano_, &undo_, &redo_, &matchButton_, &modeLabel_,
                                                                       &analyzerLabel_, &compareLabel_})
        c->setVisible(pro);
    if (!pro) match_->setVisible(false);
    boundBand_ = -2;
    bindBand();
}

void QproEditor::layoutContent(juce::Rectangle<int> area) {
    auto r = area.reduced(12, 8);
    const bool pro = proView();
    display_->setBounds(r.removeFromTop(pro ? 420 : 500));
    match_->setBounds(display_->getRight() - 320, display_->getY() + 10, 300, 186);
    r.removeFromTop(10);

    if (!pro) {
        auto out = r.removeFromRight(110);
        output_.setBounds(out.removeFromTop(110));
        noBand_.setBounds(r);
        auto left = r.removeFromLeft(170);
        bandTitle_.setBounds(left.removeFromTop(22));
        type_.setBounds(left.removeFromTop(26));
        const int kw = 110;
        freq_.setBounds(r.removeFromLeft(kw).withHeight(110));
        gain_.setBounds(r.removeFromLeft(kw).withHeight(110));
        q_.setBounds(r.removeFromLeft(kw).withHeight(110));
        return;
    }

    // Band panel (left).
    auto band = r.removeFromLeft(560);
    noBand_.setBounds(band);
    auto rowA = band.removeFromTop(26);
    bandTitle_.setBounds(rowA.removeFromLeft(68));
    type_.setBounds(rowA.removeFromLeft(118).reduced(2, 0));
    slope_.setBounds(rowA.removeFromLeft(100).reduced(2, 0));
    place_.setBounds(rowA.removeFromLeft(86).reduced(2, 0));
    active_.setBounds(rowA.removeFromLeft(62).reduced(4, 0));
    solo_.setBounds(rowA.removeFromLeft(58).reduced(2, 0));
    delete_.setBounds(rowA.removeFromLeft(68).reduced(2, 0));
    band.removeFromTop(8);
    auto rowB = band.removeFromTop(112);
    freq_.setBounds(rowB.removeFromLeft(92));
    gain_.setBounds(rowB.removeFromLeft(92));
    q_.setBounds(rowB.removeFromLeft(92));
    auto dynCol = rowB.removeFromLeft(96).reduced(4, 22);
    dynamic_.setBounds(dynCol.removeFromTop(26));
    dynCol.removeFromTop(8);
    sidechain_.setBounds(dynCol.removeFromTop(26));
    range_.setBounds(rowB.removeFromLeft(92));
    threshold_.setBounds(rowB.removeFromLeft(92));

    // Global panel (right).
    r.removeFromLeft(16);
    auto g1 = r.removeFromTop(26);
    modeLabel_.setBounds(g1.removeFromLeft(44));
    resolution_.setBounds(g1.removeFromRight(110).reduced(2, 0));
    mode_.setBounds(g1);
    r.removeFromTop(6);
    auto g2 = r.removeFromTop(24);
    analyzerLabel_.setBounds(g2.removeFromLeft(70));
    for (auto* t : {&pre_, &post_, &sc_}) t->setBounds(g2.removeFromLeft(62));
    piano_.setBounds(g2.removeFromLeft(72));
    range_dB_.setBounds(g2.reduced(2, 0));
    r.removeFromTop(6);
    auto g3 = r.removeFromTop(26);
    compareLabel_.setBounds(g3.removeFromLeft(70));
    compare_.setBounds(g3.removeFromLeft(150).reduced(2, 0));
    matchButton_.setBounds(g3.removeFromLeft(90).reduced(2, 0));
    redo_.setBounds(g3.removeFromRight(60).reduced(2, 0));
    undo_.setBounds(g3.removeFromRight(60).reduced(2, 0));
    r.removeFromTop(4);
    auto g4 = r.removeFromTop(96);
    auto toggles = g4.removeFromRight(100).reduced(0, 18);
    autoGain_.setBounds(toggles.removeFromTop(26));
    toggles.removeFromTop(6);
    invert_.setBounds(toggles.removeFromTop(26));
    const int kw = g4.getWidth() / 5;
    for (auto* k : {&scale_, &output_, &pan_, &attack_, &release_}) k->setBounds(g4.removeFromLeft(kw));
}

void QproEditor::simulateForSnapshot(float) {
    display_->testSignal = true;
    if (proc_.firstFreeBand() == 0) {  // empty EQ: show a typical vocal curve with a dynamic band
        proc_.presets->load(4);
        for (int i = 0; i < kBands && selected_ < 0; ++i)
            if (proc_.band(i).used) select(i);
        select(1);
    }
    display_->compareId = 0;
    display_->refresh();
}
