// PatTemplate processor: gain + meters + analysis feed. The pattern every PatSuite plugin follows:
//   - parameters in an APVTS, read on the audio thread through cached atomic pointers;
//   - the engine (dsp/) does all the audio work;
//   - the UI gets meters through atomics and audio for its visuals through a lock-free FIFO.
#pragma once

#include "common/core/SpscFifo.h"
#include "dsp/template/TemplateEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class TemplateProcessor final : public juce::AudioProcessor {
public:
    TemplateProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatTemplate"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState state;
    pat::SpscFifo<float> analysis{32768};  // mono mix of the output, for the spectrum
    std::atomic<float> inPeak{0}, outPeak{0};
    std::atomic<double> sampleRate{48000};

private:
    pat::dsp::TemplateEngine engine_;
    std::atomic<float>* gain_ = nullptr;
    std::array<float, 4096> mono_{};
};
