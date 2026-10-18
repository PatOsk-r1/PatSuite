#include "plugins/PatBass/Processor.h"

#include "ui/bass/BassEditor.h"

using pat::dsp::BassEngine;

namespace {
using F = juce::AudioParameterFloat;
using A = juce::AudioParameterFloatAttributes;
using C = juce::AudioParameterChoice;
using B = juce::AudioParameterBool;
using I = juce::AudioParameterInt;

juce::String pct(float v) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
juce::String bip(float v) { return (v > 0 ? "+" : "") + juce::String(juce::roundToInt(v * 100.0f)) + " %"; }
juce::String secs(float v) { return v < 1.0f ? juce::String(juce::roundToInt(v * 1000.0f)) + " ms" : juce::String(v, 2) + " s"; }
juce::String hz(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, 2) + " kHz" : juce::String(juce::roundToInt(v)) + " Hz"; }

juce::NormalisableRange<float> skewed(float lo, float hi, float centre) {
    juce::NormalisableRange<float> r(lo, hi);
    r.setSkewForCentre(centre);
    return r;
}

void addF(juce::AudioProcessorValueTreeState::ParameterLayout& l, const char* id, const char* name, juce::NormalisableRange<float> r, float def,
          std::function<juce::String(float)> text) {
    l.add(std::make_unique<F>(juce::ParameterID{id, 1}, name, r, def, A().withStringFromValueFunction([text](float v, int) { return text(v); })));
}

juce::StringArray names(const char* (*fn)(int), int n) {
    juce::StringArray a;
    for (int i = 0; i < n; ++i) a.add(fn(i));
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    const auto lin = [](float a, float b) { return juce::NormalisableRange<float>(a, b); };
    const auto time = skewed(0.0005f, 5.0f, 0.3f);
    // OSC A
    addF(l, "a_shape", "Osc A Shape", lin(0, 1), 0.66f, [](float v) {
        static const char* n[] = {"Sine", "Triangle", "Saw", "Square"};
        return juce::String(n[juce::jlimit(0, 3, juce::roundToInt(v * 3.0f))]) + " " + juce::String(juce::roundToInt(v * 100.0f));
    });
    addF(l, "a_pw", "Pulse Width", lin(0.05f, 0.95f), 0.5f, pct);
    addF(l, "a_fm", "FM Growl", lin(0, 1), 0.0f, pct);
    l.add(std::make_unique<C>(juce::ParameterID{"a_fmratio", 1}, "FM Ratio", names(&BassEngine::fmRatioName, BassEngine::kNumFmRatios), 1));
    l.add(std::make_unique<I>(juce::ParameterID{"unison", 1}, "Unison", 1, 7, 1));
    addF(l, "detune", "Detune", lin(0, 1), 0.2f, pct);
    addF(l, "spread", "Stereo Spread", lin(0, 1), 0.6f, pct);
    addF(l, "a_level", "Osc A Level", lin(0, 1), 0.8f, pct);
    // OSC B
    l.add(std::make_unique<B>(juce::ParameterID{"b_on", 1}, "Osc B On", false));
    addF(l, "b_shape", "Osc B Shape", lin(0, 1), 0.66f, pct);
    l.add(std::make_unique<I>(juce::ParameterID{"b_oct", 1}, "Osc B Octave", -2, 2, 0));
    l.add(std::make_unique<I>(juce::ParameterID{"b_semi", 1}, "Osc B Semitones", -12, 12, 0));
    addF(l, "b_fine", "Osc B Fine", lin(-100, 100), 0.0f, [](float v) { return juce::String(juce::roundToInt(v)) + " ct"; });
    addF(l, "b_level", "Osc B Level", lin(0, 1), 0.6f, pct);
    // SUB / NOISE / PUNCH
    l.add(std::make_unique<B>(juce::ParameterID{"sub_on", 1}, "Sub On", true));
    l.add(std::make_unique<C>(juce::ParameterID{"sub_oct", 1}, "Sub Octave", juce::StringArray{"-1 oct", "-2 oct"}, 0));
    l.add(std::make_unique<C>(juce::ParameterID{"sub_shape", 1}, "Sub Shape", juce::StringArray{"Sine", "Triangle"}, 0));
    addF(l, "sub_level", "Sub Level", lin(0, 1), 0.6f, pct);
    addF(l, "noise", "Noise", lin(0, 1), 0.0f, pct);
    addF(l, "punch", "Punch (pitch drop)", lin(0, 48), 0.0f, [](float v) { return "+" + juce::String(v, 1) + " st"; });
    addF(l, "punch_time", "Punch Time", skewed(5, 500, 60), 60.0f, [](float v) { return juce::String(juce::roundToInt(v)) + " ms"; });
    addF(l, "click", "Click", lin(0, 1), 0.0f, pct);
    // VOICE
    l.add(std::make_unique<B>(juce::ParameterID{"mono", 1}, "Mono", true));
    l.add(std::make_unique<B>(juce::ParameterID{"legato", 1}, "Legato", true));
    addF(l, "glide", "Glide", skewed(0, 1000, 120), 0.0f, [](float v) { return juce::String(juce::roundToInt(v)) + " ms"; });
    addF(l, "bend", "Bend Range", lin(0, 12), 2.0f, [](float v) { return juce::String(juce::roundToInt(v)) + " st"; });
    addF(l, "velocity", "Velocity", lin(0, 1), 0.5f, pct);
    // FILTER
    l.add(std::make_unique<C>(juce::ParameterID{"f_type", 1}, "Filter Type", names(&BassEngine::filterTypeName, BassEngine::kNumFilterTypes), 0));
    addF(l, "cutoff", "Cutoff", skewed(20, 20000, 800), 1200.0f, hz);
    addF(l, "reso", "Resonance", lin(0, 1), 0.2f, pct);
    addF(l, "f_drive", "Filter Drive", lin(0, 1), 0.0f, pct);
    addF(l, "f_env", "Filter Env", lin(-1, 1), 0.3f, bip);
    addF(l, "keytrack", "Key Track", lin(0, 1), 0.3f, pct);
    addF(l, "vowel", "Vowel", lin(0, 1), 0.0f, [](float v) {
        static const char* n[] = {"A", "E", "I", "O", "U"};
        return juce::String(n[juce::jlimit(0, 4, juce::roundToInt(v * 4.0f))]);
    });
    addF(l, "fe_a", "Filter Attack", time, 0.002f, secs);
    addF(l, "fe_d", "Filter Decay", time, 0.3f, secs);
    addF(l, "fe_s", "Filter Sustain", lin(0, 1), 0.0f, pct);
    addF(l, "fe_r", "Filter Release", time, 0.2f, secs);
    addF(l, "ae_a", "Amp Attack", time, 0.002f, secs);
    addF(l, "ae_d", "Amp Decay", time, 0.5f, secs);
    addF(l, "ae_s", "Amp Sustain", lin(0, 1), 1.0f, pct);
    addF(l, "ae_r", "Amp Release", time, 0.15f, secs);
    // LFO
    l.add(std::make_unique<C>(juce::ParameterID{"lfo_shape", 1}, "LFO Shape", names(&BassEngine::lfoShapeName, BassEngine::kNumLfoShapes), 0));
    l.add(std::make_unique<B>(juce::ParameterID{"lfo_sync", 1}, "LFO Sync", true));
    l.add(std::make_unique<C>(juce::ParameterID{"lfo_div", 1}, "LFO Rate (sync)", names(&BassEngine::lfoDivisionName, BassEngine::kNumLfoDivisions), 3));
    addF(l, "lfo_rate", "LFO Rate (Hz)", skewed(0.05f, 20, 2), 2.0f, [](float v) { return juce::String(v, 2) + " Hz"; });
    l.add(std::make_unique<B>(juce::ParameterID{"lfo_retrig", 1}, "LFO Retrigger", true));
    addF(l, "lfo_cut", "LFO > Cutoff (Wobble)", lin(-1, 1), 0.0f, bip);
    addF(l, "lfo_pitch", "LFO > Pitch", lin(-1, 1), 0.0f, bip);
    addF(l, "lfo_shapeamt", "LFO > Shape", lin(-1, 1), 0.0f, bip);
    addF(l, "lfo_vol", "LFO > Volume", lin(0, 1), 0.0f, pct);
    addF(l, "lfo_vowel", "LFO > Vowel", lin(-1, 1), 0.0f, bip);
    // FX
    l.add(std::make_unique<C>(juce::ParameterID{"dist_type", 1}, "Distortion", names(&BassEngine::distTypeName, BassEngine::kNumDistTypes), 0));
    addF(l, "drive", "Drive", lin(0, 1), 0.0f, pct);
    addF(l, "dist_mix", "Distortion Mix", lin(0, 1), 1.0f, pct);
    l.add(std::make_unique<B>(juce::ParameterID{"clean_lows", 1}, "Keep Lows Clean", true));
    addF(l, "split", "Clean Below", skewed(40, 300, 120), 120.0f, hz);
    addF(l, "glue", "Glue", lin(0, 1), 0.0f, pct);
    addF(l, "width", "Width", lin(0, 1), 0.5f, pct);
    addF(l, "volume", "Volume", lin(-24, 6), 0.0f, [](float v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; });
    return l;
}
}  // namespace

juce::StringArray bass::parameterIds() {
    return {"a_shape", "a_pw", "a_fm", "a_fmratio", "unison", "detune", "spread", "a_level", "b_on", "b_shape", "b_oct", "b_semi", "b_fine",
            "b_level", "sub_on", "sub_oct", "sub_shape", "sub_level", "noise", "punch", "punch_time", "click", "mono", "legato", "glide",
            "bend", "velocity", "f_type", "cutoff", "reso", "f_drive", "f_env", "keytrack", "vowel", "fe_a", "fe_d", "fe_s", "fe_r",
            "ae_a", "ae_d", "ae_s", "ae_r", "lfo_shape", "lfo_sync", "lfo_div", "lfo_rate", "lfo_retrig", "lfo_cut", "lfo_pitch",
            "lfo_shapeamt", "lfo_vol", "lfo_vowel", "dist_type", "drive", "dist_mix", "clean_lows", "split", "glue", "width", "volume"};
}

BassProcessor::BassProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)), state(*this, nullptr, "PARAMS", layout()) {
    for (const auto& id : bass::parameterIds()) raw_[id] = state.getRawParameterValue(id);
    mono_.assign(8192, 0.0f);
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatBass", factoryPresets());
}

pat::dsp::BassSettings BassProcessor::settings() const {
    const auto v = [this](const char* id) { return raw_.at(id)->load(); };
    const auto b = [&](const char* id) { return v(id) > 0.5f; };
    const auto i = [&](const char* id) { return juce::roundToInt(v(id)); };
    pat::dsp::BassSettings s;
    s.aShape = v("a_shape");
    s.aPw = v("a_pw");
    s.aFm = v("a_fm");
    s.aFmRatio = i("a_fmratio");
    s.unison = i("unison");
    s.detune = v("detune");
    s.spread = v("spread");
    s.aLevel = v("a_level");
    s.bOn = b("b_on");
    s.bShape = v("b_shape");
    s.bOct = i("b_oct");
    s.bSemi = i("b_semi");
    s.bFine = v("b_fine");
    s.bLevel = v("b_level");
    s.subOn = b("sub_on");
    s.subOct = i("sub_oct") + 1;
    s.subShape = i("sub_shape");
    s.subLevel = v("sub_level");
    s.noise = v("noise");
    s.punchSemis = v("punch");
    s.punchMs = v("punch_time");
    s.click = v("click");
    s.mono = b("mono");
    s.legato = b("legato");
    s.glideMs = v("glide");
    s.bendRange = v("bend");
    s.velocity = v("velocity");
    s.filterType = i("f_type");
    s.cutoff = v("cutoff");
    s.reso = v("reso");
    s.filterDrive = v("f_drive");
    s.filterEnv = v("f_env");
    s.keyTrack = v("keytrack");
    s.vowel = v("vowel");
    s.fA = v("fe_a");
    s.fD = v("fe_d");
    s.fS = v("fe_s");
    s.fR = v("fe_r");
    s.aA = v("ae_a");
    s.aD = v("ae_d");
    s.aS = v("ae_s");
    s.aR = v("ae_r");
    s.lfoShape = i("lfo_shape");
    s.lfoSync = b("lfo_sync");
    s.lfoDiv = i("lfo_div");
    s.lfoHz = v("lfo_rate");
    s.lfoRetrig = b("lfo_retrig");
    s.lfoCut = v("lfo_cut");
    s.lfoPitch = v("lfo_pitch");
    s.lfoShapeAmt = v("lfo_shapeamt");
    s.lfoVol = v("lfo_vol");
    s.lfoVowel = v("lfo_vowel");
    s.distType = i("dist_type");
    s.drive = v("drive");
    s.distMix = v("dist_mix");
    s.cleanLows = b("clean_lows");
    s.split = v("split");
    s.glue = v("glue");
    s.width = v("width");
    s.volumeDb = v("volume");
    return s;
}

bool BassProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void BassProcessor::prepareToPlay(double sr, int) {
    engine_.prepare(sr);
    keyboardState.reset();
}

void BassProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer(midi, 0, n, true);  // the on-screen keyboard plays too
    engine_.setSettings(settings());

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying();
        }
    const double sr = getSampleRate() > 0 ? getSampleRate() : 48000.0;

    float* l = buffer.getWritePointer(0);
    std::vector<float>& tmp = mono_;  // right channel when the output is mono
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : tmp.data();
    int pos = 0;
    const auto renderTo = [&](int end) {
        while (pos < end) {
            const int m = std::min(end - pos, buffer.getNumChannels() > 1 ? end - pos : static_cast<int>(tmp.size()));
            engine_.render(l + pos, (buffer.getNumChannels() > 1 ? r + pos : tmp.data()), m, bpm, ppq + pos * bpm / 60.0 / sr, playing);
            pos += m;
        }
    };
    for (const auto meta : midi) {
        const auto msg = meta.getMessage();
        renderTo(std::clamp(meta.samplePosition, 0, n));
        if (msg.isNoteOn()) engine_.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff()) engine_.noteOff(msg.getNoteNumber());
        else if (msg.isPitchWheel()) engine_.pitchBend((msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine_.allNotesOff();
    }
    renderTo(n);

    // Oscilloscope feed (mono) and display values.
    for (int done = 0; done < n;) {
        const int chunk = std::min(n - done, static_cast<int>(mono_.size()));
        if (buffer.getNumChannels() > 1)
            for (int i = 0; i < chunk; ++i) mono_[static_cast<std::size_t>(i)] = 0.5f * (l[done + i] + r[done + i]);
        else
            std::copy(l + done, l + done + chunk, mono_.begin());
        scope.push(mono_.data(), static_cast<std::size_t>(chunk));
        done += chunk;
    }
    lfoValue.store(engine_.lfoValue(), std::memory_order_relaxed);
    cutoffNow.store(engine_.cutoffNow(), std::memory_order_relaxed);
    voices.store(engine_.activeVoices(), std::memory_order_relaxed);
    outPeak.store(buffer.getMagnitude(0, n), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* BassProcessor::createEditor() { return new BassEditor(*this); }

void BassProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void BassProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) state.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassProcessor(); }
