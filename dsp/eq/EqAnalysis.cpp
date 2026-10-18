#include "dsp/eq/EqAnalysis.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr float kLogSpan = 6.90775527898f;  // ln(1000)
}  // namespace

float SpectrumAnalyzer::binHz(int i) noexcept { return 20.0f * std::exp(kLogSpan * static_cast<float>(i) / (kBins - 1)); }
float SpectrumAnalyzer::binForHz(float hz) noexcept { return std::log(std::max(hz, 1.0f) / 20.0f) / kLogSpan * (kBins - 1); }

SpectrumAnalyzer::SpectrumAnalyzer(int maxFftSize) : max_(maxFftSize), size_(std::min(4096, maxFftSize)) {
    for (int n = 1024; n <= max_; n *= 2) ffts_.push_back(std::make_unique<Fft>(n));
    ring_.assign(static_cast<std::size_t>(max_), 0.0f);
    window_.assign(static_cast<std::size_t>(max_), 0.0f);
    mags_.assign(static_cast<std::size_t>(max_ / 2 + 1), 0.0f);
    frame_.assign(static_cast<std::size_t>(max_), 0.0f);
    scratch_.assign(static_cast<std::size_t>(max_), {});
    setFftSize(size_);
}

void SpectrumAnalyzer::setFftSize(int n) noexcept {
    int s = 1024;
    while (s < n && s < max_) s *= 2;
    size_ = s;
    for (int i = 0; i < size_; ++i) window_[static_cast<std::size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2.0 * kPi * i / size_));
}

void SpectrumAnalyzer::push(const float* x, int n) noexcept {
    for (int i = 0; i < n; ++i) {
        ring_[static_cast<std::size_t>(write_)] = x[i];
        write_ = (write_ + 1) % max_;
    }
}

void SpectrumAnalyzer::compute(double sampleRate, float* outDb) noexcept {
    for (int i = 0; i < size_; ++i) {
        const int idx = (write_ - size_ + i + max_) % max_;
        frame_[static_cast<std::size_t>(i)] = ring_[static_cast<std::size_t>(idx)] * window_[static_cast<std::size_t>(i)];
    }
    const Fft* fft = nullptr;
    for (const auto& f : ffts_)
        if (f->size() == size_) fft = f.get();
    if (!fft) return;
    fft->realMagnitudes(frame_.data(), mags_.data(), scratch_.data());
    const float norm = 4.0f / static_cast<float>(size_);  // Hann coherent gain 0.5, one-sided
    const float binPerHz = static_cast<float>(size_ / sampleRate);
    const int top = size_ / 2;
    for (int i = 0; i < kBins; ++i) {
        // Each display point covers the FFT bins up to halfway to its neighbours: the loudest wins.
        const float lo = binHz(std::max(0, i - 1)) * 0.5f + binHz(i) * 0.5f;
        const float hi = binHz(std::min(kBins - 1, i + 1)) * 0.5f + binHz(i) * 0.5f;
        const float bLo = lo * binPerHz, bHi = hi * binPerHz;
        float m;
        if (bHi - bLo < 1.0f) {  // fewer than one FFT bin: interpolate
            const float b = std::min(binHz(i) * binPerHz, static_cast<float>(top - 1));
            const int k = static_cast<int>(b);
            const float f = b - k;
            m = mags_[static_cast<std::size_t>(k)] + f * (mags_[static_cast<std::size_t>(k) + 1] - mags_[static_cast<std::size_t>(k)]);
        } else {
            m = 0.0f;
            for (int k = static_cast<int>(bLo); k <= std::min(top, static_cast<int>(bHi)); ++k) m = std::max(m, mags_[static_cast<std::size_t>(k)]);
        }
        outDb[i] = 20.0f * std::log10(m * norm + 1e-9f);
    }
}

void EqMatch::smooth(const float* in, float* out, float octaves) {
    const int n = SpectrumAnalyzer::kBins;
    const float perOctave = (n - 1) / 9.96578f;  // display points per octave (20 Hz .. 20 kHz)
    const int half = std::max(1, static_cast<int>(octaves * perOctave * 0.5f));
    for (int i = 0; i < n; ++i) {
        float sum = 0.0f;
        int count = 0;
        for (int k = std::max(0, i - half); k <= std::min(n - 1, i + half); ++k) {
            sum += in[k];
            ++count;
        }
        out[i] = sum / static_cast<float>(count);
    }
}

std::vector<EqBand> EqMatch::fit(const float* referenceDb, const float* currentDb, int maxBands, float amount) {
    const int n = SpectrumAnalyzer::kBins;
    std::vector<float> diff(static_cast<std::size_t>(n)), res(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) diff[static_cast<std::size_t>(i)] = referenceDb[i] - currentDb[i];
    EqMatch::smooth(diff.data(), res.data(), 1.0f / 3.0f);
    // Ignore the overall level: centre the curve on its 100 Hz .. 10 kHz average.
    const int a = static_cast<int>(SpectrumAnalyzer::binForHz(100.0f)), b = static_cast<int>(SpectrumAnalyzer::binForHz(10000.0f));
    float mean = 0.0f;
    for (int i = a; i <= b; ++i) mean += res[static_cast<std::size_t>(i)];
    mean /= static_cast<float>(b - a + 1);
    for (auto& v : res) v = std::clamp(v - mean, -24.0f, 24.0f);

    std::vector<EqBand> bands;
    const int lo = static_cast<int>(SpectrumAnalyzer::binForHz(25.0f)), hi = static_cast<int>(SpectrumAnalyzer::binForHz(18000.0f));
    for (int iter = 0; iter < maxBands; ++iter) {
        int best = lo;
        for (int i = lo; i <= hi; ++i)
            if (std::abs(res[static_cast<std::size_t>(i)]) > std::abs(res[static_cast<std::size_t>(best)])) best = i;
        const float g = res[static_cast<std::size_t>(best)];
        if (std::abs(g) < 0.5f) break;
        // Width: where the residual falls to half of the peak on each side.
        int l = best, r = best;
        while (l > 0 && res[static_cast<std::size_t>(l)] * g > 0.5f * g * g) --l;
        while (r < n - 1 && res[static_cast<std::size_t>(r)] * g > 0.5f * g * g) ++r;
        EqBand band;
        band.used = true;
        band.freq = SpectrumAnalyzer::binHz(best);
        band.gain = g;
        const float hzL = SpectrumAnalyzer::binHz(l), hzR = SpectrumAnalyzer::binHz(r);
        if (best <= lo + 2 || l == 0) {
            band.type = EqBand::LowShelf;
            band.freq = hzR;
            band.q = 0.7071f;
        } else if (best >= hi - 2 || r == n - 1) {
            band.type = EqBand::HighShelf;
            band.freq = hzL;
            band.q = 0.7071f;
        } else {
            const float bw = std::max(0.2f, std::log2(hzR / hzL));
            const float p = std::pow(2.0f, bw);
            band.q = std::clamp(std::sqrt(p) / (p - 1.0f), 0.3f, 8.0f);
        }
        for (int i = 0; i < n; ++i)
            res[static_cast<std::size_t>(i)] -= 20.0f * static_cast<float>(std::log10(std::max(1e-6, EqDesign::idealMagnitude(band, band.gain, SpectrumAnalyzer::binHz(i)))));
        bands.push_back(band);
    }
    for (auto& band : bands) band.gain *= amount;
    return bands;
}

void Collision::compute(const float* aDb, const float* bDb, float* out, float floorDb) {
    for (int i = 0; i < SpectrumAnalyzer::kBins; ++i) {
        const float loud = std::clamp((std::min(aDb[i], bDb[i]) - floorDb) / 40.0f, 0.0f, 1.0f);
        const float close = std::clamp(1.0f - std::abs(aDb[i] - bDb[i]) / 18.0f, 0.0f, 1.0f);
        out[i] = loud * close;
    }
}

}  // namespace pat::dsp
