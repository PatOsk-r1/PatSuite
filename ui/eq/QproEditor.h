// PatQpro editor.
//   Display: the analyzer (input, output, sidechain) behind the EQ curve. Every band is a numbered
//            node in its own colour:
//              - double-click an empty spot to add a band (or drag from an empty spot: the new band
//                snaps to the nearest peak of the spectrum)
//              - drag a node to move it (Shift: fine), the mouse wheel changes its Q
//              - Ctrl/Shift-click selects several nodes and moves them together
//              - double-click a node to bypass it, right-click for type, slope, placement, dynamic,
//                solo and delete
//            Dynamic bands show how far they are moving right now. With "Compare" another track
//            (another PatQpro) or the sidechain is drawn on top and the frequencies where both
//            fight are marked in red. The mouse position shows its frequency and musical note.
//   Simple view: display + the selected band's type, frequency, gain, Q and the output.
//   Pro view:    everything: slope, placement, dynamic range/threshold/sidechain, solo, processing
//                mode (zero latency / natural / linear phase), analyzer, compare, EQ match, auto gain,
//                gain scale, output pan and polarity, display range, undo/redo.
#pragma once

#include "common/ui/LayeredCanvas.h"
#include "common/ui/NeonKnob.h"
#include "common/ui/PluginFrame.h"
#include "common/ui/SegmentedChoice.h"
#include "plugins/PatQpro/Processor.h"

#include <set>

class QproEditor final : public pat::ui::PluginFrame {
public:
    explicit QproEditor(QproProcessor& p);
    ~QproEditor() override;

    void select(int band, bool add = false);
    void selectionKeep(int band);  // make `band` the main one without changing the group
    int selected() const noexcept { return selected_; }
    const std::set<int>& selection() const noexcept { return selection_; }
    static juce::Colour bandColour(int band);
    static juce::String noteName(float hz);

private:
    class Display;
    class MatchPanel;

    void layoutContent(juce::Rectangle<int> area) override;
    void viewChanged(bool pro) override;
    void simulateForSnapshot(float seconds) override;
    void tick(float dt);
    void bindBand();
    void refreshCompare();

    QproProcessor& proc_;
    std::unique_ptr<Display> display_;
    std::unique_ptr<MatchPanel> match_;
    int selected_ = -1;
    std::set<int> selection_;

    // Selected band.
    juce::Label bandTitle_;
    juce::ComboBox type_, slope_, place_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt_, slopeAtt_, placeAtt_;
    pat::ui::NeonKnob freq_, gain_, q_, range_, threshold_;
    juce::ToggleButton active_{"ON"}, dynamic_{"DYNAMIC"}, sidechain_{"SC"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> activeAtt_, dynAtt_, scAtt_;
    juce::TextButton solo_{"SOLO"}, delete_{"DELETE"};
    juce::Label noBand_;

    // Global.
    pat::ui::SegmentedChoice mode_;
    juce::ComboBox resolution_, range_dB_, compare_;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment resolutionAtt_;
    pat::ui::NeonKnob scale_, output_, pan_, attack_, release_;
    juce::ToggleButton autoGain_{"AUTO GAIN"}, invert_{"INVERT"}, pre_{"PRE"}, post_{"POST"}, sc_{"SC"}, piano_{"PIANO"};
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAtt_, invertAtt_;
    juce::TextButton undo_{"UNDO"}, redo_{"REDO"}, matchButton_{"EQ MATCH"};
    juce::Label modeLabel_, analyzerLabel_, compareLabel_;
    std::vector<int> compareIds_;
    int boundBand_ = -2, compareTick_ = 0;
};
