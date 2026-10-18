#include "plugins/PatMaster/Processor.h"

#include "ui/master/MasterEditor.h"

using namespace master;

namespace {
juce::String db(float v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; }

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<F>(juce::ParameterID{kGain, 1}, "Gain", juce::NormalisableRange<float>(0.0f, 24.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return db(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kCeiling, 1}, "Ceiling", juce::NormalisableRange<float>(-12.0f, 0.0f), -1.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));
    juce::NormalisableRange<float> rel(1.0f, 1000.0f);
    rel.setSkewForCentre(100.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kRelease, 1}, "Release", rel, 100.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; })));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kAutoRelease, 1}, "Auto Release", true));
    l.add(std::make_unique<F>(juce::ParameterID{kLink, 1}, "Stereo Link", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })));
    l.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{kTruePeak, 1}, "True Peak", true));
    l.add(std::make_unique<F>(juce::ParameterID{kClip, 1}, "Soft Clip", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })));
    return l;
}
}  // namespace

MasterProcessor::MasterProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "PARAMS", layout()) {
    const char* ids[] = {kGain, kCeiling, kRelease, kAutoRelease, kLink, kTruePeak, kClip};
    for (std::size_t i = 0; i < p_.size(); ++i) p_[i] = state.getRawParameterValue(ids[i]);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatMaster", factoryPresets());
}

int MasterProcessor::target() const { return juce::jlimit(0, kNumTargets - 1, static_cast<int>(state.state.getProperty("target", 0))); }
void MasterProcessor::setTarget(int t) { state.state.setProperty("target", juce::jlimit(0, kNumTargets - 1, t), nullptr); }

bool MasterProcessor::matchTarget() {
    const float now = integrated.load();
    if (now <= pat::dsp::LoudnessMeter::kSilence + 1.0f) return false;
    auto* p = state.getParameter(kGain);
    const float gain = juce::jlimit(0.0f, 24.0f, p_[0]->load() + (kTargets[target()].lufs - now));
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(gain));
    p->endChangeGesture();
    if (auto* c = state.getParameter(kCeiling)) {
        c->beginChangeGesture();
        c->setValueNotifyingHost(c->convertTo0to1(kTargets[target()].ceilingDb));
        c->endChangeGesture();
    }
    resetMeter();  // measure again with the new gain
    return true;
}

bool MasterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) && layouts.getMainInputChannelSet() == out;
}

void MasterProcessor::prepareToPlay(double sr, int) {
    limiter_.prepare(sr);
    meter_.prepare(sr);
    setLatencySamples(limiter_.latency());
}

void MasterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    if (resetRequest_.exchange(false)) meter_.reset();
    const int n = buffer.getNumSamples();
    float* l = buffer.getWritePointer(0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const float inPk = buffer.getMagnitude(0, n);

    pat::dsp::LimiterSettings s;
    s.gainDb = p_[0]->load();
    s.ceilingDb = p_[1]->load();
    s.releaseMs = p_[2]->load();
    s.autoRelease = p_[3]->load() > 0.5f;
    s.link = p_[4]->load();
    s.truePeak = p_[5]->load() > 0.5f;
    s.clip = p_[6]->load();
    limiter_.process(l, r, n, s);
    meter_.process(l, r, n);

    momentary.store(meter_.momentary(), std::memory_order_relaxed);
    shortTerm.store(meter_.shortTerm(), std::memory_order_relaxed);
    integrated.store(meter_.integrated(), std::memory_order_relaxed);
    range.store(meter_.range(), std::memory_order_relaxed);
    truePeak.store(meter_.truePeakDb(), std::memory_order_relaxed);
    grDb.store(limiter_.gainReductionDb(), std::memory_order_relaxed);
    outPeak[0].store(buffer.getMagnitude(0, 0, n), std::memory_order_relaxed);
    outPeak[1].store(r ? buffer.getMagnitude(1, 0, n) : buffer.getMagnitude(0, 0, n), std::memory_order_relaxed);

    // History: one point per ~1024 samples.
    histAcc_.in = std::max(histAcc_.in, inPk * std::pow(10.0f, s.gainDb / 20.0f));
    histAcc_.out = std::max(histAcc_.out, buffer.getMagnitude(0, n));
    histAcc_.gr = std::min(histAcc_.gr, limiter_.gainReductionDb());
    histFill_ += n;
    if (histFill_ >= 1024) {
        history.push(&histAcc_, 1);
        histAcc_ = {};
        histFill_ = 0;
    }
}

juce::AudioProcessorEditor* MasterProcessor::createEditor() { return new MasterEditor(*this); }

void MasterProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void MasterProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterProcessor(); }
