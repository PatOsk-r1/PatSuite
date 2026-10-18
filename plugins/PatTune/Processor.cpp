#include "plugins/PatTune/Processor.h"

#include "dsp/tune/Scales.h"
#include "ui/tune/TuneEditor.h"

using namespace tune;

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    const auto pct = [](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; };
    juce::StringArray keys, scales;
    for (int k = 0; k < 12; ++k) keys.add(pat::dsp::keyName(k));
    for (int s = 0; s < pat::dsp::numScales(); ++s) scales.add(pat::dsp::scale(s).name);
    juce::NormalisableRange<float> speed(0.0f, 400.0f);
    speed.setSkewForCentre(60.0f);
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kAuto, 1}, "Auto", true));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kKey, 1}, "Key", keys, 0));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kScale, 1}, "Scale", scales, 1));
    l.add(std::make_unique<F>(juce::ParameterID{kSpeed, 1}, "Retune Speed", speed, 40.0f,
                              A().withStringFromValueFunction([](float v, int) { return v < 0.5f ? juce::String("Instant") : juce::String(juce::roundToInt(v)) + " ms"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kHumanize, 1}, "Humanize", juce::NormalisableRange<float>(0.0f, 1.0f), 0.3f, A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kFormant, 1}, "Formant", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " st"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kMix, 1}, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f, A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kGate, 1}, "Confidence Gate", juce::NormalisableRange<float>(0.3f, 0.95f), 0.6f, A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kA4, 1}, "A4 Reference", juce::NormalisableRange<float>(415.0f, 466.0f), 440.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " Hz"; })));
    return l;
}

}  // namespace

TuneProcessor::TuneProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()),
      customMask_(pat::dsp::kDefaultCustomMask) {
    const char* ids[] = {kAuto, kKey, kScale, kSpeed, kHumanize, kFormant, kMix, kGate, kA4};
    for (std::size_t i = 0; i < p_.size(); ++i) p_[i] = state.getRawParameterValue(ids[i]);
    state.state.setProperty("customMask", static_cast<int>(pat::dsp::kDefaultCustomMask), nullptr);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatTune", factoryPresets());
    // The drawn custom scale is not a parameter: it travels in user presets as an extra.
    presets->saveExtras = [this](juce::XmlElement& x) {
        auto* e = x.createNewChildElement("Extra");
        e->setAttribute("key", "customMask");
        e->setAttribute("value", juce::String(static_cast<int>(customMask())));
    };
    presets->loadExtra = [this](const juce::String& key, const juce::String& value) {
        if (key == "customMask") setCustomMask(static_cast<std::uint32_t>(value.getIntValue()));
    };
}

void TuneProcessor::setCustomMask(std::uint32_t mask) {
    customMask_.store(mask & 0xFFFFFFu);
    state.state.setProperty("customMask", static_cast<int>(mask & 0xFFFFFFu), nullptr);
}

bool TuneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void TuneProcessor::prepareToPlay(double sr, int) {
    engine_.prepare(sr);
    setLatencySamples(engine_.latencySamples());
}

void TuneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const auto v = [this](int i) { return p_[static_cast<std::size_t>(i)]->load(std::memory_order_relaxed); };
    pat::dsp::TuneSettings s;
    s.autoOn = v(0) > 0.5f;
    s.key = juce::roundToInt(v(1));
    s.scale = juce::roundToInt(v(2));
    s.retuneMs = v(3);
    s.humanize = v(4);
    s.formantSt = v(5);
    s.mix = v(6);
    s.gate = v(7);
    s.a4Hz = v(8);
    s.customMask = customMask_.load(std::memory_order_relaxed);
    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, 8192);
        const int k = engine_.process(l + done, r ? r + done : nullptr, chunk, s, points_.data(), static_cast<int>(points_.size()));
        history.push(points_.data(), static_cast<std::size_t>(k));
        done += chunk;
    }
    const auto last = engine_.last();
    clarity.store(last.clarity, std::memory_order_relaxed);
    detectedHz.store(last.detectedHz, std::memory_order_relaxed);
    targetHz.store(last.targetHz, std::memory_order_relaxed);
    outputHz.store(last.outputHz, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* TuneProcessor::createEditor() { return new TuneEditor(*this); }

void TuneProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void TuneProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) {
            state.replaceState(juce::ValueTree::fromXml(*xml));
            customMask_.store(static_cast<std::uint32_t>(static_cast<int>(state.state.getProperty("customMask", static_cast<int>(pat::dsp::kDefaultCustomMask)))) & 0xFFFFFFu);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TuneProcessor(); }
