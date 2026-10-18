#include "plugins/PatQpro/Processor.h"

#include "ui/eq/QproEditor.h"

using namespace qpro;
using pat::dsp::EqBand;

namespace {
const char* kFieldNames[NumFields] = {"used", "active", "type", "freq", "gain", "q", "slope", "place", "dyn", "range", "thr", "sc"};

juce::String hzText(float v) { return v >= 1000.0f ? juce::String(v / 1000.0f, v >= 10000.0f ? 1 : 2) + " kHz" : juce::String(v, v < 100 ? 1 : 0) + " Hz"; }
juce::String dbText(float v) { return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB"; }

juce::AudioProcessorValueTreeState::ParameterLayout layout() {
    using F = juce::AudioParameterFloat;
    using A = juce::AudioParameterFloatAttributes;
    using C = juce::AudioParameterChoice;
    using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    juce::StringArray types, slopes{"6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall"};
    for (int t = 0; t < EqBand::NumTypes; ++t) types.add(EqBand::typeName(t));
    juce::NormalisableRange<float> freq(10.0f, 30000.0f, [](float a, float b, float v) { return a * std::pow(b / a, v); },
                                        [](float a, float b, float v) { return std::log(v / a) / std::log(b / a); });
    juce::NormalisableRange<float> q(0.025f, 40.0f, [](float a, float b, float v) { return a * std::pow(b / a, v); },
                                     [](float a, float b, float v) { return std::log(v / a) / std::log(b / a); });
    for (int b = 0; b < kBands; ++b) {
        const juce::String n = "Band " + juce::String(b + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup>("band" + juce::String(b + 1), "Band " + juce::String(b + 1), "|");
        group->addChild(std::make_unique<B>(juce::ParameterID{bandId(b, Used), 1}, n + "Used", false));
        group->addChild(std::make_unique<B>(juce::ParameterID{bandId(b, Active), 1}, n + "Active", true));
        group->addChild(std::make_unique<C>(juce::ParameterID{bandId(b, Type), 1}, n + "Type", types, 0));
        group->addChild(std::make_unique<F>(juce::ParameterID{bandId(b, Freq), 1}, n + "Frequency", freq, 1000.0f,
                                            A().withStringFromValueFunction([](float v, int) { return hzText(v); })));
        group->addChild(std::make_unique<F>(juce::ParameterID{bandId(b, Gain), 1}, n + "Gain", juce::NormalisableRange<float>(-30.0f, 30.0f), 0.0f,
                                            A().withStringFromValueFunction([](float v, int) { return dbText(v); })));
        group->addChild(std::make_unique<F>(juce::ParameterID{bandId(b, Q), 1}, n + "Q", q, 1.0f,
                                            A().withStringFromValueFunction([](float v, int) { return juce::String(v, v < 10 ? 2 : 1); })));
        group->addChild(std::make_unique<C>(juce::ParameterID{bandId(b, Slope), 1}, n + "Slope", slopes, 1));
        group->addChild(std::make_unique<C>(juce::ParameterID{bandId(b, Place), 1}, n + "Placement", juce::StringArray{"Stereo", "Left", "Right", "Mid", "Side"}, 0));
        group->addChild(std::make_unique<B>(juce::ParameterID{bandId(b, Dyn), 1}, n + "Dynamic", false));
        group->addChild(std::make_unique<F>(juce::ParameterID{bandId(b, Range), 1}, n + "Dynamic Range", juce::NormalisableRange<float>(-30.0f, 30.0f), 0.0f,
                                            A().withStringFromValueFunction([](float v, int) { return dbText(v); })));
        group->addChild(std::make_unique<F>(juce::ParameterID{bandId(b, Threshold), 1}, n + "Threshold", juce::NormalisableRange<float>(-80.0f, 0.0f), -30.0f,
                                            A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " dB"; })));
        group->addChild(std::make_unique<B>(juce::ParameterID{bandId(b, Sc), 1}, n + "External Sidechain", false));
        l.add(std::move(group));
    }
    l.add(std::make_unique<C>(juce::ParameterID{kMode, 1}, "Processing Mode", juce::StringArray{"Zero Latency", "Natural Phase", "Linear Phase"}, 0));
    l.add(std::make_unique<C>(juce::ParameterID{kResolution, 1}, "Linear Phase Resolution", juce::StringArray{"Low", "Medium", "High", "Very High"}, 1));
    l.add(std::make_unique<F>(juce::ParameterID{kScale, 1}, "Gain Scale", juce::NormalisableRange<float>(-2.0f, 2.0f), 1.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kOutput, 1}, "Output Gain", juce::NormalisableRange<float>(-36.0f, 36.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) { return dbText(v); })));
    l.add(std::make_unique<F>(juce::ParameterID{kPan, 1}, "Output Pan", juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f,
                              A().withStringFromValueFunction([](float v, int) {
                                  const int p = juce::roundToInt(std::abs(v) * 100.0f);
                                  return p == 0 ? juce::String("C") : juce::String(p) + (v < 0 ? " L" : " R");
                              })));
    l.add(std::make_unique<B>(juce::ParameterID{kInvert, 1}, "Phase Invert", false));
    l.add(std::make_unique<B>(juce::ParameterID{kAutoGain, 1}, "Auto Gain", false));
    juce::NormalisableRange<float> att(0.5f, 200.0f);
    att.setSkewForCentre(15.0f);
    juce::NormalisableRange<float> rel(10.0f, 2000.0f);
    rel.setSkewForCentre(150.0f);
    l.add(std::make_unique<F>(juce::ParameterID{kAttack, 1}, "Dynamic Attack", att, 10.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })));
    l.add(std::make_unique<F>(juce::ParameterID{kRelease, 1}, "Dynamic Release", rel, 120.0f,
                              A().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; })));
    return l;
}
}  // namespace

juce::String qpro::bandId(int band, int field) { return "b" + juce::String(band + 1) + "_" + kFieldNames[field]; }

// ------------------------------------------------------------------------------------------- builder
// Builds linear-phase kernels away from the audio and message threads.
class QproProcessor::Builder final : public juce::Thread {
public:
    explicit Builder(pat::dsp::LinearPhase& lp) : juce::Thread("PatQpro FIR"), lp_(lp) { startThread(juce::Thread::Priority::low); }
    ~Builder() override {
        signalThreadShouldExit();
        event_.signal();
        stopThread(2000);
    }
    void request(const std::array<EqBand, kBands>& bands, float scale, double fs, int resolution) {
        {
            const juce::ScopedLock sl(lock_);
            bands_ = bands;
            scale_ = scale;
            fs_ = fs;
            res_ = resolution;
            pending_ = true;
        }
        event_.signal();
    }
    void run() override {
        while (!threadShouldExit()) {
            event_.wait(500);
            std::array<EqBand, kBands> bands;
            float scale;
            double fs;
            int res;
            {
                const juce::ScopedLock sl(lock_);
                if (!pending_) {
                    lp_.collect();
                    continue;
                }
                bands = bands_;
                scale = scale_;
                fs = fs_;
                res = res_;
                pending_ = false;
            }
            lp_.publish(pat::dsp::LinearPhase::build(bands.data(), kBands, scale, fs, res));
        }
    }

private:
    pat::dsp::LinearPhase& lp_;
    juce::CriticalSection lock_;
    juce::WaitableEvent event_;
    std::array<EqBand, kBands> bands_{};
    float scale_ = 1;
    double fs_ = 48000;
    int res_ = 1;
    bool pending_ = false;
};

// ------------------------------------------------------------------------------------------- processor
QproProcessor::QproProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state(*this, &undo, "PARAMS", layout()) {
    for (int b = 0; b < kBands; ++b)
        for (int f = 0; f < NumFields; ++f) bp_[static_cast<std::size_t>(b)][static_cast<std::size_t>(f)] = state.getRawParameterValue(bandId(b, f));
    const char* ids[] = {kMode, kResolution, kScale, kOutput, kPan, kInvert, kAutoGain, kAttack, kRelease};
    for (std::size_t i = 0; i < gp_.size(); ++i) gp_[i] = state.getRawParameterValue(ids[i]);
    pre_.fill(-120.0f);
    post_ = sc_ = pre_;
    pull_.assign(65536, 0.0f);
    mono_.assign(8192, 0.0f);
    hubId_ = hub_->nextId++;
    hub_->slots.push_back({hubId_, {}, {}, 0.0});
    engine_.prepare(48000.0, 1);
    builder_ = std::make_unique<Builder>(engine_.linear());
    presets = std::make_unique<pat::ui::PresetLibrary>(state, "PatQpro", factoryPresets());
    startTimerHz(rateHz_);
}

QproProcessor::~QproProcessor() {
    stopTimer();
    auto& s = hub_->slots;
    s.erase(std::remove_if(s.begin(), s.end(), [this](const SpectrumHub::Slot& x) { return x.id == hubId_; }), s.end());
    builder_.reset();
}

bool QproProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainInputChannelSet() != out) return false;
    if (layouts.inputBuses.size() > 1) {
        const auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void QproProcessor::prepareToPlay(double sr, int) {
    sr_.store(sr);
    engine_.prepare(sr, juce::roundToInt(gp_[1]->load()));
    lastBandsKey_.clear();  // rebuild the linear-phase kernel for this rate
    latency_ = -1;
    updateServices();
}

EqBand QproProcessor::band(int i) const {
    const auto& p = bp_[static_cast<std::size_t>(i)];
    EqBand b;
    b.used = p[Used]->load() > 0.5f;
    b.active = p[Active]->load() > 0.5f;
    b.type = juce::roundToInt(p[Type]->load());
    b.freq = p[Freq]->load();
    b.gain = p[Gain]->load();
    b.q = p[Q]->load();
    b.slope = juce::roundToInt(p[Slope]->load());
    b.place = juce::roundToInt(p[Place]->load());
    b.dynamic = p[Dyn]->load() > 0.5f;
    b.dynRange = p[Range]->load();
    b.dynThreshold = p[Threshold]->load();
    b.dynSidechain = p[Sc]->load() > 0.5f;
    return b;
}

void QproProcessor::bands(EqBand* out) const {
    for (int i = 0; i < kBands; ++i) out[i] = band(i);
}

pat::dsp::EqGlobal QproProcessor::global() const {
    pat::dsp::EqGlobal g;
    g.mode = juce::roundToInt(gp_[0]->load());
    g.resolution = juce::roundToInt(gp_[1]->load());
    g.gainScale = gp_[2]->load();
    g.outputDb = gp_[3]->load();
    g.pan = gp_[4]->load();
    g.invert = gp_[5]->load() > 0.5f;
    g.autoGainDb = gp_[6]->load() > 0.5f ? autoGainDb_.load() : 0.0f;
    g.attackMs = gp_[7]->load();
    g.releaseMs = gp_[8]->load();
    g.soloBand = solo.load();
    return g;
}

void QproProcessor::setParam(const juce::String& id, float value) {
    if (auto* p = state.getParameter(id)) {
        const float v = p->convertTo0to1(value);
        if (std::abs(v - p->getValue()) < 1e-7f) return;
        p->beginChangeGesture();
        p->setValueNotifyingHost(v);
        p->endChangeGesture();
    }
}

void QproProcessor::setBand(int i, const EqBand& b) {
    setParam(bandId(i, Used), b.used ? 1.0f : 0.0f);
    setParam(bandId(i, Active), b.active ? 1.0f : 0.0f);
    setParam(bandId(i, Type), static_cast<float>(b.type));
    setParam(bandId(i, Freq), b.freq);
    setParam(bandId(i, Gain), b.gain);
    setParam(bandId(i, Q), b.q);
    setParam(bandId(i, Slope), static_cast<float>(b.slope));
    setParam(bandId(i, Place), static_cast<float>(b.place));
    setParam(bandId(i, Dyn), b.dynamic ? 1.0f : 0.0f);
    setParam(bandId(i, Range), b.dynRange);
    setParam(bandId(i, Threshold), b.dynThreshold);
    setParam(bandId(i, Sc), b.dynSidechain ? 1.0f : 0.0f);
}

int QproProcessor::firstFreeBand() const {
    for (int i = 0; i < kBands; ++i)
        if (bp_[static_cast<std::size_t>(i)][Used]->load() < 0.5f) return i;
    return -1;
}

void QproProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
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
    for (int i = 0; i < kBands; ++i) audioBands_[static_cast<std::size_t>(i)] = band(i);
    const auto g = global();

    const auto pushMono = [this](pat::SpscFifo<float>& fifo, const float* a, const float* b, int count) {
        for (int done = 0; done < count;) {
            const int chunk = std::min(count - done, static_cast<int>(mono_.size()));
            for (int i = 0; i < chunk; ++i) mono_[static_cast<std::size_t>(i)] = b ? 0.5f * (a[done + i] + b[done + i]) : a[done + i];
            fifo.push(mono_.data(), static_cast<std::size_t>(chunk));
            done += chunk;
        }
    };
    float pin = 0;
    for (int i = 0; i < n; ++i) pin = std::max(pin, std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.0f));
    peakIn.store(pin, std::memory_order_relaxed);
    pushMono(preFifo, l, r, n);
    bool scSignal = false;
    if (scL) {
        pushMono(scFifo, scL, scR, n);
        for (int i = 0; i < n && !scSignal; i += 16) scSignal = std::abs(scL[i]) > 1e-5f;
    }
    scActive_.store(scSignal, std::memory_order_relaxed);

    engine_.process(l, r, scL, scR, n, audioBands_.data(), g);

    pushMono(postFifo, l, r, n);
    float pout = 0;
    for (int i = 0; i < n; ++i) pout = std::max(pout, std::max(std::abs(l[i]), r ? std::abs(r[i]) : 0.0f));
    peakOut.store(pout, std::memory_order_relaxed);
    for (int c = main.getNumChannels(); c < getTotalNumOutputChannels(); ++c) buffer.clear(c, 0, n);
}

// ------------------------------------------------------------------------------------------- services
void QproProcessor::setAnalyzerRate(int hz, int fftSize) {
    hz = juce::jlimit(5, 60, hz);
    if (hz != rateHz_) {
        rateHz_ = hz;
        startTimerHz(rateHz_);
    }
    for (auto* a : {&preAn_, &postAn_, &scAn_})
        if (a->fftSize() != fftSize) a->setFftSize(fftSize);
}

void QproProcessor::updateTrackProperties(const TrackProperties& p) {
    if (p.name.has_value()) trackName_ = *p.name;
}

juce::String QproProcessor::instanceName() const { return trackName_.isNotEmpty() ? trackName_ : "PatQpro " + juce::String(hubId_); }

std::vector<std::pair<int, juce::String>> QproProcessor::otherInstances() const {
    std::vector<std::pair<int, juce::String>> out;
    for (const auto& s : hub_->slots)
        if (s.id != hubId_) out.push_back({s.id, s.name});
    return out;
}

const QproProcessor::Spectrum* QproProcessor::instanceSpectrum(int id) const {
    for (const auto& s : hub_->slots)
        if (s.id == id) return &s.db;
    return nullptr;
}

void QproProcessor::timerCallback() {
    const double fs = sr_.load();
    // Pull the newest audio into the analyzers.
    const auto feed = [this](pat::SpscFifo<float>& fifo, pat::dsp::SpectrumAnalyzer& an) {
        fifo.skipToNewest(pull_.size());
        const auto got = fifo.pop(pull_.data(), pull_.size());
        an.push(pull_.data(), static_cast<int>(got));
        return got > 0;
    };
    const bool fresh = feed(preFifo, preAn_);
    feed(postFifo, postAn_);
    const bool scFresh = feed(scFifo, scAn_);
    Spectrum tmp{};
    const float fall = std::exp(-1.0f / (0.25f * static_cast<float>(rateHz_)));  // display release
    const auto update = [&](pat::dsp::SpectrumAnalyzer& an, Spectrum& dst) {
        an.compute(fs, tmp.data());
        for (std::size_t i = 0; i < dst.size(); ++i) dst[i] = tmp[i] > dst[i] ? tmp[i] : tmp[i] + fall * (dst[i] - tmp[i]);
    };
    if (fresh) {
        update(preAn_, pre_);
        update(postAn_, post_);
    }
    if (scFresh) update(scAn_, sc_);
    else if (!scActive_.load())
        for (auto& v : sc_) v = std::max(-120.0f, v - 3.0f);

    for (auto& s : hub_->slots)
        if (s.id == hubId_) {
            s.name = instanceName();
            s.db = post_;
            s.updated = juce::Time::getMillisecondCounterHiRes();
        }

    if (learning_) {
        const Spectrum* ref = learnRef_ == 0 ? &sc_ : instanceSpectrum(learnRef_);
        if (ref) {
            for (std::size_t i = 0; i < pre_.size(); ++i) {
                learnRefSum_[i] += (*ref)[i];
                learnOwnSum_[i] += pre_[i];
            }
            ++learnFrames_;
        }
    }
    updateServices();
}

void QproProcessor::updateServices() {
    std::array<EqBand, kBands> b;
    bands(b.data());
    const auto g = global();
    // A compact key of everything the curve depends on.
    juce::String key;
    key.preallocateBytes(2048);
    for (const auto& x : b)
        if (x.used)
            key << (x.active ? 1 : 0) << x.type << ',' << x.freq << ',' << x.gain << ',' << x.q << ',' << x.slope << ',' << x.place << ','
                << (x.dynamic ? 1 : 0) << ';';
    key << '|' << g.gainScale << '|' << g.resolution << '|' << sr_.load();
    if (key != lastBandsKey_) {
        lastBandsKey_ = key;
        autoGainDb_.store(-pat::dsp::EqEngine::averageGainDb(b.data(), kBands, g.gainScale));
        builder_->request(b, g.gainScale, sr_.load(), g.resolution);
    }
    const int lat = pat::dsp::EqEngine::latency(g.mode, g.resolution, sr_.load());
    if (lat != latency_) {
        latency_ = lat;
        setLatencySamples(lat);
    }
}

void QproProcessor::startLearning(int referenceId) {
    learnRef_ = referenceId;
    learnFrames_ = 0;
    learnRefSum_.fill(0.0);
    learnOwnSum_.fill(0.0);
    learning_ = true;
}

void QproProcessor::stopLearning() { learning_ = false; }

bool QproProcessor::applyMatch(int maxBands, float amount) {
    if (learnFrames_ < 3) return false;
    Spectrum ref{}, own{};
    for (std::size_t i = 0; i < ref.size(); ++i) {
        ref[i] = static_cast<float>(learnRefSum_[i] / learnFrames_);
        own[i] = static_cast<float>(learnOwnSum_[i] / learnFrames_);
    }
    const auto fitted = pat::dsp::EqMatch::fit(ref.data(), own.data(), juce::jlimit(1, kBands, maxBands), amount);
    undo.beginNewTransaction("EQ Match");
    for (int i = 0; i < kBands; ++i) {
        EqBand b;  // defaults: unused
        if (i < static_cast<int>(fitted.size())) b = fitted[static_cast<std::size_t>(i)];
        setBand(i, b);
    }
    undo.beginNewTransaction();
    return true;
}

juce::AudioProcessorEditor* QproProcessor::createEditor() { return new QproEditor(*this); }

void QproProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void QproProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(state.state.getType())) {
            state.replaceState(juce::ValueTree::fromXml(*xml));
            undo.clearUndoHistory();
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new QproProcessor(); }
