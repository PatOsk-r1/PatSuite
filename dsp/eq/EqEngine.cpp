#include "dsp/eq/EqEngine.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr int kSub = 32;  // coefficient update step (samples at the processing rate)

inline double biquad(const Biquad& c, double* z, double x) noexcept {
    const double y = c.b0 * x + z[0];
    z[0] = c.b1 * x - c.a1 * y + z[1];
    z[1] = c.b2 * x - c.a2 * y;
    return y;
}

inline void toMS(float* a, float* b, int n) noexcept {
    for (int i = 0; i < n; ++i) {
        const float m = 0.5f * (a[i] + b[i]), s = 0.5f * (a[i] - b[i]);
        a[i] = m;
        b[i] = s;
    }
}

inline void toLR(float* a, float* b, int n) noexcept {
    for (int i = 0; i < n; ++i) {
        const float l = a[i] + b[i], r = a[i] - b[i];
        a[i] = l;
        b[i] = r;
    }
}

float coefFor(double fs, float ms, int len) noexcept { return 1.0f - std::exp(static_cast<float>(-len / (fs * ms * 0.001))); }
}  // namespace

int EqEngine::latency(int mode, int resolution, double sampleRate) noexcept {
    if (mode == EqGlobal::Linear) return LinearPhase::latency(resolution);
    if (mode == EqGlobal::Natural && sampleRate < 80000.0) return static_cast<int>(Oversampler2x::kLatency);
    return 0;
}

void EqEngine::prepare(double sampleRate, int resolution) {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    upL_.assign(2 * kChunk, 0.0f);
    upR_ = upL_;
    det_.assign(kChunk, 0.0f);
    scDet_ = det_;
    monoR_.assign(kChunk, 0.0f);
    linear_.prepare(resolution);
    reset();
}

void EqEngine::reset() noexcept {
    for (auto& b : bands_) {
        b = Band{};
    }
    for (auto& d : dynGain_) d.store(0.0f);
    for (auto& o : os_) o.reset();
    linear_.reset();
    soloLast_ = -1;
    lastMode_ = -1;
}

float EqEngine::averageGainDb(const EqBand* bands, int numBands, float gainScale) noexcept {
    constexpr int kPoints = 60;
    double sum = 0.0;
    for (int k = 0; k < kPoints; ++k) {
        const double hz = 20.0 * std::pow(1000.0, (k + 0.5) / kPoints);
        double db = 0.0;
        for (int i = 0; i < numBands; ++i) {
            const auto& b = bands[i];
            if (!b.used || !b.active || b.dynamic) continue;
            const double m = std::max(1e-6, EqDesign::idealMagnitude(b, b.gain * gainScale, hz));
            db += 20.0 * std::log10(m) * (b.place == EqBand::Stereo ? 1.0 : 0.5);
        }
        sum += db;
    }
    return static_cast<float>(sum / kPoints);
}

void EqEngine::updateBand(Band& st, const EqBand& b, float gainDb, double fs, int subLen) noexcept {
    const bool structural = !st.on || st.type != b.type || st.slope != b.slope || st.fs != fs || st.place != b.place;
    if (structural) {
        st.sFreq = b.freq;
        st.sGain = gainDb;
        st.sQ = b.q;
    } else {
        const float a = coefFor(fs, 15.0f, subLen);
        st.sFreq = std::exp(std::log(st.sFreq) + a * (std::log(std::max(1.0f, b.freq)) - std::log(st.sFreq)));
        st.sGain += a * (gainDb - st.sGain);
        st.sQ += a * (b.q - st.sQ);
    }
    if (!structural && std::abs(st.sFreq - st.freq) <= st.freq * 1e-5f && std::abs(st.sGain - st.gain) < 1e-4f && std::abs(st.sQ - st.q) < 1e-5f) return;
    EqBand tmp = b;
    tmp.freq = st.sFreq;
    tmp.q = st.sQ;
    const int n = EqDesign::digital(tmp, st.sGain, fs, st.coef);
    if (structural || n != st.sections)
        for (auto& sec : st.z)
            for (auto& ch : sec) ch[0] = ch[1] = 0.0;
    st.sections = n;
    st.type = b.type;
    st.slope = b.slope;
    st.place = b.place;
    st.fs = fs;
    st.freq = st.sFreq;
    st.gain = st.sGain;
    st.q = st.sQ;
    st.on = true;
}

void EqEngine::runChain(float* l, float* r, int n, double fs, const EqBand* bands, float scale, int which) noexcept {
    bool ms = false;
    for (int i = 0; i < kBands; ++i) {
        const auto& b = bands[i];
        auto& st = bands_[static_cast<std::size_t>(i)];
        if (!b.used || !b.active || (which == 1 && !b.dynamic)) {
            st.on = false;
            continue;
        }
        const bool wantMS = b.place == EqBand::Mid || b.place == EqBand::Side;
        const bool wantLR = b.place == EqBand::Left || b.place == EqBand::Right;
        if (wantMS && !ms) {
            toMS(l, r, n);
            ms = true;
        } else if (wantLR && ms) {
            toLR(l, r, n);
            ms = false;
        }
        const bool doA = b.place != EqBand::Right && b.place != EqBand::Side;
        const bool doB = b.place != EqBand::Left && b.place != EqBand::Mid;
        const float gain = EqBand::hasGain(b.type) ? b.gain * scale + (b.dynamic ? st.dyn : 0.0f) : 0.0f;
        for (int off = 0; off < n; off += kSub) {
            const int m = std::min(kSub, n - off);
            updateBand(st, b, gain, fs, m);
            for (int s = 0; s < st.sections; ++s) {
                const auto& c = st.coef[s];
                if (doA) {
                    double* z = st.z[s][0];
                    for (int k = off; k < off + m; ++k) l[k] = static_cast<float>(biquad(c, z, l[k]));
                }
                if (doB) {
                    double* z = st.z[s][1];
                    for (int k = off; k < off + m; ++k) r[k] = static_cast<float>(biquad(c, z, r[k]));
                }
            }
        }
    }
    if (ms) toLR(l, r, n);
}

void EqEngine::detect(const float* det, int n, const EqBand* bands, const EqGlobal& g, bool haveSc, const float* scDet) noexcept {
    const float att = coefFor(sr_, std::max(0.1f, g.attackMs), 1), rel = coefFor(sr_, std::max(1.0f, g.releaseMs), 1);
    for (int i = 0; i < kBands; ++i) {
        const auto& b = bands[i];
        auto& st = bands_[static_cast<std::size_t>(i)];
        if (!b.used || !b.active || !b.dynamic || !EqBand::hasGain(b.type)) {
            st.dyn = 0.0f;
            st.env = 0.0f;
            dynGain_[static_cast<std::size_t>(i)].store(0.0f, std::memory_order_relaxed);
            continue;
        }
        if (std::abs(st.detFreq - b.freq) > b.freq * 0.002f || st.detFreq == 0.0f) {
            // Detector: the region the band works on.
            EqBand d;
            d.type = b.type == EqBand::LowShelf ? EqBand::HighCut : (b.type == EqBand::HighShelf || b.type == EqBand::TiltShelf) ? EqBand::LowCut : EqBand::BandPass;
            d.freq = b.freq;
            d.q = d.type == EqBand::BandPass ? std::clamp(b.q, 0.4f, 8.0f) : 0.7071f;
            d.slope = 1;
            EqDesign::digital(d, 0.0f, sr_, &st.det);
            st.detFreq = b.freq;
        }
        const float* src = (b.dynSidechain && haveSc) ? scDet : det;
        float env = st.env;
        for (int k = 0; k < n; ++k) {
            const float x = std::abs(static_cast<float>(biquad(st.det, st.dz, src[k])));
            env += (x > env ? att : rel) * (x - env);
        }
        st.env = env;
        const float levelDb = 20.0f * std::log10(env + 1e-9f);
        const float over = std::max(0.0f, levelDb - b.dynThreshold);
        const float range = b.dynRange * g.gainScale;
        st.dyn = range < 0.0f ? std::max(range, -over) : std::min(range, over);
        dynGain_[static_cast<std::size_t>(i)].store(st.dyn, std::memory_order_relaxed);
    }
}

void EqEngine::solo(float* l, float* r, int n, const EqBand& b) noexcept {
    if (soloLast_ != b.type * 100000 + static_cast<int>(b.freq) + static_cast<int>(b.q * 1000.0f) * 7) {
        EqBand d;
        d.type = (b.type == EqBand::LowShelf || b.type == EqBand::LowCut) ? EqBand::HighCut
                 : (b.type == EqBand::HighShelf || b.type == EqBand::HighCut) ? EqBand::LowCut
                                                                               : EqBand::BandPass;
        d.freq = b.freq;
        d.q = d.type == EqBand::BandPass ? std::clamp(b.q, 0.3f, 12.0f) : 0.7071f;
        d.slope = 1;
        EqDesign::digital(d, 0.0f, sr_, &soloCoef_);
        soloLast_ = b.type * 100000 + static_cast<int>(b.freq) + static_cast<int>(b.q * 1000.0f) * 7;
    }
    for (int k = 0; k < n; ++k) {
        l[k] = static_cast<float>(biquad(soloCoef_, soloZ_[0], l[k]));
        r[k] = static_cast<float>(biquad(soloCoef_, soloZ_[1], r[k]));
    }
}

void EqEngine::process(float* l, float* r, const float* scL, const float* scR, int n, const EqBand* bands, const EqGlobal& g) noexcept {
    if (upL_.empty() || n <= 0) return;
    int mode = g.mode;
    if (mode == EqGlobal::Natural && sr_ >= 80000.0) mode = EqGlobal::ZeroLatency;
    if (mode != lastMode_) {
        for (auto& b : bands_) {
            b.on = false;
            for (auto& sec : b.z)
                for (auto& ch : sec) ch[0] = ch[1] = 0.0;
        }
        for (auto& o : os_) o.reset();
        if (mode == EqGlobal::Linear) linear_.reset();
        lastMode_ = mode;
    }
    const bool solo = g.soloBand >= 0 && g.soloBand < kBands && bands[g.soloBand].used;
    for (int off = 0; off < n; off += kChunk) {
        const int m = std::min(kChunk, n - off);
        float* L = l + off;
        float* R = r ? r + off : monoR_.data();
        if (!r) std::copy(L, L + m, R);
        for (int i = 0; i < m; ++i) det_[static_cast<std::size_t>(i)] = 0.5f * (L[i] + R[i]);
        const bool haveSc = scL != nullptr;
        if (haveSc)
            for (int i = 0; i < m; ++i) scDet_[static_cast<std::size_t>(i)] = 0.5f * (scL[off + i] + (scR ? scR[off + i] : scL[off + i]));
        detect(det_.data(), m, bands, g, haveSc, scDet_.data());

        if (solo) {
            this->solo(L, R, m, bands[g.soloBand]);
        } else if (mode == EqGlobal::Natural) {
            for (int i = 0; i < m; ++i) {
                const auto k = static_cast<std::size_t>(2 * i);
                os_[0].up(L[i], upL_[k], upL_[k + 1]);
                os_[1].up(R[i], upR_[k], upR_[k + 1]);
            }
            runChain(upL_.data(), upR_.data(), 2 * m, 2.0 * sr_, bands, g.gainScale, 0);
            for (int i = 0; i < m; ++i) {
                const auto k = static_cast<std::size_t>(2 * i);
                L[i] = os_[0].down(upL_[k], upL_[k + 1]);
                R[i] = os_[1].down(upR_[k], upR_[k + 1]);
            }
        } else if (mode == EqGlobal::Linear) {
            linear_.process(L, R, m);
            runChain(L, R, m, sr_, bands, g.gainScale, 1);
        } else {
            runChain(L, R, m, sr_, bands, g.gainScale, 0);
        }

        // Output: level (+ auto gain), balance and polarity, smoothed.
        const float target = std::pow(10.0f, (g.outputDb + g.autoGainDb) / 20.0f) * (g.invert ? -1.0f : 1.0f);
        const float p = std::clamp(g.pan, -1.0f, 1.0f);
        const float tl = p > 0 ? 1.0f - p : 1.0f, tr = p < 0 ? 1.0f + p : 1.0f;
        const float a = coefFor(sr_, 20.0f, 1);
        for (int i = 0; i < m; ++i) {
            outGain_ += a * (target - outGain_);
            panL_ += a * (tl - panL_);
            panR_ += a * (tr - panR_);
            L[i] *= outGain_ * panL_;
            R[i] *= outGain_ * panR_;
        }
        if (!r)
            for (int i = 0; i < m; ++i) L[i] = 0.5f * (L[i] + R[i]);
    }
}

}  // namespace pat::dsp
