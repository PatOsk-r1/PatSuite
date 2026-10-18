// PatDeEsser editor.
//   Simple view: live spectrum with the sibilance band in neon, one big "Smooth" knob, and a
//                reduction meter.
//   Pro view:    the band can be dragged on the spectrum (centre = frequency, edges = width);
//                frequency, width, threshold, ratio, release and output knobs; split/wide mode;
//                listen; a sibilance meter with a draggable threshold line.
#pragma once

#include "common/ui/LevelMeter.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SpectrumView.h"
#include "plugins/PatDeEsser/Processor.h"

class DeEsserEditor final : public pat::ui::PluginFrame {
public:
    explicit DeEsserEditor(DeEsserProcessor& p);
    ~DeEsserEditor() override;

private:
    class BandOverlay;
    class SibilanceMeter;
    class ReductionMeter;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;
    void tick(float dt, float detectorDb, float reductionDb);
    float param(const char* id) const;
    void setParam(const char* id, float value);

    DeEsserProcessor& proc_;
    pat::ui::SpectrumView spectrum_;
    std::unique_ptr<BandOverlay> overlay_;
    std::unique_ptr<SibilanceMeter> sibilance_;
    std::unique_ptr<ReductionMeter> reduction_;
    pat::ui::NeonKnob smooth_;
    pat::ui::NeonKnob freq_, width_, threshold_, ratio_, release_, output_;
    juce::TextButton split_{"SPLIT BAND"}, wide_{"WIDE BAND"};
    juce::ToggleButton listen_{"LISTEN"};
    juce::AudioProcessorValueTreeState::ButtonAttachment listenAttachment_;
    juce::Label gentle_, strong_, hint_;
    bool simulating_ = false;
    float lastFreq_ = -1, lastWidth_ = -1;
};
