#include "dsp/eq/Oversampler2x.h"

#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;

double ipow(double x, int n) noexcept {
    double r = 1.0;
    for (int i = 0; i < n; ++i) r *= x;
    return r;
}

double accNum(double q, int order, int c) noexcept {
    double acc = 0.0, term;
    int i = 0, sign = 1;
    do {
        term = ipow(q, i * (i + 1)) * std::sin((i * 2 + 1) * c * kPi / order) * sign;
        acc += term;
        sign = -sign;
        ++i;
    } while (std::abs(term) > 1e-100 && i < 64);
    return acc;
}

double accDen(double q, int order, int c) noexcept {
    double acc = 0.0, term;
    int i = 1, sign = -1;
    do {
        term = ipow(q, i * i) * std::cos(i * 2 * c * kPi / order) * sign;
        acc += term;
        sign = -sign;
        ++i;
    } while (std::abs(term) > 1e-100 && i < 64);
    return acc;
}
}  // namespace

std::array<double, HalfBandDesign::kCoefs> HalfBandDesign::compute(double transition) noexcept {
    // Elliptic half-band design (transition parameter -> allpass coefficients).
    double k = std::tan((1.0 - transition * 2.0) * kPi / 4.0);
    k *= k;
    const double kksqrt = std::pow(1.0 - k * k, 0.25);
    const double e = 0.5 * (1.0 - kksqrt) / (1.0 + kksqrt);
    const double e2 = e * e, e4 = e2 * e2;
    const double q = e * (1.0 + e4 * (2.0 + e4 * (15.0 + 150.0 * e4)));
    const int order = kCoefs * 2 + 1;
    std::array<double, kCoefs> c{};
    for (int i = 0; i < kCoefs; ++i) {
        const int ci = i + 1;
        const double num = accNum(q, order, ci) * std::pow(q, 0.25);
        const double den = accDen(q, order, ci) + 0.5;
        const double ww = num / den;
        const double wwsq = ww * ww;
        const double x = std::sqrt((1.0 - wwsq * k) * (1.0 - wwsq / k)) / (1.0 + wwsq);
        c[static_cast<std::size_t>(i)] = (1.0 - x) / (1.0 + x);
    }
    return c;
}

Oversampler2x::Oversampler2x() noexcept : coef_(HalfBandDesign::compute(0.04)) {}

void Oversampler2x::reset() noexcept {
    up_ = {};
    down_ = {};
}

void Oversampler2x::stage(const std::array<double, HalfBandDesign::kCoefs>& c, Chain& s, double& even, double& odd) noexcept {
    // Two allpass chains side by side: even coefficients on one path, odd on the other.
    for (int i = 0; i < HalfBandDesign::kCoefs; i += 2) {
        const auto a = static_cast<std::size_t>(i), b = a + 1;
        const double t0 = s.x[a], t1 = s.x[b];
        s.x[a] = even;
        s.x[b] = odd;
        even = (even - s.y[a]) * c[a] + t0;
        odd = (odd - s.y[b]) * c[b] + t1;
        s.y[a] = even;
        s.y[b] = odd;
    }
}

void Oversampler2x::up(float in, float& out0, float& out1) noexcept {
    double even = in, odd = in;
    stage(coef_, up_, even, odd);
    out0 = static_cast<float>(even);
    out1 = static_cast<float>(odd);
}

float Oversampler2x::down(float in0, float in1) noexcept {
    double even = in1, odd = in0;
    stage(coef_, down_, even, odd);
    return static_cast<float>(0.5 * (even + odd));
}

}  // namespace pat::dsp
