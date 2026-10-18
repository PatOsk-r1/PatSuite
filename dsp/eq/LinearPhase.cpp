#include "dsp/eq/LinearPhase.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kMaxPartitions = 16384 / LinearPhase::kBlock;
}  // namespace

std::unique_ptr<LinearKernel> LinearPhase::build(const EqBand* bands, int numBands, float gainScale, double fs, int resolution) {
    auto k = std::make_unique<LinearKernel>();
    const int L = lengthFor(resolution);
    k->length = L;
    k->partitions = L / kBlock;
    for (int i = 0; i < numBands; ++i)
        if (bands[i].used && bands[i].active && !bands[i].dynamic && (bands[i].place == EqBand::Mid || bands[i].place == EqBand::Side)) k->cross = true;

    // Zero-phase target responses of the four paths.
    std::vector<double> resp[4];
    for (auto& r : resp) r.assign(static_cast<std::size_t>(L / 2 + 1), 0.0);
    for (int bin = 0; bin <= L / 2; ++bin) {
        const double hz = std::max(1.0, bin * fs / L);
        double hl = 1, hr = 1, hm = 1, hs = 1;
        for (int i = 0; i < numBands; ++i) {
            const auto& b = bands[i];
            if (!b.used || !b.active || b.dynamic) continue;
            const double m = EqDesign::idealMagnitude(b, b.gain * gainScale, hz);
            switch (b.place) {
                case EqBand::Left: hl *= m; break;
                case EqBand::Right: hr *= m; break;
                case EqBand::Mid: hm *= m; break;
                case EqBand::Side: hs *= m; break;
                default:
                    hl *= m;
                    hr *= m;
                    break;
            }
        }
        const double t0 = 0.5 * (hm + hs), t1 = 0.5 * (hm - hs);
        const auto b = static_cast<std::size_t>(bin);
        resp[0][b] = t0 * hl;  // L -> L
        resp[1][b] = t1 * hr;  // R -> L
        resp[2][b] = t1 * hl;  // L -> R
        resp[3][b] = t0 * hr;  // R -> R
    }

    Fft big(L), small(kFftSize);
    std::vector<std::complex<float>> spec(static_cast<std::size_t>(L));
    std::vector<float> fir(static_cast<std::size_t>(L));
    std::vector<std::complex<float>> part(static_cast<std::size_t>(kFftSize));
    for (int path = 0; path < 4; ++path) {
        auto& h = k->h[path];
        h.assign(static_cast<std::size_t>(k->partitions * kFftSize), {});
        if (path == 1 || path == 2) {
            if (!k->cross) continue;
        }
        for (int bin = 0; bin < L; ++bin) {
            const int mirror = bin <= L / 2 ? bin : L - bin;
            spec[static_cast<std::size_t>(bin)] = {static_cast<float>(resp[path][static_cast<std::size_t>(mirror)]), 0.0f};
        }
        big.inverse(spec.data());
        // Centre the impulse at L/2 and window it (Blackman).
        for (int m = 0; m < L; ++m) {
            const int idx = ((m - L / 2) % L + L) % L;
            const double w = 0.42 - 0.5 * std::cos(2.0 * kPi * m / L) + 0.08 * std::cos(4.0 * kPi * m / L);
            fir[static_cast<std::size_t>(m)] = m == 0 ? 0.0f : spec[static_cast<std::size_t>(idx)].real() * static_cast<float>(w);
        }
        for (int p = 0; p < k->partitions; ++p) {
            std::fill(part.begin(), part.end(), std::complex<float>{});
            for (int i = 0; i < kBlock; ++i) part[static_cast<std::size_t>(i)] = {fir[static_cast<std::size_t>(p * kBlock + i)], 0.0f};
            small.transform(part.data());
            std::copy(part.begin(), part.end(), h.begin() + p * kFftSize);
        }
    }
    return k;
}

LinearPhase::LinearPhase() = default;

LinearPhase::~LinearPhase() {
    delete pending_.exchange(nullptr);
    delete current_;
    collect();
}

void LinearPhase::prepare(int resolution) {
    resolution_ = resolution;
    inL_.assign(kBlock, 0.0f);
    inR_ = outL_ = outR_ = inL_;
    histL_.assign(kFftSize, 0.0f);
    histR_ = histL_;
    maxPartitions_ = kMaxPartitions;
    fdlL_.assign(static_cast<std::size_t>(maxPartitions_ * kFftSize), {});
    fdlR_ = fdlL_;
    work_.assign(static_cast<std::size_t>(2 * kFftSize), {});
    accA_.assign(kFftSize, {});
    accB_ = accA_;
    // Start with a pass-through kernel (a delayed impulse) so the latency is right from the start.
    delete current_;
    current_ = build(nullptr, 0, 1.0f, 48000.0, resolution).release();
    reset();
}

void LinearPhase::reset() noexcept {
    std::fill(inL_.begin(), inL_.end(), 0.0f);
    std::fill(inR_.begin(), inR_.end(), 0.0f);
    std::fill(outL_.begin(), outL_.end(), 0.0f);
    std::fill(outR_.begin(), outR_.end(), 0.0f);
    std::fill(histL_.begin(), histL_.end(), 0.0f);
    std::fill(histR_.begin(), histR_.end(), 0.0f);
    std::fill(fdlL_.begin(), fdlL_.end(), std::complex<float>{});
    std::fill(fdlR_.begin(), fdlR_.end(), std::complex<float>{});
    fill_ = 0;
    fdlPos_ = 0;
}

void LinearPhase::publish(std::unique_ptr<LinearKernel> k) {
    delete pending_.exchange(k.release(), std::memory_order_acq_rel);
    collect();
}

void LinearPhase::collect() {
    LinearKernel* k = nullptr;
    while (retired_.pop(&k, 1) == 1) delete k;
}

void LinearPhase::process(float* l, float* r, int n) noexcept {
    if (inL_.empty()) return;
    for (int i = 0; i < n; ++i) {
        const auto f = static_cast<std::size_t>(fill_);
        inL_[f] = l[i];
        inR_[f] = r ? r[i] : l[i];
        l[i] = outL_[f];
        if (r) r[i] = outR_[f];
        if (++fill_ == kBlock) {
            runBlock();
            fill_ = 0;
        }
    }
}

void LinearPhase::convolve(const LinearKernel& k, std::complex<float>* out) noexcept {
    std::fill(accA_.begin(), accA_.end(), std::complex<float>{});
    std::fill(accB_.begin(), accB_.end(), std::complex<float>{});
    const int parts = std::min(k.partitions, maxPartitions_);
    for (int p = 0; p < parts; ++p) {
        const int slot = (fdlPos_ - p + maxPartitions_) % maxPartitions_;
        const auto* xl = fdlL_.data() + slot * kFftSize;
        const auto* xr = fdlR_.data() + slot * kFftSize;
        const auto* hll = k.h[0].data() + p * kFftSize;
        const auto* hrr = k.h[3].data() + p * kFftSize;
        for (int b = 0; b < kFftSize; ++b) {
            accA_[static_cast<std::size_t>(b)] += hll[b] * xl[b];
            accB_[static_cast<std::size_t>(b)] += hrr[b] * xr[b];
        }
        if (k.cross) {
            const auto* hrl = k.h[1].data() + p * kFftSize;
            const auto* hlr = k.h[2].data() + p * kFftSize;
            for (int b = 0; b < kFftSize; ++b) {
                accA_[static_cast<std::size_t>(b)] += hrl[b] * xr[b];
                accB_[static_cast<std::size_t>(b)] += hlr[b] * xl[b];
            }
        }
    }
    // Left and right outputs back into one complex signal: real = left, imaginary = right.
    const std::complex<float> j(0.0f, 1.0f);
    for (int b = 0; b < kFftSize; ++b) out[b] = accA_[static_cast<std::size_t>(b)] + j * accB_[static_cast<std::size_t>(b)];
    fft_.inverse(out);
}

void LinearPhase::runBlock() noexcept {
    // Overlap-save input: the previous block followed by the new one.
    std::copy(histL_.begin() + kBlock, histL_.end(), histL_.begin());
    std::copy(histR_.begin() + kBlock, histR_.end(), histR_.begin());
    std::copy(inL_.begin(), inL_.end(), histL_.begin() + kBlock);
    std::copy(inR_.begin(), inR_.end(), histR_.begin() + kBlock);
    auto* z = work_.data();
    for (int i = 0; i < kFftSize; ++i) z[i] = {histL_[static_cast<std::size_t>(i)], histR_[static_cast<std::size_t>(i)]};
    fft_.transform(z);
    fdlPos_ = (fdlPos_ + 1) % maxPartitions_;
    auto* xl = fdlL_.data() + fdlPos_ * kFftSize;
    auto* xr = fdlR_.data() + fdlPos_ * kFftSize;
    const std::complex<float> halfJ(0.0f, -0.5f);  // 1 / (2j)
    for (int b = 0; b < kFftSize; ++b) {
        const auto zc = std::conj(z[(kFftSize - b) % kFftSize]);
        xl[b] = 0.5f * (z[b] + zc);
        xr[b] = halfJ * (z[b] - zc);
    }

    LinearKernel* incoming = pending_.exchange(nullptr, std::memory_order_acq_rel);
    LinearKernel* old = nullptr;
    if (incoming) {
        old = current_;
        current_ = incoming;
    }
    if (!current_) return;
    auto* yNew = work_.data();
    convolve(*current_, yNew);
    if (old) {
        auto* yOld = work_.data() + kFftSize;
        convolve(*old, yOld);
        for (int i = 0; i < kBlock; ++i) {
            const float f = (i + 0.5f) / kBlock;
            const auto v = yOld[kBlock + i] + f * (yNew[kBlock + i] - yOld[kBlock + i]);
            outL_[static_cast<std::size_t>(i)] = v.real();
            outR_[static_cast<std::size_t>(i)] = v.imag();
        }
        retired_.push(&old, 1);  // a full queue (builder not collecting) leaks rather than frees here
        return;
    }
    for (int i = 0; i < kBlock; ++i) {
        outL_[static_cast<std::size_t>(i)] = yNew[kBlock + i].real();
        outR_[static_cast<std::size_t>(i)] = yNew[kBlock + i].imag();
    }
}

}  // namespace pat::dsp
