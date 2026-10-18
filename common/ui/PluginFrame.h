// Base editor shared by every PatSuite plugin.
//
//   +--------------------------------------------------------------------------+
//   | (o) PatName   [< preset name >][SAVE]   [SIMPLE|PRO] [Level: Auto v] [Neon] |
//   +--------------------------------------------------------------------------+
//   |                         content (subclass)                               |
//   +--------------------------------------------------------------------------+
//
// The whole editor is laid out on a fixed canvas (width x height) and scaled to the window, so
// the plugin window can be resized freely. The Simple/Pro choice is stored in the plugin state.
// It also owns the FrameClock that drives every animated layer of the plugin.
//
// Diagnostic mode: PATSUITE_SNAPSHOT=<folder> renders every combination of level x Neon fixed x
// view to PNG and writes how long each render took (render-cost.csv). PATSUITE_SNAPSHOT_QUIT=1
// closes the standalone afterwards.
#pragma once

#include "common/ui/FrameClock.h"
#include "common/ui/PerfSettings.h"
#include "common/ui/Presets.h"
#include "common/ui/Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace pat::ui {

class PluginFrame : public juce::AudioProcessorEditor, private juce::ChangeListener, private juce::Value::Listener {
public:
    static constexpr int kHeaderH = 52;

    PluginFrame(juce::AudioProcessor& p, juce::AudioProcessorValueTreeState& state, const juce::String& name, const juce::String& subtitle,
                juce::Colour accent, int width, int height);
    ~PluginFrame() override;

    void paint(juce::Graphics&) override;
    void resized() final;
    bool keyPressed(const juce::KeyPress&) override { return false; }  // keys belong to the host

protected:
    // Subclasses call this at the end of their constructor, once their views exist.
    void finishConstruction();
    // Optional: shows the preset browser in the header (call before finishConstruction()).
    void setPresets(PresetLibrary& lib);
    // Area below the header, in canvas coordinates; subclasses add their views to content().
    juce::Component& content() noexcept { return content_; }
    virtual void layoutContent(juce::Rectangle<int> area) = 0;
    virtual void viewChanged(bool /*pro*/) {}
    // Diagnostic snapshots: simulate input and advance the visuals by `seconds`.
    virtual void simulateForSnapshot(float /*seconds*/) {}

    bool proView() const { return static_cast<bool>(proView_.getValue()); }
    PerfSettings& settings() noexcept { return *settings_; }
    FrameClock& clock() noexcept { return clock_; }
    juce::Colour accent() const noexcept { return accent_; }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override { syncHeader(); }
    void valueChanged(juce::Value&) override;
    void syncHeader();
    void updateStatus();
    void runSnapshot();

    juce::SharedResourcePointer<PerfSettings> settings_;
    LookAndFeel lnf_;
    juce::String name_, subtitle_;
    juce::Colour accent_;
    int width_, height_;
    juce::Value proView_;
    struct Header final : juce::Component {
        explicit Header(PluginFrame& f) : frame(f) {}
        void paint(juce::Graphics& g) override;
        PluginFrame& frame;
    };
    juce::Component canvas_, content_;
    Header header_{*this};
    juce::TextButton simple_{"SIMPLE"}, pro_{"PRO"};
    juce::ComboBox level_;
    juce::ToggleButton neonFixed_{"NEON FIXED"};
    juce::Label status_;
    std::unique_ptr<PresetBar> presetBar_;
    PresetLibrary* presets_ = nullptr;
    FrameClock clock_;
    juce::TooltipWindow tooltips_{this, 600};
    int snapshotCountdown_ = -1;
    bool ready_ = false;
};

}  // namespace pat::ui
