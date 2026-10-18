// PatChain processor: main stereo bus + sidechain bus -> SidechainEngine. Graph points and
// meters go to the editor through a lock-free FIFO and atomics.
#pragma once

#include "common/core/SpscFifo.h"
#include "dsp/sidechain/SidechainEngine.h"

#include "common/ui/Presets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace sidechain {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kTrigger = "trigger";  // Auto / Tempo / Sidechain
inline constexpr const char* kPump = "pump";
inline constexpr const char* kRate = "division";  // note length of the tempo curve (1/1 .. 1/32, triplets, dotted)
inline constexpr const char* kShape = "shape";     // tempo curve shape
inline constexpr const char* kThreshold = "threshold";
inline constexpr const char* kAttack = "attack";
inline constexpr const char* kRelease = "release";
inline constexpr const char* kKnee = "knee";
inline constexpr const char* kRatio = "ratio";
inline constexpr const char* kScLow = "sclow";
inline constexpr const char* kScHigh = "schigh";
inline constexpr const char* kListen = "listen";
inline constexpr const char* kOutput = "output";
}  // namespace sidechain

class SidechainProcessor final : public juce::AudioProcessor {
public:
    SidechainProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatChain"; }
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
    static std::vector<pat::ui::FactoryPreset> factoryPresets();

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;  // factory + user presets (Presets.cpp)
    pat::SpscFifo<pat::dsp::GraphPoint> graph{8192};
    std::atomic<float> minGain{1}, triggerDb{-120};
    std::atomic<int> triggers{0};
    std::atomic<bool> usingSidechain{false}, sidechainConnected{false}, sidechainSignal{false};
    std::atomic<double> sampleRate{48000};

private:
    pat::dsp::SidechainEngine engine_;
    std::array<std::atomic<float>*, 13> p_{};
    std::array<pat::dsp::GraphPoint, 256> points_{};
};
