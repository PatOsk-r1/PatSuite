#include "plugins/PatSidechain/Processor.h"

#include "ui/sidechain/SidechainEditor.h"

using namespace sidechain;

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    const auto ms = [](float v, int) { return v < 10.0f ? juce::String(v, 1) + " ms" : juce::String(juce::roundToInt(v)) + " ms"; };
    const auto db = [](float v, int) { return juce::String(v, 1) + " dB"; };
    const auto hz = [](float v, int) { return v >= 1000.0f ? juce::String(v / 1000.0f, 1) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; };
    const auto skewed = [](float lo, float hi, float centre) {
        juce::NormalisableRange<float> r(lo, hi);
        r.setSkewForCentre(centre);
        return r;
    };
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kTrigger, 1}, "Trigger", juce::StringArray{"Auto", "Tempo", "Sidechain"}, 0));
    l.add(std::make_unique<F>(juce::ParameterID{kPump, 1}, "Pump", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kRate, 1}, "Rate", juce::StringArray{"1/4", "1/8", "1/16"}, 0));
    l.add(std::make_unique<F>(juce::ParameterID{kThreshold, 1}, "Threshold", juce::NormalisableRange<float>(-60.0f, 0.0f), -24.0f,
                              A().withStringFromValueFunction(db)));
    l.add(std::make_unique<F>(juce::ParameterID{kAttack, 1}, "Attack", skewed(0.1f, 50.0f, 5.0f), 2.0f, A().withStringFromValueFunction(ms)));
    l.add(std::make_unique<F>(juce::ParameterID{kRelease, 1}, "Release", skewed(10.0f, 1000.0f, 180.0f), 180.0f, A().withStringFromValueFunction(ms)));
    l.add(std::make_unique<F>(juce::ParameterID{kKnee, 1}, "Knee", juce::NormalisableRange<float>(0.0f, 12.0f), 6.0f, A().withStringFromValueFunction(db)));
    l.add(std::make_unique<F>(juce::ParameterID{kRatio, 1}, "Ratio", skewed(1.0f, 20.0f, 6.0f), 8.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + ":1"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kScLow, 1}, "SC Low Cut", skewed(20.0f, 500.0f, 80.0f), 20.0f, A().withStringFromValueFunction(hz)));
    l.add(std::make_unique<F>(juce::ParameterID{kScHigh, 1}, "SC High Cut", skewed(60.0f, 20000.0f, 600.0f), 150.0f, A().withStringFromValueFunction(hz)));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kListen, 1}, "Listen Sidechain", false));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; })));
    return l;
}

}  // namespace

SidechainProcessor::SidechainProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    const char* ids[] = {kTrigger, kPump, kRate, kThreshold, kAttack, kRelease, kKnee, kRatio, kScLow, kScHigh, kListen, kOutput};
    for (std::size_t i = 0; i < p_.size(); ++i) p_[i] = state.getRawParameterValue(ids[i]);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatSidechain", factoryPresets());
}

bool SidechainProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;
    if (layouts.inputBuses.size() > 1) {
        const auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void SidechainProcessor::prepareToPlay(double sr, int) {
    sampleRate.store(sr);
    engine_.prepare(sr);
}

void SidechainProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto main = getBusBuffer(buffer, true, 0);
    float* l = main.getWritePointer(0);
    float* r = main.getNumChannels() > 1 ? main.getWritePointer(1) : nullptr;
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled()) {
        auto sc = getBusBuffer(buffer, true, 1);
        if (sc.getNumChannels() > 0) {
            scL = sc.getReadPointer(0);
            scR = sc.getNumChannels() > 1 ? sc.getReadPointer(1) : scL;
        }
    }
    sidechainConnected.store(scL != nullptr, std::memory_order_relaxed);

    const auto v = [this](int i) { return p_[static_cast<std::size_t>(i)]->load(std::memory_order_relaxed); };
    pat::dsp::SidechainSettings s;
    s.trigger = juce::roundToInt(v(0));
    s.pump = v(1);
    s.rate = juce::roundToInt(v(2));
    s.thresholdDb = v(3);
    s.attackMs = v(4);
    s.releaseMs = v(5);
    s.kneeDb = v(6);
    s.ratio = v(7);
    s.scLowCutHz = v(8);
    s.scHighCutHz = v(9);
    s.listenSidechain = v(10) > 0.5f;
    s.outputDb = v(11);

    pat::dsp::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            t.playing = pos->getIsPlaying();
        }

    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, 8192);
        const int k = engine_.process(l + done, r ? r + done : nullptr, scL ? scL + done : nullptr, scR ? scR + done : nullptr, chunk, s, t,
                                      points_.data(), static_cast<int>(points_.size()));
        graph.push(points_.data(), static_cast<std::size_t>(k));
        t.ppq += chunk * t.bpm / 60.0 / sampleRate.load(std::memory_order_relaxed);
        done += chunk;
    }
    minGain.store(engine_.minGain(), std::memory_order_relaxed);
    triggerDb.store(engine_.triggerDb(), std::memory_order_relaxed);
    triggers.store(engine_.triggers(), std::memory_order_relaxed);
    usingSidechain.store(engine_.usingSidechain(), std::memory_order_relaxed);
    sidechainSignal.store(engine_.sidechainSignal(), std::memory_order_relaxed);
    for (int c = main.getNumChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear(c, 0, n);
}

juce::AudioProcessorEditor* SidechainProcessor::createEditor() { return new SidechainEditor(*this); }

void SidechainProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void SidechainProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SidechainProcessor(); }
