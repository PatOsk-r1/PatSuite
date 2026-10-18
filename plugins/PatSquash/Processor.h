// PatSquash processor: host parameters -> SquashEngine; band levels and gain changes go to the
// editor through atomics.
#pragma once

#include "common/ui/Presets.h"
#include "dsp/squash/SquashEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace squash {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kDepth = "depth";
inline constexpr const char* kTime = "time";
inline constexpr const char* kUpward = "upward";
inline constexpr const char* kDownward = "downward";
inline constexpr const char* kInput = "input";
inline constexpr const char* kOutput = "output";
inline constexpr const char* kLowCross = "xlow";
inline constexpr const char* kHighCross = "xhigh";
// Per band (b = 0 low, 1 mid, 2 high).
enum Field { DownThr, DownRatio, UpThr, UpRatio, Gain, NumFields };
juce::String bandId(int band, int field);
}  // namespace squash

class SquashProcessor final : public juce::AudioProcessor {
public:
    SquashProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatSquash"; }
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

    pat::dsp::SquashSettings settings() const noexcept;

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;
    std::array<std::atomic<float>, 3> bandLevelDb{}, bandGainDb{};
    std::atomic<float> inPeak{0}, outPeak{0};

private:
    pat::dsp::SquashEngine engine_;
    std::array<std::atomic<float>*, 8> gp_{};
    std::array<std::array<std::atomic<float>*, squash::NumFields>, 3> bp_{};
};
