#include "plugins/PatPatPat/Processor.h"

#include "ui/delay/PatPatPatEditor.h"

using namespace patpatpat;

namespace {

juce::String pct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
juce::String hzText(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }
juce::String msText(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, 2) + " s" : juce::String(juce::roundToInt(v)) + " ms"; }

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using C = juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray divisions;
    for (int i = 0; i < pat::dsp::DelayEngine::kNumDivisions; ++i) divisions.add(pat::dsp::DelayEngine::divisionName(i));
    const auto skewed = [](float lo, float hi, float centre) {
        juce::NormalisableRange<float> r(lo, hi);
        r.setSkewForCentre(centre);
        return r;
    };
    l.add(std::make_unique<C>(juce::ParameterID{kMode, 1}, "Mode", juce::StringArray{"Mono", "Stereo", "Ping-Pong", "Cross"}, 2));
    l.add(std::make_unique<C>(juce::ParameterID{kCharacter, 1}, "Character", juce::StringArray{"Clean", "Tape", "Analog", "Lo-Fi"}, 0));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kSync, 1}, "Tempo Sync", true));
    l.add(std::make_unique<C>(juce::ParameterID{kDivL, 1}, "Time L (sync)", divisions, 8));
    l.add(std::make_unique<C>(juce::ParameterID{kDivR, 1}, "Time R (sync)", divisions, 8));
    l.add(std::make_unique<F>(juce::ParameterID{kTimeL, 1}, "Time L (ms)", skewed(1.0f, 4000.0f, 400.0f), 375.0f,
                              A().withStringFromValueFunction([](float v, int) { return msText(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kTimeR, 1}, "Time R (ms)", skewed(1.0f, 4000.0f, 400.0f), 375.0f,
                              A().withStringFromValueFunction([](float v, int) { return msText(v); })));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kLink, 1}, "Link L/R", true));
    l.add(std::make_unique<F>(juce::ParameterID{kFeedback, 1}, "Feedback", juce::NormalisableRange<float>(0.0f, 1.0f), 0.45f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kLowCut, 1}, "Low Cut", skewed(20.0f, 2000.0f, 200.0f), 80.0f,
                              A().withStringFromValueFunction([](float v, int) { return hzText(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kHighCut, 1}, "High Cut", skewed(1000.0f, 20000.0f, 6000.0f), 9000.0f,
                              A().withStringFromValueFunction([](float v, int) { return hzText(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kDrive, 1}, "Drive", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kModRate, 1}, "Mod Rate", skewed(0.05f, 10.0f, 1.0f), 0.6f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + " Hz"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kModDepth, 1}, "Mod Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kDuck, 1}, "Ducking", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kFreeze, 1}, "Freeze", false));
    l.add(std::make_unique<F>(juce::ParameterID{kWidth, 1}, "Width", juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kDiffusion, 1}, "Diffusion", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kMix, 1}, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 0.35f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; })));
    return l;
}

}  // namespace

PatPatPatProcessor::PatPatPatProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    const char* ids[] = {kMode,    kCharacter, kSync,     kDivL,     kDivR, kTimeL,   kTimeR,  kLink,      kFeedback, kLowCut,
                         kHighCut, kDrive,     kModRate,  kModDepth, kDuck, kFreeze,  kWidth,  kDiffusion, kMix,      kOutput};
    for (std::size_t i = 0; i < p_.size(); ++i) p_[i] = state.getRawParameterValue(ids[i]);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatPatPat", factoryPresets());
}

pat::dsp::DelaySettings PatPatPatProcessor::settings() const noexcept {
    const auto v = [this](int i) { return p_[static_cast<std::size_t>(i)]->load(std::memory_order_relaxed); };
    pat::dsp::DelaySettings s;
    s.mode = juce::roundToInt(v(0));
    s.character = juce::roundToInt(v(1));
    s.sync = v(2) > 0.5f;
    s.divisionL = juce::roundToInt(v(3));
    s.divisionR = juce::roundToInt(v(4));
    s.timeLMs = v(5);
    s.timeRMs = v(6);
    s.link = v(7) > 0.5f;
    s.feedback = v(8);
    s.lowCutHz = v(9);
    s.highCutHz = v(10);
    s.drive = v(11);
    s.modRateHz = v(12);
    s.modDepth = v(13);
    s.duck = v(14);
    s.freeze = v(15) > 0.5f;
    s.width = v(16);
    s.diffusion = v(17);
    s.mix = v(18);
    s.outputDb = v(19);
    return s;
}

bool PatPatPatProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void PatPatPatProcessor::prepareToPlay(double sr, int) { engine_.prepare(sr); }

void PatPatPatProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    double tempo = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) tempo = *b;
    bpm.store(tempo, std::memory_order_relaxed);
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    engine_.process(l, r, buffer.getNumSamples(), settings(), tempo);
    inputLevel.store(engine_.inputLevel(), std::memory_order_relaxed);
    peakL.store(engine_.outputPeak(0), std::memory_order_relaxed);
    peakR.store(engine_.outputPeak(1), std::memory_order_relaxed);
    duckGain.store(engine_.duckGain(), std::memory_order_relaxed);
    echoL.store(engine_.echoPeak(0), std::memory_order_relaxed);
    echoR.store(engine_.echoPeak(1), std::memory_order_relaxed);
    onsetLevel.store(engine_.onsetLevel(), std::memory_order_relaxed);
    onsets.store(engine_.onsets(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* PatPatPatProcessor::createEditor() { return new PatPatPatEditor(*this); }

void PatPatPatProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void PatPatPatProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PatPatPatProcessor(); }
