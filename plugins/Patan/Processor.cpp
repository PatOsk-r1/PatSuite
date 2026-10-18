#include "plugins/Patan/Processor.h"

#include "ui/pan/PatanEditor.h"

using namespace patan;

namespace {

juce::String pct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray divisions;
    for (int i = 0; i < pat::dsp::PanEngine::kNumDivisions; ++i) divisions.add(pat::dsp::PanEngine::divisionName(i));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kMode, 1}, "Mode", juce::StringArray{"Pan", "Volume", "Width"}, 0));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kSync, 1}, "Tempo Sync", true));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kDivision, 1}, "Rate (sync)", divisions, 2));
    juce::NormalisableRange<float> rate(0.02f, 20.0f);
    rate.setSkewForCentre(1.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kRate, 1}, "Rate (Hz)", rate, 0.5f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, v < 1 ? 2 : 1) + " Hz"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kDepth, 1}, "Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kPhase, 1}, "Phase", juce::NormalisableRange<float>(0.0f, 360.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + juce::String::fromUTF8("°"); })));
    juce::NormalisableRange<float> smooth(0.0f, 200.0f);
    smooth.setSkewForCentre(20.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kSmooth, 1}, "Smooth", smooth, 5.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kCenter, 1}, "Center", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) {
                                  const int p = juce::roundToInt(std::abs(v) * 100.0f);
                                  return p == 0 ? juce::String("C") : juce::String(p) + (v < 0 ? " L" : " R");
                              })));
    l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{kLaw, 1}, "Pan Law",
                                                       juce::StringArray{"Balance", "-3 dB", "-4.5 dB", "-6 dB"}, 0));
    l.add(std::make_unique<F>(juce::ParameterID{kWidth, 1}, "Stereo Width", juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kMix, 1}, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return pct(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; })));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kInvert, 1}, "Invert", false));
    return l;
}

}  // namespace

PatanProcessor::PatanProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    const char* ids[] = {kMode, kSync, kDivision, kRate, kDepth, kPhase, kSmooth, kCenter, kLaw, kWidth, kMix, kOutput, kInvert};
    for (std::size_t i = 0; i < p_.size(); ++i) p_[i] = state.getRawParameterValue(ids[i]);
    curveChanged();
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "Patan", factoryPresets());
    // The drawn curve is not a parameter: it travels in presets as an extra.
    presets->saveExtras = [this](juce::XmlElement& x) {
        auto* e = x.createNewChildElement("Extra");
        e->setAttribute("key", "curve");
        e->setAttribute("value", juce::String(curve_.toString()));
    };
    presets->loadExtra = [this](const juce::String& key, const juce::String& value) {
        if (key == "curve") setCurveText(value);
    };
}

void PatanProcessor::curveChanged() {
    curve_.render(table_.writeSlot());
    table_.publish();
    state.state.setProperty("curve", juce::String(curve_.toString()), nullptr);
    ++curveVersion_;
}

void PatanProcessor::setCurveText(const juce::String& text) {
    if (curve_.fromString(text.toStdString())) curveChanged();
}

bool PatanProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void PatanProcessor::prepareToPlay(double sr, int) { engine_.prepare(sr); }

void PatanProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const auto v = [this](int i) { return p_[static_cast<std::size_t>(i)]->load(std::memory_order_relaxed); };
    pat::dsp::PanSettings s;
    s.mode = juce::roundToInt(v(0));
    s.sync = v(1) > 0.5f;
    s.division = juce::roundToInt(v(2));
    s.rateHz = v(3);
    s.depth = v(4);
    s.phaseDeg = v(5);
    s.smoothMs = v(6);
    s.center = v(7);
    s.law = juce::roundToInt(v(8));
    s.stereoWidth = v(9);
    s.mix = v(10);
    s.outputDb = v(11);
    s.invert = v(12) > 0.5f;

    pat::dsp::PanTransport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            t.playing = pos->getIsPlaying();
        }
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    engine_.process(l, r, buffer.getNumSamples(), table_.read(), s, t);
    phase.store(engine_.phase(), std::memory_order_relaxed);
    position.store(engine_.position(), std::memory_order_relaxed);
    peakL.store(engine_.outputPeak(0), std::memory_order_relaxed);
    peakR.store(engine_.outputPeak(1), std::memory_order_relaxed);
    mode.store(s.mode, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* PatanProcessor::createEditor() { return new PatanEditor(*this); }

void PatanProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void PatanProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) {
            state.replaceState(juce::ValueTree::fromXml(*xml));
            setCurveText(state.state.getProperty("curve").toString());
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PatanProcessor(); }
