// PatMaster processor: limiter + loudness meter on the output. Meter values go to the editor
// through atomics, the level history through a lock-free FIFO.
#pragma once

#include "common/core/SpscFifo.h"
#include "common/ui/Presets.h"
#include "dsp/master/LimiterEngine.h"
#include "dsp/master/Loudness.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace master {
// Parameter IDs (stored in host projects: never rename).
inline constexpr const char* kGain = "gain";
inline constexpr const char* kCeiling = "ceiling";
inline constexpr const char* kRelease = "release";
inline constexpr const char* kAutoRelease = "autorelease";
inline constexpr const char* kLink = "link";
inline constexpr const char* kTruePeak = "truepeak";
inline constexpr const char* kClip = "clip";

// Loudness targets of the main platforms (integrated LUFS, true peak ceiling).
struct Target {
    const char* name;
    float lufs, ceilingDb;
};
inline constexpr Target kTargets[] = {
    {"Spotify", -14.0f, -1.0f},       {"YouTube", -14.0f, -1.0f},         {"Apple Music", -16.0f, -1.0f}, {"Amazon Music", -14.0f, -2.0f},
    {"Tidal", -14.0f, -1.0f},         {"Deezer", -15.0f, -1.0f},          {"SoundCloud", -14.0f, -1.0f},  {"Club / DJ", -8.0f, -0.3f},
    {"Loud Trap / EDM", -7.0f, -0.3f}, {"Podcast", -16.0f, -1.0f},         {"TV (EBU R128)", -23.0f, -1.0f},
};
inline constexpr int kNumTargets = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));

struct HistoryPoint {
    float in = 0, out = 0, gr = 0;  // peaks (linear) and gain reduction (dB, <= 0)
};
}  // namespace master

class MasterProcessor final : public juce::AudioProcessor {
public:
    MasterProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatMaster"; }
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

    int target() const;  // index into kTargets (stored in the plugin state)
    void setTarget(int t);
    // Moves the gain so the integrated loudness lands on the target (needs some playback first).
    bool matchTarget();
    void resetMeter() noexcept { resetRequest_.store(true); }

    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;
    std::atomic<float> momentary{-70}, shortTerm{-70}, integrated{-70}, range{0}, truePeak{-120}, grDb{0};
    std::array<std::atomic<float>, 2> outPeak{};
    pat::SpscFifo<master::HistoryPoint> history{4096};

private:
    pat::dsp::LimiterEngine limiter_;
    pat::dsp::LoudnessMeter meter_;
    std::array<std::atomic<float>*, 7> p_{};
    std::atomic<bool> resetRequest_{false};
    int histFill_ = 0;
    master::HistoryPoint histAcc_{};
};
