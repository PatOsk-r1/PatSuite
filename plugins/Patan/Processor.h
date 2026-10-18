// Patan processor: host parameters + the drawn curve -> PanEngine. The curve is edited on the
// message thread and reaches the audio thread through a lock-free triple buffer; it is saved in
// the plugin state (property "curve") and in presets.
#pragma once

#include "common/core/TripleBuffer.h"
#include "common/ui/Presets.h"
#include "dsp/pan/PanEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace patan {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kMode = "mode";  // Pan / Volume / Width
inline constexpr const char* kSync = "sync";
inline constexpr const char* kDivision = "division";
inline constexpr const char* kRate = "rate";
inline constexpr const char* kDepth = "depth";
inline constexpr const char* kPhase = "phase";
inline constexpr const char* kSmooth = "smooth";
inline constexpr const char* kCenter = "center";
inline constexpr const char* kLaw = "law";
inline constexpr const char* kWidth = "width";
inline constexpr const char* kMix = "mix";
inline constexpr const char* kOutput = "output";
inline constexpr const char* kInvert = "invert";
}  // namespace patan

class PatanProcessor final : public juce::AudioProcessor {
public:
    PatanProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Patan"; }
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

    // The drawn curve (message thread only). Call curveChanged() after editing curve().
    pat::dsp::PanCurve& curve() noexcept { return curve_; }
    void curveChanged();
    void setCurveText(const juce::String& text);
    int curveVersion() const noexcept { return curveVersion_; }  // bumps on every change

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;
    std::atomic<double> phase{0};       // cycle position for the playhead, 0..1
    std::atomic<float> position{0};     // current curve output, -1..1
    std::atomic<float> peakL{0}, peakR{0};
    std::atomic<int> mode{0};

private:
    pat::dsp::PanEngine engine_;
    pat::dsp::PanCurve curve_;
    pat::TripleBuffer<pat::dsp::CurveTable> table_;
    int curveVersion_ = 0;
    std::array<std::atomic<float>*, 13> p_{};
};
