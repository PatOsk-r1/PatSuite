#include "plugins/PatDeEsser/Processor.h"

#include "ui/deesser/DeEsserEditor.h"

using namespace deesser;

namespace {

juce::String hzText(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::NormalisableRange<float> freq(2000.0f, 14000.0f);
    freq.setSkewForCentre(6000.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kFreq, 1}, "Frequency", freq, 6500.0f,
                              A().withStringFromValueFunction([](float v, int) { return hzText(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kWidth, 1}, "Bandwidth", juce::NormalisableRange<float>(0.3f, 2.5f), 1.17f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + " oct"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kThreshold, 1}, "Threshold", juce::NormalisableRange<float>(-60.0f, 0.0f), -27.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));
    juce::NormalisableRange<float> ratio(1.0f, 20.0f);
    ratio.setSkewForCentre(5.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kRatio, 1}, "Ratio", ratio, 6.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + ":1"; })));
    juce::NormalisableRange<float> rel(5.0f, 300.0f);
    rel.setSkewForCentre(60.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kRelease, 1}, "Release", rel, 60.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; })));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kMode, 1}, "Mode", juce::StringArray{"Split band", "Wide band"}, 0));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kListen, 1}, "Listen", false));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; })));
    return l;
}

}  // namespace

DeEsserProcessor::DeEsserProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    freq_ = state.getRawParameterValue(kFreq);
    width_ = state.getRawParameterValue(kWidth);
    threshold_ = state.getRawParameterValue(kThreshold);
    ratio_ = state.getRawParameterValue(kRatio);
    release_ = state.getRawParameterValue(kRelease);
    mode_ = state.getRawParameterValue(kMode);
    listen_ = state.getRawParameterValue(kListen);
    output_ = state.getRawParameterValue(kOutput);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatDeEsser", factoryPresets());
}

bool DeEsserProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void DeEsserProcessor::prepareToPlay(double sr, int) {
    sampleRate.store(sr);
    engine_.prepare(sr);
}

void DeEsserProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    // Spectrum shows the input (what the de-esser listens to).
    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, static_cast<int>(mono_.size()));
        for (int i = 0; i < chunk; ++i) mono_[static_cast<std::size_t>(i)] = r ? 0.5f * (l[done + i] + r[done + i]) : l[done + i];
        analysis.push(mono_.data(), static_cast<std::size_t>(chunk));
        done += chunk;
    }

    pat::dsp::DeEsserSettings s;
    s.freqHz = freq_->load(std::memory_order_relaxed);
    s.widthOct = width_->load(std::memory_order_relaxed);
    s.thresholdDb = threshold_->load(std::memory_order_relaxed);
    s.ratio = ratio_->load(std::memory_order_relaxed);
    s.releaseMs = release_->load(std::memory_order_relaxed);
    s.split = mode_->load(std::memory_order_relaxed) < 0.5f;
    s.listen = listen_->load(std::memory_order_relaxed) > 0.5f;
    s.outputDb = output_->load(std::memory_order_relaxed);
    engine_.process(l, r, n, s);

    detectorDb.store(engine_.detectorDb(), std::memory_order_relaxed);
    reductionDb.store(engine_.reductionDb(), std::memory_order_relaxed);
    inPeak.store(engine_.inputPeak(), std::memory_order_relaxed);
    outPeak.store(engine_.outputPeak(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DeEsserProcessor::createEditor() { return new DeEsserEditor(*this); }

void DeEsserProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void DeEsserProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DeEsserProcessor(); }
