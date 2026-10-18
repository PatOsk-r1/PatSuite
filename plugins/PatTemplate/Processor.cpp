#include "plugins/PatTemplate/Processor.h"

#include "ui/template/TemplateEditor.h"

namespace {
juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gain", 1}, "Gain", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction([](float v, int) {
            return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB";
        })));
    return l;
}
}  // namespace

TemplateProcessor::TemplateProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    gain_ = state.getRawParameterValue("gain");
}

bool TemplateProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void TemplateProcessor::prepareToPlay(double sr, int) {
    sampleRate.store(sr);
    engine_.prepare(sr);
}

void TemplateProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    engine_.process(l, r, n, gain_->load(std::memory_order_relaxed));
    inPeak.store(engine_.inputPeak(), std::memory_order_relaxed);
    outPeak.store(engine_.outputPeak(), std::memory_order_relaxed);
    // Analysis feed: never blocks, drops samples if the UI is not reading (editor closed).
    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, static_cast<int>(mono_.size()));
        for (int i = 0; i < chunk; ++i) mono_[static_cast<std::size_t>(i)] = r ? 0.5f * (l[done + i] + r[done + i]) : l[done + i];
        analysis.push(mono_.data(), static_cast<std::size_t>(chunk));
        done += chunk;
    }
}

juce::AudioProcessorEditor* TemplateProcessor::createEditor() { return new TemplateEditor(*this); }

void TemplateProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void TemplateProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TemplateProcessor(); }
