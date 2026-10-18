// PatDoubler processor: parameters -> DoublerEngine; the input waveform and the stereo output go
// to the editor through lock-free FIFOs (wave stack and vectorscope).
#pragma once

#include "common/core/SpscFifo.h"
#include "dsp/doubler/DoublerEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace doubler {
// Parameter IDs (stored in host projects: never rename). Per-copy IDs: delay1..4, detune1..4, pan1..4.
inline constexpr const char* kCopies = "copies";
inline constexpr const char* kVariation = "variation";
inline constexpr const char* kSpread = "spread";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kThickness = "thickness";
inline constexpr const char* kOutput = "output";
inline juce::String delayId(int c) { return "delay" + juce::String(c + 1); }
inline juce::String detuneId(int c) { return "detune" + juce::String(c + 1); }
inline juce::String panId(int c) { return "pan" + juce::String(c + 1); }
}  // namespace doubler

class DoublerProcessor final : public juce::AudioProcessor {
public:
    DoublerProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatDoubler"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.2; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Current settings from the parameters (used by the audio thread and the editor).
    pat::dsp::DoublerSettings settings() const noexcept;

    juce::AudioProcessorValueTreeState state;
    pat::SpscFifo<float> wave{16384};                        // mono input, for the wave stack
    pat::SpscFifo<pat::dsp::StereoSample> scope{8192};       // stereo output, for the vectorscope
    std::atomic<double> sampleRate{48000};

private:
    pat::dsp::DoublerEngine engine_;
    // Raw parameter values, looked up once (no string building on the audio thread).
    std::atomic<float>*copies_ = nullptr, *variation_ = nullptr, *spread_ = nullptr, *width_ = nullptr, *thickness_ = nullptr, *output_ = nullptr;
    std::array<std::atomic<float>*, 4> delay_{}, detune_{}, pan_{};
    std::array<pat::dsp::StereoSample, 2048> scopeBuf_{};
    std::array<float, 4096> mono_{};
};
