#include "common/core/Fft.h"

#include <cassert>
#include <cmath>
#include <numbers>
#include <utility>

namespace pat {

Fft::Fft(int size) : n_(size), bitrev_(static_cast<std::size_t>(size)), twiddles_(static_cast<std::size_t>(size / 2)) {
    assert(size >= 2 && (size & (size - 1)) == 0);
    int bits = 0;
    while ((1 << bits) < size) ++bits;
    for (int i = 0; i < size; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev_[static_cast<std::size_t>(i)] = r;
    }
    for (int k = 0; k < size / 2; ++k) {
        const double a = -2.0 * std::numbers::pi * k / size;
        twiddles_[static_cast<std::size_t>(k)] = {static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a))};
    }
}

void Fft::transform(std::complex<float>* data) const noexcept {
    for (int i = 0; i < n_; ++i) {
        const int j = bitrev_[static_cast<std::size_t>(i)];
        if (j > i) std::swap(data[i], data[j]);
    }
    for (int len = 2; len <= n_; len <<= 1) {
        const int half = len / 2;
        const int step = n_ / len;
        for (int start = 0; start < n_; start += len) {
            for (int k = 0; k < half; ++k) {
                const auto w = twiddles_[static_cast<std::size_t>(k * step)];
                const auto u = data[start + k];
                const auto v = data[start + k + half] * w;
                data[start + k] = u + v;
                data[start + k + half] = u - v;
            }
        }
    }
}

void Fft::inverse(std::complex<float>* data) const noexcept {
    for (int i = 0; i < n_; ++i) data[i] = std::conj(data[i]);
    transform(data);
    const float scale = 1.0f / static_cast<float>(n_);
    for (int i = 0; i < n_; ++i) data[i] = std::conj(data[i]) * scale;
}

void Fft::realMagnitudes(const float* input, float* magnitudes, std::complex<float>* scratch) const noexcept {
    for (int i = 0; i < n_; ++i) scratch[i] = {input[i], 0.0f};
    transform(scratch);
    for (int k = 0; k <= n_ / 2; ++k) magnitudes[k] = std::abs(scratch[k]);
}

}  // namespace pat
