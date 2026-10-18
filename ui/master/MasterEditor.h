// PatMaster editor.
//   History: the level going in (grey), coming out (neon) and the limiting (red, from the top),
//            scrolling over the last seconds.
//   Loudness: integrated LUFS (big), short-term, momentary, range and true peak, compared with
//             the chosen platform (Spotify, YouTube, Club...). MATCH moves the gain so the song
//             lands on that target; RESET starts the measurement again.
//   Simple view: GAIN and CEILING. Pro view: also release (or auto), stereo link, soft clip and
//                true peak.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "plugins/PatMaster/Processor.h"

class MasterEditor final : public pat::ui::PluginFrame {
public:
    explicit MasterEditor(MasterProcessor& p);
    ~MasterEditor() override;

private:
    class History;
    class LoudnessPanel;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;

    MasterProcessor& proc_;
    std::unique_ptr<History> history_;
    std::unique_ptr<LoudnessPanel> loudness_;
    pat::ui::NeonKnob gain_, ceiling_, release_, link_, clip_;
    juce::ToggleButton autoRelease_{"AUTO RELEASE"}, truePeak_{"TRUE PEAK"};
    juce::AudioProcessorValueTreeState::ButtonAttachment autoReleaseAtt_, truePeakAtt_;
    juce::ComboBox target_;
    juce::TextButton match_{"MATCH TARGET"}, reset_{"RESET"};
    juce::Label hint_;
    bool simulating_ = false;
};
