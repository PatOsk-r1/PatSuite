// PatQpro processor: 24 bands of host parameters -> EqEngine.
//
// Message-thread services (a timer, also while the editor is closed):
//   - spectra of the input, output and sidechain (for the display, the collision view and match)
//   - every PatQpro instance in the session publishes its output spectrum to a shared hub, so
//     each one can show where it clashes with another track
//   - auto gain from the curve, latency changes, and the linear-phase FIR builds, which run on a
//     background thread and are handed to the audio thread lock-free
// Undo/redo covers every parameter change.
#pragma once

#include "common/core/SpscFifo.h"
#include "common/ui/Presets.h"
#include "dsp/eq/EqAnalysis.h"
#include "dsp/eq/EqEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace qpro {
constexpr int kBands = pat::dsp::EqEngine::kBands;
// Per-band parameter IDs: "b<n>_<field>" with n = 1..24 (stored in host projects: never rename).
enum Field { Used, Active, Type, Freq, Gain, Q, Slope, Place, Dyn, Range, Threshold, Sc, NumFields };
juce::String bandId(int band, int field);
// Global parameter IDs.
inline constexpr const char* kMode = "mode";
inline constexpr const char* kResolution = "resolution";
inline constexpr const char* kScale = "scale";
inline constexpr const char* kOutput = "output";
inline constexpr const char* kPan = "pan";
inline constexpr const char* kInvert = "invert";
inline constexpr const char* kAutoGain = "autogain";
inline constexpr const char* kAttack = "attack";
inline constexpr const char* kRelease = "release";
}  // namespace qpro

// Output spectra of every PatQpro instance in this session (message thread only).
struct SpectrumHub {
    struct Slot {
        int id = 0;
        juce::String name;
        std::array<float, pat::dsp::SpectrumAnalyzer::kBins> db{};
        double updated = 0;
    };
    std::vector<Slot> slots;
    int nextId = 1;
};

class QproProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    QproProcessor();
    ~QproProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PatQpro"; }
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
    void updateTrackProperties(const TrackProperties& p) override;
    static std::vector<pat::ui::FactoryPreset> factoryPresets();

    // Bands as the engine sees them (message thread: for the display).
    pat::dsp::EqBand band(int i) const;
    void bands(pat::dsp::EqBand* out) const;
    pat::dsp::EqGlobal global() const;
    // Write a band into the parameters (as one undoable step when wrapped in beginEdit/endEdit).
    void setBand(int i, const pat::dsp::EqBand& b);
    void setParam(const juce::String& id, float value);  // real units, notifies the host
    int firstFreeBand() const;
    double sampleRate() const noexcept { return sr_.load(); }
    float dynamicGainDb(int i) const noexcept { return engine_.dynamicGainDb(i); }

    // Spectra (message thread), dB on the analyzer axis.
    using Spectrum = std::array<float, pat::dsp::SpectrumAnalyzer::kBins>;
    const Spectrum& preSpectrum() const noexcept { return pre_; }
    const Spectrum& postSpectrum() const noexcept { return post_; }
    const Spectrum& scSpectrum() const noexcept { return sc_; }
    bool sidechainActive() const noexcept { return scActive_.load(); }
    void setAnalyzerRate(int hz, int fftSize);  // the editor asks for faster updates while open
    // Other instances (for collisions): id, name.
    std::vector<std::pair<int, juce::String>> otherInstances() const;
    const Spectrum* instanceSpectrum(int id) const;
    juce::String instanceName() const;

    // EQ match: learn the reference (sidechain or another instance) and this track for a while,
    // then fit bands. referenceId: 0 = sidechain, otherwise an instance id.
    void startLearning(int referenceId);
    void stopLearning();
    bool learning() const noexcept { return learning_; }
    float learnedSeconds() const noexcept { return learnFrames_ / static_cast<float>(juce::jmax(1, rateHz_)); }
    bool applyMatch(int maxBands, float amount);  // replaces all bands; undoable

    juce::UndoManager undo;
    juce::AudioProcessorValueTreeState state;
    std::unique_ptr<pat::ui::PresetLibrary> presets;
    pat::SpscFifo<float> preFifo{65536}, postFifo{65536}, scFifo{65536};
    std::atomic<float> peakIn{0}, peakOut{0};
    std::atomic<int> solo{-1};  // band being soloed (not saved)

private:
    class Builder;
    void timerCallback() override;
    void updateServices();

    pat::dsp::EqEngine engine_;
    std::array<std::array<std::atomic<float>*, qpro::NumFields>, qpro::kBands> bp_{};
    std::array<std::atomic<float>*, 9> gp_{};
    std::array<pat::dsp::EqBand, qpro::kBands> audioBands_{};
    std::atomic<double> sr_{48000};
    std::atomic<float> autoGainDb_{0};
    std::atomic<bool> scActive_{false};
    std::vector<float> mono_;
    int latency_ = -1;

    pat::dsp::SpectrumAnalyzer preAn_{8192}, postAn_{8192}, scAn_{8192};
    Spectrum pre_{}, post_{}, sc_{};
    std::vector<float> pull_;
    int rateHz_ = 15, ticks_ = 0;
    juce::SharedResourcePointer<SpectrumHub> hub_;
    int hubId_ = 0;
    juce::String trackName_;

    bool learning_ = false;
    int learnRef_ = 0, learnFrames_ = 0;
    std::array<double, pat::dsp::SpectrumAnalyzer::kBins> learnRefSum_{}, learnOwnSum_{};

    std::unique_ptr<Builder> builder_;
    juce::String lastBandsKey_;
};
