#include "dsp/eq/EqFilters.h"

#include <algorithm>
#include <cmath>

namespace pat::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kSlopes[EqBand::kNumSlopes] = {6, 12, 18, 24, 30, 36, 48, 72, 96, 96};

double dbToGain(double db) noexcept { return std::pow(10.0, db / 20.0); }

// Butterworth angle term 2*sin(theta_m) of pole pair m (0-based) for order n.
double bwDamping(int m, int n) noexcept { return 2.0 * std::sin((2.0 * m + 1.0) * kPi / (2.0 * n)); }

// Resonance: the sharpest pair of a Butterworth set gets Q (0.707 = flat Butterworth).
double resonance(double damping, bool sharpest, float q) noexcept {
    return sharpest ? damping * (0.70710678 / std::clamp(static_cast<double>(q), 0.1, 40.0)) : damping;
}

// Butterworth-style low shelf of order n, DC gain g (linear), half-gain point at 1.
int lowShelf(int n, double g, float q, AnalogSection* out, int count, bool high) noexcept {
    const double a = std::pow(g, 1.0 / (2.0 * n));
    const int pairs = n / 2;
    for (int m = 0; m < pairs && count < EqDesign::kMaxSections; ++m) {
        const double d = resonance(bwDamping(m, n), m == 0, q);
        AnalogSection s;
        if (!high) {  // (s^2 + d a s + a^2) / (s^2 + d s / a + 1/a^2)
            s.b2 = 1;
            s.b1 = d * a;
            s.b0 = a * a;
            s.a2 = 1;
            s.a1 = d / a;
            s.a0 = 1.0 / (a * a);
        } else {  // s -> 1/s: (a^2 s^2 + d a s + 1) / (s^2 / a^2 + d s / a + 1)
            s.b2 = a * a;
            s.b1 = d * a;
            s.b0 = 1;
            s.a2 = 1.0 / (a * a);
            s.a1 = d / a;
            s.a0 = 1;
        }
        out[count++] = s;
    }
    if ((n & 1) && count < EqDesign::kMaxSections) {
        AnalogSection s;
        if (!high) {  // (s + a) / (s + 1/a)
            s.b1 = 1;
            s.b0 = a;
            s.a1 = 1;
            s.a0 = 1.0 / a;
        } else {  // (a s + 1) / (s / a + 1)
            s.b1 = a;
            s.b0 = 1;
            s.a1 = 1.0 / a;
            s.a0 = 1;
        }
        out[count++] = s;
    }
    return count;
}

int butterworth(int n, float q, bool highpass, AnalogSection* out) noexcept {
    int count = 0;
    for (int m = 0; m < n / 2 && count < EqDesign::kMaxSections; ++m) {
        AnalogSection s;
        s.a2 = 1;
        s.a1 = resonance(bwDamping(m, n), m == 0, q);
        s.a0 = 1;
        s.b2 = highpass ? 1 : 0;  // s^2 / (...) or 1 / (...)
        s.b0 = highpass ? 0 : 1;
        out[count++] = s;
    }
    if ((n & 1) && count < EqDesign::kMaxSections) {
        AnalogSection s;
        s.a1 = 1;
        s.a0 = 1;
        if (highpass) {
            s.b1 = 1;
            s.b0 = 0;
        } else {
            s.b0 = 1;
        }
        out[count++] = s;
    }
    return count;
}
}  // namespace

int EqBand::slopeDb(int index) noexcept { return kSlopes[std::clamp(index, 0, kNumSlopes - 1)]; }

const char* EqBand::typeName(int type) noexcept {
    static const char* names[] = {"Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt"};
    return names[std::clamp(type, 0, NumTypes - 1)];
}

bool EqBand::hasGain(int type) noexcept {
    return type == Bell || type == LowShelf || type == HighShelf || type == TiltShelf || type == FlatTilt;
}

bool EqBand::hasSlope(int type) noexcept {
    return type == LowShelf || type == HighShelf || type == LowCut || type == HighCut || type == BandPass || type == TiltShelf;
}

int EqDesign::analog(const EqBand& b, float gainDb, AnalogSection* out) noexcept {
    const double q = std::clamp(static_cast<double>(b.q), 0.025, 40.0);
    const int order = std::clamp(EqBand::slopeDb(b.slope) / 6, 1, 16);
    switch (b.type) {
        case EqBand::Bell: {
            if (std::abs(gainDb) < 1e-4f) return 0;
            const double A = std::pow(10.0, gainDb / 40.0);
            out[0] = {1, A / q, 1, 1, 1.0 / (A * q), 1};
            return 1;
        }
        case EqBand::LowShelf:
        case EqBand::HighShelf: {
            if (std::abs(gainDb) < 1e-4f) return 0;
            return lowShelf(std::min(order, 16), dbToGain(gainDb), b.q, out, 0, b.type == EqBand::HighShelf);
        }
        case EqBand::TiltShelf: {
            if (std::abs(gainDb) < 1e-4f) return 0;
            const int n = lowShelf(std::min(order, 16), dbToGain(gainDb), b.q, out, 0, true);
            const double half = dbToGain(-0.5 * gainDb);  // lows down by half, highs up by half
            out[0].b0 *= half;
            out[0].b1 *= half;
            out[0].b2 *= half;
            return n;
        }
        case EqBand::FlatTilt: {
            if (std::abs(gainDb) < 1e-4f) return 0;
            // Five first-order high shelves two octaves apart around the pivot: a near straight
            // tilt of gainDb across about ten octaves, 0 dB at the band frequency.
            const double step = dbToGain(gainDb / 5.0);
            const double a = std::sqrt(step);
            for (int k = 0; k < 5; ++k) {
                const double rel = std::pow(4.0, k - 2);  // centre of this shelf relative to the pivot
                // (a s/rel + 1) / (s/(a rel) + 1)
                out[k] = {1, a / rel, 0, 1, 1.0 / (a * rel), 0};
            }
            // Normalise to 0 dB at the pivot.
            const double atPivot = std::abs(analogResponse(out, 5, 1.0, 1.0));
            for (int k = 0; k < 5; ++k) {
                const double c = std::pow(atPivot, -1.0 / 5.0);
                out[k].b0 *= c;
                out[k].b1 *= c;
            }
            return 5;
        }
        case EqBand::LowCut: return butterworth(order, b.q, true, out);
        case EqBand::HighCut: return butterworth(order, b.q, false, out);
        case EqBand::Notch: out[0] = {1, 0, 1, 1, 1.0 / q, 1}; return 1;
        case EqBand::BandPass: {
            const int n = std::clamp(order / 2, 1, kMaxSections);
            for (int k = 0; k < n; ++k) out[k] = {0, 1.0 / q, 0, 1, 1.0 / q, 1};
            return n;
        }
        default: return 0;
    }
}

int EqDesign::digital(const EqBand& b, float gainDb, double fs, Biquad* out) noexcept {
    AnalogSection a[kMaxSections];
    const int n = analog(b, gainDb, a);
    const double fc = std::clamp(static_cast<double>(b.freq), 5.0, fs * 0.4999);
    const double K = std::tan(kPi * fc / fs);
    for (int i = 0; i < n; ++i) {
        const auto& s = a[i];
        double B0, B1, B2, A0, A1, A2;
        if (s.a2 == 0.0 && s.b2 == 0.0) {  // first order
            B0 = s.b1 + s.b0 * K;
            B1 = -s.b1 + s.b0 * K;
            B2 = 0;
            A0 = s.a1 + s.a0 * K;
            A1 = -s.a1 + s.a0 * K;
            A2 = 0;
        } else {
            const double K2 = K * K;
            B0 = s.b2 + s.b1 * K + s.b0 * K2;
            B1 = 2.0 * (s.b0 * K2 - s.b2);
            B2 = s.b2 - s.b1 * K + s.b0 * K2;
            A0 = s.a2 + s.a1 * K + s.a0 * K2;
            A1 = 2.0 * (s.a0 * K2 - s.a2);
            A2 = s.a2 - s.a1 * K + s.a0 * K2;
        }
        out[i] = {B0 / A0, B1 / A0, B2 / A0, A1 / A0, A2 / A0};
    }
    return n;
}

std::complex<double> EqDesign::analogResponse(const AnalogSection* s, int n, double hz, double fc) noexcept {
    const std::complex<double> j(0.0, hz / fc);
    std::complex<double> h(1.0, 0.0);
    for (int i = 0; i < n; ++i)
        h *= (s[i].b2 * j * j + s[i].b1 * j + s[i].b0) / (s[i].a2 * j * j + s[i].a1 * j + s[i].a0);
    return h;
}

std::complex<double> EqDesign::digitalResponse(const Biquad* s, int n, double hz, double fs) noexcept {
    const double w = 2.0 * kPi * hz / fs;
    const std::complex<double> z1 = std::polar(1.0, -w), z2 = z1 * z1;
    std::complex<double> h(1.0, 0.0);
    for (int i = 0; i < n; ++i) h *= (s[i].b0 + s[i].b1 * z1 + s[i].b2 * z2) / (1.0 + s[i].a1 * z1 + s[i].a2 * z2);
    return h;
}

double EqDesign::idealMagnitude(const EqBand& b, float gainDb, double hz) noexcept {
    if (b.slope == EqBand::kNumSlopes - 1 && (b.type == EqBand::LowCut || b.type == EqBand::HighCut)) {
        const bool pass = b.type == EqBand::LowCut ? hz >= b.freq : hz <= b.freq;
        return pass ? 1.0 : 0.0;
    }
    AnalogSection a[kMaxSections];
    const int n = analog(b, gainDb, a);
    return std::abs(analogResponse(a, n, hz, b.freq));
}

}  // namespace pat::dsp
