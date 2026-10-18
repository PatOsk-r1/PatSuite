// PatTune processor: parameters -> TuneEngine. Reports the engine latency to the host (FL Studio
// compensates it). Pitch history goes to the editor through a lock-free FIFO; the custom scale
// (24 quarter-tone toggles) is plugin state, read by the audio thread through an atomic.
#pragma once

#include "common/core/SpscFifo.h"
#include "dsp/tune/TuneEngine.h"

#include "common/ui/Presets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace tune {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kAuto = "auto";
inline constexpr const char* kKey = "key";
inline constexpr const char* kScale = "scale";
inline constexpr const char* kSpeed = "speed";
inline constexpr const char* kHumanize = "humanize";
inline constexpr const char* kFormant = "formant";
inline constexpr const char* kMix = "mix";
inline constexpr const char* kGate = "gate";
inline constexpr const char* kA4 = "a4";
}  // namespace tune

class TuneProcessor final : public juce::AudioProcessor {
public:
    TuneProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatTune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static std::vector<pat::ui::FactoryPreset> factoryPresets();

    // Custom scale: bit i = note i * 50 cents above the key (24 quarter tones).
    std::uint32_t customMask() const noexcept { return customMask_.load(); }
    void setCustomMask(std::uint32_t mask);

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;  // factory + user presets (Presets.cpp)
    pat::SpscFifo<pat::dsp::TunePoint> history{4096};
    std::atomic<float> clarity{0}, detectedHz{0}, targetHz{0}, outputHz{0};

private:
    pat::dsp::TuneEngine engine_;
    std::atomic<std::uint32_t> customMask_;
    std::array<std::atomic<float>*, 9> p_{};
    std::array<pat::dsp::TunePoint, 256> points_{};
};
