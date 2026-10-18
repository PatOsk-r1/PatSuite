#include "common/ui/PluginFrame.h"

#include "common/ui/KeyboardFocus.h"

namespace pat::ui {

PluginFrame::PluginFrame(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& state, const juce::String& name,
                         const juce::String& subtitle, juce::Colour accent, int width, int height)
    : juce::AudioProcessorEditor(&p), name_(name), subtitle_(subtitle), accent_(accent), width_(width), height_(height),
      clock_(*this, *settings_) {
    setLookAndFeel(&lnf_);
    proView_.referTo(state.state.getPropertyAsValue("proView", nullptr));
    proView_.addListener(this);

    addAndMakeVisible(canvas_);
    canvas_.addAndMakeVisible(header_);
    canvas_.addAndMakeVisible(content_);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&simple_, &pro_, &level_, &neonFixed_, &status_}) header_.addAndMakeVisible(c);

    for (auto* b : {&simple_, &pro_}) {
        b->setColour(juce::TextButton::buttonOnColourId, accent_);
        b->setClickingTogglesState(false);
    }
    simple_.setTooltip("Simple view: the essentials");
    pro_.setTooltip("Pro view: every control");
    simple_.onClick = [this] { proView_ = false; };
    pro_.onClick = [this] { proView_ = true; };

    level_.addItem("Level: Auto", 1);
    level_.addItem("Level: Low", 2);
    level_.addItem("Level: Medium", 3);
    level_.addItem("Level: High", 4);
    level_.setTooltip("Visual quality. Auto picks one from your computer; Low is lightest on the CPU.");
    level_.onChange = [this] { settings_->setChoice(static_cast<PerfSettings::Choice>(level_.getSelectedId() - 1)); };

    neonFixed_.setColour(juce::ToggleButton::tickColourId, accent_);
    neonFixed_.setTooltip("Neon fixed: turns every animation off; meters update every 100 ms");
    neonFixed_.onClick = [this] {
        settings_->setNeonFixed(neonFixed_.getToggleState() ? PerfSettings::NeonFixed::On : PerfSettings::NeonFixed::Off);
    };
    status_.setFont(font(10.5f));
    status_.setColour(juce::Label::textColourId, colours::faint);
    status_.setJustificationType(juce::Justification::centredLeft);
    status_.setBorderSize({});

    settings_->addChangeListener(this);
    syncHeader();

    clock_.add([this](float) {
        updateStatus();
        if (presetBar_) presetBar_->refresh();
        if (snapshotCountdown_ > 0 && --snapshotCountdown_ == 0) runSnapshot();
    });
    if (juce::SystemStats::getEnvironmentVariable("PATSUITE_SNAPSHOT", {}).isNotEmpty()) snapshotCountdown_ = 15;
}

void PluginFrame::updateStatus() {
    const juce::String s = juce::String(std::string(tierName(settings_->tier()))) + (settings_->neonFixed() ? " | neon fixed" : "") + " | " +
                           juce::String(settings_->frameRate()) + " fps" + (clock_.paused() ? " | paused" : "");
    if (status_.getText() != s) status_.setText(s, juce::dontSendNotification);
}

void PluginFrame::setPresets(PresetLibrary& lib) {
    presets_ = &lib;
    presetBar_ = std::make_unique<PresetBar>(lib, accent_);
    header_.addAndMakeVisible(*presetBar_);
}

void PluginFrame::finishConstruction() {
    ready_ = true;
    viewChanged(proView());
    setResizable(true, true);
    setResizeLimits(width_ * 6 / 10, height_ * 6 / 10, width_ * 2, height_ * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio(static_cast<double>(width_) / height_);
    setSize(width_, height_);
    resized();
    releaseKeyboard(*this);  // keep the host's computer-keyboard MIDI working
}

PluginFrame::~PluginFrame() {
    settings_->removeChangeListener(this);
    proView_.removeListener(this);
    setLookAndFeel(nullptr);
}

void PluginFrame::valueChanged(juce::Value&) {
    syncHeader();
    if (!ready_) return;
    viewChanged(proView());
    layoutContent(content_.getLocalBounds());
    repaint();
}

void PluginFrame::syncHeader() {
    simple_.setToggleState(!proView(), juce::dontSendNotification);
    pro_.setToggleState(proView(), juce::dontSendNotification);
    level_.setSelectedId(static_cast<int>(settings_->choice()) + 1, juce::dontSendNotification);
    level_.changeItemText(1, "Level: Auto (" + juce::String(std::string(tierName(settings_->detectedTier()))) + ")");
    neonFixed_.setToggleState(settings_->neonFixed(), juce::dontSendNotification);
}

void PluginFrame::paint(juce::Graphics& g) { g.fillAll(colours::background); }

void PluginFrame::resized() {
    if (!ready_) return;
    canvas_.setTransform(juce::AffineTransform::scale(static_cast<float>(getWidth()) / static_cast<float>(width_)));
    canvas_.setBounds(0, 0, width_, height_);
    header_.setBounds(0, 0, width_, kHeaderH);
    content_.setBounds(0, kHeaderH, width_, height_ - kHeaderH);

    auto h = header_.getLocalBounds().reduced(14, 10).withTrimmedBottom(4);
    neonFixed_.setBounds(h.removeFromRight(118));
    h.removeFromRight(8);
    level_.setBounds(h.removeFromRight(160).reduced(0, 2));
    status_.setBounds(width_ - 14 - 118 - 8 - 160, kHeaderH - 14, 160, 12);  // small line under the level selector
    if (presetBar_) {
        // Preset browser in the middle; SIMPLE | PRO moves next to the level selector.
        h.removeFromRight(12);
        pro_.setBounds(h.removeFromRight(60).withY(12).withHeight(kHeaderH - 24));
        simple_.setBounds(h.removeFromRight(66).withY(12).withHeight(kHeaderH - 24));
        const int nameEnd = 44 + juce::roundToInt(juce::GlyphArrangement::getStringWidth(font(20.0f, true), name_)) + 22;
        presetBar_->setBounds(nameEnd, 12, juce::jmax(160, simple_.getX() - 20 - nameEnd), kHeaderH - 24);
    } else {
        const int segW = 92;
        simple_.setBounds(width_ / 2 - segW, 12, segW, kHeaderH - 24);
        pro_.setBounds(width_ / 2, 12, segW, kHeaderH - 24);
    }

    layoutContent(content_.getLocalBounds());
}

void PluginFrame::Header::paint(juce::Graphics& g) {
    // Static header: logo orb, plugin name and subtitle.
    g.setColour(colours::panel);
    g.fillRect(getLocalBounds());
    g.setColour(colours::line);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
    const auto orb = juce::Rectangle<float>(16.0f, static_cast<float>(getHeight()) * 0.5f - 9.0f, 18.0f, 18.0f);
    g.setColour(frame.accent_.withAlpha(0.25f));
    g.fillEllipse(orb.expanded(4.0f));
    g.setColour(frame.accent_);
    g.drawEllipse(orb, 2.0f);
    g.fillEllipse(orb.reduced(6.0f));
    g.setColour(colours::text);
    g.setFont(font(20.0f, true));
    g.drawText(frame.name_, 44, 0, 200, getHeight(), juce::Justification::centredLeft);
    const int nameW = juce::roundToInt(juce::GlyphArrangement::getStringWidth(font(20.0f, true), frame.name_));
    if (frame.presetBar_) return;  // the preset browser takes the subtitle's place
    g.setColour(colours::dim);
    g.setFont(font(12.0f));
    g.drawText(frame.subtitle_, 52 + nameW, 0, 220, getHeight(), juce::Justification::centredLeft);
}

void PluginFrame::runSnapshot() {
    const juce::File dir(juce::SystemStats::getEnvironmentVariable("PATSUITE_SNAPSHOT", {}));
    dir.createDirectory();
    const auto oldChoice = settings_->choice();
    const auto oldNeon = settings_->neonFixedChoice();
    const bool oldPro = proView();
    juce::String csv = "level,neon_fixed,view,render_ms\n";
    for (int t = 0; t < 3; ++t)
        for (int neon = 0; neon < 2; ++neon)
            for (int pro = 0; pro < 2; ++pro) {
                settings_->forceForSnapshot(static_cast<PerfTier>(t), neon == 1);
                proView_ = pro == 1;
                valueChanged(proView_);  // Value listeners are asynchronous; apply now
                updateStatus();
                simulateForSnapshot(2.0f);
                // Average render time over a few frames (static layer cached after the first).
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                juce::Image img;
                for (int k = 0; k < 5; ++k) img = createComponentSnapshot(getLocalBounds(), true, 1.0f);  // includes the background
                const double ms = (juce::Time::getMillisecondCounterHiRes() - t0) / 5.0;
                const juce::String tag = juce::String(std::string(tierName(static_cast<PerfTier>(t)))).toLowerCase() + (neon ? "-neonfixed" : "-animated") +
                                         (pro ? "-pro" : "-simple");
                juce::PNGImageFormat png;
                const auto file = dir.getChildFile(name_ + "-" + tag + ".png");
                file.deleteFile();
                if (auto out = file.createOutputStream()) png.writeImageToStream(img, *out);
                csv << tierName(static_cast<PerfTier>(t)).data() << "," << neon << "," << (pro ? "pro" : "simple") << "," << juce::String(ms, 2) << "\n";
            }
    dir.getChildFile(name_ + "-render-cost.csv").replaceWithText(csv);
    const auto& hw = settings_->hardware();
    dir.getChildFile(name_ + "-hardware.txt")
        .replaceWithText("cores=" + juce::String(hw.physicalCores) + "\nmemoryMB=" + juce::String(hw.memoryMB) + "\ngpuMB=" +
                         juce::String(hw.gpuMemoryMB) + "\ngpu=" + settings_->gpuName() + "\ndetected=" +
                         juce::String(std::string(tierName(settings_->detectedTier()))) + "\n");
    if (presets_) {
        juce::String txt = "factory=" + juce::String(presets_->numFactory()) + " total=" + juce::String(static_cast<int>(presets_->entries().size())) + "\n";
        for (const auto& problem : presets_->validate()) txt << "PROBLEM " << problem << "\n";
        for (const auto& e : presets_->entries()) txt << e.category << "/" << e.name << "\n";
        dir.getChildFile(name_ + "-presets.txt").replaceWithText(txt);
    }
    // Back to the user's own settings (the forced ones were never saved).
    settings_->setChoice(oldChoice);
    settings_->setNeonFixed(oldNeon);
    proView_ = oldPro;
    if (juce::SystemStats::getEnvironmentVariable("PATSUITE_SNAPSHOT_QUIT", {}) == "1")
        if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
}

}  // namespace pat::ui
