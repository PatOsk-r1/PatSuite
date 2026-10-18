// PatBass processor: MIDI + host parameters -> BassEngine. The output goes to the editor's
// oscilloscope through a lock-free FIFO; the on-screen keyboard plays through keyboardState.
#pragma once

#include "common/core/SpscFifo.h"
#include "common/ui/Presets.h"
#include "dsp/bass/BassEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <map>

namespace bass {
// Every parameter: ID (stored in host projects: never rename) and its place in BassSettings is
// set in Processor.cpp (makeSettings).
juce::StringArray parameterIds();
}  // namespace bass

class BassProcessor final : public juce::AudioProcessor {
public:
    BassProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatBass"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static std::vector<pat::ui::FactoryPreset> factoryPresets();

    pat::dsp::BassSettings settings() const;
    float value(const char* id) const { return raw_.at(id)->load(); }

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;
    juce::MidiKeyboardState keyboardState;
    pat::SpscFifo<float> scope{16384};
    std::atomic<float> lfoValue{0}, cutoffNow{1000}, outPeak{0};
    std::atomic<int> voices{0};

private:
    pat::dsp::BassEngine engine_;
    std::map<juce::String, std::atomic<float>*> raw_;
    std::vector<float> mono_;
};
