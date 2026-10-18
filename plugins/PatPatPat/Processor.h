// PatPatPat processor: host parameters -> DelayEngine. Tempo, levels and the input envelope go
// to the editor through atomics (for the bouncing echoes display).
#pragma once

#include "common/ui/Presets.h"
#include "dsp/delay/DelayEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace patpatpat {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kMode = "mode";  // Mono / Stereo / Ping-Pong / Cross
inline constexpr const char* kCharacter = "character";
inline constexpr const char* kSync = "sync";
inline constexpr const char* kDivL = "divl";
inline constexpr const char* kDivR = "divr";
inline constexpr const char* kTimeL = "timel";
inline constexpr const char* kTimeR = "timer";
inline constexpr const char* kLink = "link";
inline constexpr const char* kFeedback = "feedback";
inline constexpr const char* kLowCut = "lowcut";
inline constexpr const char* kHighCut = "highcut";
inline constexpr const char* kDrive = "drive";
inline constexpr const char* kModRate = "modrate";
inline constexpr const char* kModDepth = "moddepth";
inline constexpr const char* kDuck = "duck";
inline constexpr const char* kFreeze = "freeze";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kDiffusion = "diffusion";
inline constexpr const char* kMix = "mix";
inline constexpr const char* kOutput = "output";
}  // namespace patpatpat

class PatPatPatProcessor final : public juce::AudioProcessor {
public:
    PatPatPatProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatPatPat"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static std::vector<pat::ui::FactoryPreset> factoryPresets();

    // Current settings as the engine sees them (also used by the editor for the tap display).
    pat::dsp::DelaySettings settings() const noexcept;

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;
    std::atomic<double> bpm{120};
    std::atomic<float> inputLevel{0}, peakL{0}, peakR{0}, duckGain{1};

private:
    pat::dsp::DelayEngine engine_;
    std::array<std::atomic<float>*, 20> p_{};
};
