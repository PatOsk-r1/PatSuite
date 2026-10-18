// Radix-2 complex FFT. All tables are built in the constructor; transform() never allocates,
// so it is safe on the audio thread.
#pragma once

#include <complex>
#include <vector>

namespace pat {

class Fft {
public:
    explicit Fft(int size);  // size must be a power of two

    int size() const noexcept { return n_; }
    void transform(std::complex<float>* data) const noexcept;  // in place, forward
    void inverse(std::complex<float>* data) const noexcept;    // in place, scaled by 1/size

    // Magnitude spectrum of a real signal (bins 0..size/2). `scratch` must hold size() values.
    void realMagnitudes(const float* input, float* magnitudes, std::complex<float>* scratch) const noexcept;

private:
    int n_;
    std::vector<int> bitrev_;
    std::vector<std::complex<float>> twiddles_;
};

}  // namespace pat
