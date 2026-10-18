#include "dsp/master/Loudness.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;

// 4x interpolation: for each fractional position (0, 1/4, 2/4, 3/4) the weights of the 8 samples
// around it (windowed sinc).
struct TpCoefs {
    float c[4][8];
    TpCoefs() {
        for (int p = 0; p < 4; ++p)
            for (int j = 0; j < 8; ++j) {
                const double t = p / 4.0 - (j - 3);  // distance from sample m + (j - 3)
                const double sinc = std::abs(t) < 1e-9 ? 1.0 : std::sin(kPi * t) / (kPi * t);
                const double w = std::abs(t) >= 4.0 ? 0.0 : 0.5 + 0.5 * std::cos(kPi * t / 4.0);
                c[p][j] = static_cast<float>(sinc * w);
            }
    }
};
const TpCoefs kTp;

double toLufs(double energy) noexcept { return energy > 1e-12 ? -0.691 + 10.0 * std::log10(energy) : -200.0; }
}  // namespace

float LoudnessMeter::TruePeak::push(float x) noexcept {
    hist[static_cast<std::size_t>(pos)] = x;
    pos = (pos + 1) & 7;
    // hist in time order: oldest at pos. Sample m (the 4th oldest) is the reference.
    float s[8];
    for (int j = 0; j < 8; ++j) s[j] = hist[static_cast<std::size_t>((pos + j) & 7)];
    float best = std::abs(s[3]);
    for (int p = 1; p < 4; ++p) {
        float v = 0.0f;
        for (int j = 0; j < 8; ++j) v += kTp.c[p][j] * s[j];
        best = std::max(best, std::abs(v));
    }
    return best;
}

int LoudnessMeter::bin(double lufs) noexcept { return std::clamp(static_cast<int>((lufs + 70.0) * 10.0), 0, kBins - 1); }
double LoudnessMeter::fromBin(int b) noexcept { return -70.0 + (b + 0.5) / 10.0; }

void LoudnessMeter::prepare(double sampleRate) {
    sr_ = sampleRate > 0 ? sampleRate : 48000.0;
    // K-weighting: a high shelf (+4 dB above ~1.7 kHz) and a high pass (~38 Hz), as in BS.1770.
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan(kPi * f0 / sr_), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf_.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelf_.b1 = 2.0 * (K * K - Vh) / a0;
        shelf_.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf_.a1 = 2.0 * (K * K - 1.0) / a0;
        shelf_.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan(kPi * f0 / sr_);
        const double a0 = 1.0 + K / Q + K * K;
        highpass_.b0 = 1.0;
        highpass_.b1 = -2.0;
        highpass_.b2 = 1.0;
        highpass_.a1 = 2.0 * (K * K - 1.0) / a0;
        highpass_.a2 = (1.0 - K / Q + K * K) / a0;
    }
    blockLen_ = std::max(1, static_cast<int>(sr_ * 0.1));
    histEnergy_.assign(kBins, 0.0);
    histCount_.assign(kBins, 0);
    stCount_.assign(kBins, 0);
    reset();
}

void LoudnessMeter::reset() noexcept {
    for (auto* f : {&shelf_, &highpass_})
        for (auto& ch : f->z) ch[0] = ch[1] = 0.0;
    blockFill_ = 0;
    blockSum_ = 0;
    blocks_.fill(0.0);
    blockCount_ = blockPos_ = 0;
    std::fill(histEnergy_.begin(), histEnergy_.end(), 0.0);
    std::fill(histCount_.begin(), histCount_.end(), 0);
    std::fill(stCount_.begin(), stCount_.end(), 0);
    momentary_ = shortTerm_ = integrated_ = kSilence;
    range_ = 0.0f;
    truePeak_ = -120.0f;
    tpL_ = tpR_ = {};
}

void LoudnessMeter::process(const float* l, const float* r, int n) noexcept {
    if (histEnergy_.empty()) return;
    float tp = 0.0f;
    for (int i = 0; i < n; ++i) {
        const double kl = highpass_.tick(shelf_.tick(l[i], 0), 0);
        double sum = kl * kl;
        tp = std::max(tp, tpL_.push(l[i]));
        if (r) {
            const double kr = highpass_.tick(shelf_.tick(r[i], 1), 1);
            sum += kr * kr;
            tp = std::max(tp, tpR_.push(r[i]));
        }
        blockSum_ += sum;
        if (++blockFill_ >= blockLen_) finishBlock();
    }
    truePeak_ = std::max(truePeak_, 20.0f * std::log10(std::max(tp, 1e-6f)));
}

void LoudnessMeter::finishBlock() noexcept {
    blocks_[static_cast<std::size_t>(blockPos_)] = blockSum_ / blockFill_;
    blockPos_ = (blockPos_ + 1) % 30;
    blockCount_ = std::min(blockCount_ + 1, 1 << 30);
    blockSum_ = 0;
    blockFill_ = 0;
    const auto mean = [this](int count) {
        double e = 0.0;
        for (int k = 1; k <= count; ++k) e += blocks_[static_cast<std::size_t>((blockPos_ - k + 30) % 30)];
        return e / count;
    };
    if (blockCount_ >= 4) {
        const double e = mean(4);
        const double m = toLufs(e);
        momentary_ = static_cast<float>(std::max(m, static_cast<double>(kSilence)));
        if (m > -70.0) {  // absolute gate
            const int b = bin(m);
            histEnergy_[static_cast<std::size_t>(b)] += e;
            ++histCount_[static_cast<std::size_t>(b)];
        }
        // Integrated: blocks above (ungated mean - 10 LU).
        double sumE = 0.0;
        long long cnt = 0;
        for (int b = 0; b < kBins; ++b) {
            sumE += histEnergy_[static_cast<std::size_t>(b)];
            cnt += histCount_[static_cast<std::size_t>(b)];
        }
        if (cnt > 0) {
            const double gate = toLufs(sumE / static_cast<double>(cnt)) - 10.0;
            double e2 = 0.0;
            long long c2 = 0;
            for (int b = bin(gate); b < kBins; ++b) {
                if (fromBin(b) < gate) continue;
                e2 += histEnergy_[static_cast<std::size_t>(b)];
                c2 += histCount_[static_cast<std::size_t>(b)];
            }
            if (c2 > 0) integrated_ = static_cast<float>(toLufs(e2 / static_cast<double>(c2)));
        }
    }
    if (blockCount_ >= 30) {
        const double st = toLufs(mean(30));
        shortTerm_ = static_cast<float>(std::max(st, static_cast<double>(kSilence)));
        if (st > -70.0) ++stCount_[static_cast<std::size_t>(bin(st))];
        // Loudness range: spread (10th..95th percentile) of short-term values above (mean - 20 LU).
        double e = 0.0;
        long long c = 0;
        for (int b = 0; b < kBins; ++b)
            if (stCount_[static_cast<std::size_t>(b)] > 0) {
                e += stCount_[static_cast<std::size_t>(b)] * std::pow(10.0, (fromBin(b) + 0.691) / 10.0);
                c += stCount_[static_cast<std::size_t>(b)];
            }
        if (c > 1) {
            const int g = bin(toLufs(e / static_cast<double>(c)) - 20.0);
            long long total = 0;
            for (int b = g; b < kBins; ++b) total += stCount_[static_cast<std::size_t>(b)];
            if (total > 1) {
                const auto percentile = [&](double q) {
                    const auto target = static_cast<long long>(q * static_cast<double>(total - 1));
                    long long acc = 0;
                    for (int b = g; b < kBins; ++b) {
                        acc += stCount_[static_cast<std::size_t>(b)];
                        if (acc > target) return fromBin(b);
                    }
                    return fromBin(kBins - 1);
                };
                range_ = static_cast<float>(percentile(0.95) - percentile(0.10));
            }
        }
    } else {
        shortTerm_ = kSilence;
    }
}

}  // namespace pat::dsp
