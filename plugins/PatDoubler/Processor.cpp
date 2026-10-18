#include "plugins/PatDoubler/Processor.h"

#include "ui/doubler/DoublerEditor.h"

using namespace doubler;

namespace {

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    const pat::dsp::DoublerSettings defaults;
    const auto pct = [](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; };
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kCopies, 1}, "Copies", juce::StringArray{"1", "2", "3", "4"}, 3));
    for (int c = 0; c < pat::dsp::DoublerSettings::kMaxCopies; ++c) {
        const auto sc = static_cast<std::size_t>(c);
        const juce::String n = " " + juce::String(c + 1);
        l.add(std::make_unique<F>(juce::ParameterID{delayId(c), 1}, "Delay" + n, juce::NormalisableRange<float>(1.0f, 80.0f), defaults.delayMs[sc],
                                  A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })));
        l.add(std::make_unique<F>(juce::ParameterID{detuneId(c), 1}, "Detune" + n, juce::NormalisableRange<float>(-50.0f, 50.0f), defaults.detuneCents[sc],
                                  A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(juce::roundToInt(v)) + " ct"; })));
        l.add(std::make_unique<F>(juce::ParameterID{panId(c), 1}, "Pan" + n, juce::NormalisableRange<float>(-1.0f, 1.0f), defaults.pan[sc],
                                  A().withStringFromValueFunction([](float v, int) {
                                      return std::abs(v) < 0.01f ? juce::String("C") : juce::String(juce::roundToInt(std::abs(v) * 100.0f)) + (v < 0 ? " L" : " R");
                                  })));
    }
    l.add(std::make_unique<F>(juce::ParameterID{kVariation, 1}, "Timing Variation", juce::NormalisableRange<float>(0.0f, 1.0f), defaults.variation,
                              A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kSpread, 1}, "Stereo Spread", juce::NormalisableRange<float>(0.0f, 1.5f), defaults.spread,
                              A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kWidth, 1}, "Width", juce::NormalisableRange<float>(0.0f, 1.0f), defaults.width, A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kThickness, 1}, "Thickness", juce::NormalisableRange<float>(0.0f, 1.0f), defaults.thickness,
                              A().withStringFromValueFunction(pct)));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; })));
    return l;
}

}  // namespace

DoublerProcessor::DoublerProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    copies_ = state.getRawParameterValue(kCopies);
    variation_ = state.getRawParameterValue(kVariation);
    spread_ = state.getRawParameterValue(kSpread);
    width_ = state.getRawParameterValue(kWidth);
    thickness_ = state.getRawParameterValue(kThickness);
    output_ = state.getRawParameterValue(kOutput);
    for (int c = 0; c < 4; ++c) {
        delay_[static_cast<std::size_t>(c)] = state.getRawParameterValue(delayId(c));
        detune_[static_cast<std::size_t>(c)] = state.getRawParameterValue(detuneId(c));
        pan_[static_cast<std::size_t>(c)] = state.getRawParameterValue(panId(c));
    }
}

bool DoublerProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet(), in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();  // mono in, stereo out is the classic use
}

void DoublerProcessor::prepareToPlay(double sr, int) {
    sampleRate.store(sr);
    engine_.prepare(sr);  // allocates the delay line (not on the audio thread)
}

pat::dsp::DoublerSettings DoublerProcessor::settings() const noexcept {
    const auto v = [](const std::atomic<float>* p) { return p->load(std::memory_order_relaxed); };
    pat::dsp::DoublerSettings s;
    s.copies = juce::roundToInt(v(copies_)) + 1;
    for (std::size_t c = 0; c < 4; ++c) {
        s.delayMs[c] = v(delay_[c]);
        s.detuneCents[c] = v(detune_[c]);
        s.pan[c] = v(pan_[c]);
    }
    s.variation = v(variation_);
    s.spread = v(spread_);
    s.width = v(width_);
    s.thickness = v(thickness_);
    s.outputDb = v(output_);
    return s;
}

void DoublerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    // Mono in, stereo out: copy the input to the right channel first.
    if (ins == 1 && outs > 1) buffer.copyFrom(1, 0, buffer, 0, 0, n);
    float* l = buffer.getWritePointer(0);
    float* r = outs > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, static_cast<int>(mono_.size()));
        for (int i = 0; i < chunk; ++i) mono_[static_cast<std::size_t>(i)] = r ? 0.5f * (l[done + i] + r[done + i]) : l[done + i];
        wave.push(mono_.data(), static_cast<std::size_t>(chunk));
        done += chunk;
    }

    const auto s = settings();
    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, 4096);
        const int k = engine_.process(l + done, r ? r + done : nullptr, chunk, s, scopeBuf_.data(), static_cast<int>(scopeBuf_.size()));
        scope.push(scopeBuf_.data(), static_cast<std::size_t>(k));
        done += chunk;
    }
}

juce::AudioProcessorEditor* DoublerProcessor::createEditor() { return new DoublerEditor(*this); }

void DoublerProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void DoublerProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DoublerProcessor(); }
