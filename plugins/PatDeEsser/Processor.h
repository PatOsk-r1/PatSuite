// PatDeEsser processor: host parameters -> DeEsserEngine; meters and the input spectrum feed go
// to the editor through atomics and a lock-free FIFO.
#pragma once

#include "common/core/SpscFifo.h"
#include "dsp/deesser/DeEsserEngine.h"

#include "common/ui/Presets.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace deesser {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kFreq = "freq";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kThreshold = "threshold";
inline constexpr const char* kRatio = "ratio";
inline constexpr const char* kRelease = "release";
inline constexpr const char* kMode = "mode";  // 0 = split band, 1 = wide band
inline constexpr const char* kListen = "listen";
inline constexpr const char* kOutput = "output";

// The Simple view's "Smooth" knob is a macro over threshold and ratio (0 = gentle, 1 = strong).
inline float smoothToThreshold(float s) noexcept { return -12.0f - 30.0f * s; }
inline float smoothToRatio(float s) noexcept { return 2.0f + 8.0f * s; }
inline float thresholdToSmooth(float thr) noexcept { return juce::jlimit(0.0f, 1.0f, (-12.0f - thr) / 30.0f); }
}  // namespace deesser

class DeEsserProcessor final : public juce::AudioProcessor {
public:
    DeEsserProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatDeEsser"; }
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
    pat::SpscFifo<float> analysis{32768};  // mono input, for the spectrum
    std::atomic<float> detectorDb{-120}, reductionDb{0}, inPeak{0}, outPeak{0};
    std::atomic<double> sampleRate{48000};

private:
    pat::dsp::DeEsserEngine engine_;
    std::atomic<float>*freq_ = nullptr, *width_ = nullptr, *threshold_ = nullptr, *ratio_ = nullptr, *release_ = nullptr, *mode_ = nullptr,
                        *listen_ = nullptr, *output_ = nullptr;
    std::array<float, 4096> mono_{};
};
