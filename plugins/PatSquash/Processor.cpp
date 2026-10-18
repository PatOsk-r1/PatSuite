#include "plugins/PatSquash/Processor.h"

#include "ui/squash/SquashEditor.h"

using namespace squash;

namespace {
const char* kBandNames[3] = {"Low", "Mid", "High"};
const char* kBandKeys[3] = {"l", "m", "h"};
const char* kFieldKeys[NumFields] = {"dthr", "dratio", "uthr", "uratio", "gain"};

juce::String pct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
juce::String db(float v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; }
juce::String hz(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    const auto skewed = [](float lo, float hi, float centre) {
        juce::NormalisableRange<float> r(lo, hi);
        r.setSkewForCentre(centre);
        return r;
    };
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<F>(juce::ParameterID{kDepth, 1}, "Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kTime, 1}, "Time", skewed(0.1f, 4.0f, 1.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kUpward, 1}, "Upward", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kDownward, 1}, "Downward", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kInput, 1}, "Input Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return db(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return db(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kLowCross, 1}, "Low / Mid Split", skewed(40.0f, 1000.0f, 150.0f), 120.0f,
                              A().withStringFromValueFunction([](float v, int) { return hz(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kHighCross, 1}, "Mid / High Split", skewed(1000.0f, 12000.0f, 3000.0f), 2500.0f,
                              A().withStringFromValueFunction([](float v, int) { return hz(v); })));
    const float defDown[3] = {-28.0f, -30.0f, -32.0f}, defUp[3] = {-38.0f, -40.0f, -42.0f};
    for (int b = 0; b < 3; ++b) {
        const juce::String n = juce::String(kBandNames[b]) + " ";
        l.add(std::make_unique<F>(juce::ParameterID{bandId(b, DownThr), 1}, n + "Down Threshold", juce::NormalisableRange<float>(-60.0f, 0.0f), defDown[b],
                                  A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));
        l.add(std::make_unique<F>(juce::ParameterID{bandId(b, DownRatio), 1}, n + "Down Ratio", skewed(1.0f, 60.0f, 6.0f), 6.0f,
                                  A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + ":1"; })));
        l.add(std::make_unique<F>(juce::ParameterID{bandId(b, UpThr), 1}, n + "Up Threshold", juce::NormalisableRange<float>(-80.0f, 0.0f), defUp[b],
                                  A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));
        l.add(std::make_unique<F>(juce::ParameterID{bandId(b, UpRatio), 1}, n + "Up Ratio", skewed(1.0f, 20.0f, 3.0f), 3.0f,
                                  A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + ":1"; })));
        l.add(std::make_unique<F>(juce::ParameterID{bandId(b, Gain), 1}, n + "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f,
                                  A().withStringFromValueFunction([](float v, int) { return db(v); })));
    }
    return l;
}
}  // namespace

juce::String squash::bandId(int band, int field) { return juce::String(kBandKeys[band]) + "_" + kFieldKeys[field]; }

SquashProcessor::SquashProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    const char* ids[] = {kDepth, kTime, kUpward, kDownward, kInput, kOutput, kLowCross, kHighCross};
    for (std::size_t i = 0; i < gp_.size(); ++i) gp_[i] = state.getRawParameterValue(ids[i]);
    for (int b = 0; b < 3; ++b)
        for (int f = 0; f < NumFields; ++f) bp_[static_cast<std::size_t>(b)][static_cast<std::size_t>(f)] = state.getRawParameterValue(bandId(b, f));
    for (auto& v : bandLevelDb) v.store(-120.0f);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatSquash", factoryPresets());
}

pat::dsp::SquashSettings SquashProcessor::settings() const noexcept {
    pat::dsp::SquashSettings s;
    s.depth = gp_[0]->load();
    s.time = gp_[1]->load();
    s.upward = gp_[2]->load();
    s.downward = gp_[3]->load();
    s.inputDb = gp_[4]->load();
    s.outputDb = gp_[5]->load();
    s.lowCrossHz = gp_[6]->load();
    s.highCrossHz = gp_[7]->load();
    for (int b = 0; b < 3; ++b) {
        const auto& p = bp_[static_cast<std::size_t>(b)];
        auto& d = s.band[static_cast<std::size_t>(b)];
        d.downThresholdDb = p[DownThr]->load();
        d.downRatio = p[DownRatio]->load();
        d.upThresholdDb = p[UpThr]->load();
        d.upRatio = p[UpRatio]->load();
        d.gainDb = p[Gain]->load();
    }
    return s;
}

bool SquashProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void SquashProcessor::prepareToPlay(double sr, int) { engine_.prepare(sr); }

void SquashProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    inPeak.store(buffer.getMagnitude(0, buffer.getNumSamples()), std::memory_order_relaxed);
    engine_.process(l, r, buffer.getNumSamples(), settings());
    outPeak.store(buffer.getMagnitude(0, buffer.getNumSamples()), std::memory_order_relaxed);
    for (int b = 0; b < 3; ++b) {
        bandLevelDb[static_cast<std::size_t>(b)].store(engine_.bandLevelDb(b), std::memory_order_relaxed);
        bandGainDb[static_cast<std::size_t>(b)].store(engine_.bandGainDb(b), std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* SquashProcessor::createEditor() { return new SquashEditor(*this); }

void SquashProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void SquashProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SquashProcessor(); }
