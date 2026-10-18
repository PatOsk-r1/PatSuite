// PatQpro filter design. Every band is designed as an analog prototype (normalised to its
// frequency) made of first/second order sections, then turned into digital biquads with the
// bilinear transform. The analog prototype is also what the linear-phase mode and the display
// use as the "ideal" curve.
//
// Band types: bell, low/high shelf (Butterworth-style shelves of any slope, the frequency is the
// half-gain point), low/high cut (Butterworth, 6..96 dB/oct, Q adds resonance), notch, band pass,
// tilt shelf (lows down / highs up around the frequency) and flat tilt (a straight tilt of the
// whole spectrum, pivoting at the frequency).
#pragma once

#include <complex>

namespace pat::dsp {

struct EqBand {
    enum Type { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch, BandPass, TiltShelf, FlatTilt, NumTypes };
    enum Place { Stereo = 0, Left, Right, Mid, Side };
    static constexpr int kNumSlopes = 10;  // 6, 12, 18, 24, 30, 36, 48, 72, 96 dB/oct, brickwall
    static int slopeDb(int index) noexcept;
    static const char* typeName(int type) noexcept;
    static bool hasGain(int type) noexcept;   // bell, shelves, tilts
    static bool hasSlope(int type) noexcept;  // shelves, cuts, band pass, tilt shelf

    bool used = false;   // the band exists
    bool active = true;  // false = bypassed
    int type = Bell;
    float freq = 1000.0f, gain = 0.0f, q = 1.0f;
    int slope = 1;  // index (default 12 dB/oct)
    int place = Stereo;
    bool dynamic = false;
    float dynRange = 0.0f;      // dB the gain may move (negative = cut when loud)
    float dynThreshold = -30.0f;
    bool dynSidechain = false;  // detector listens to the sidechain input
};

// Normalised analog section: (b2 s^2 + b1 s + b0) / (a2 s^2 + a1 s + a0), s = j f / fc.
struct AnalogSection {
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
};

// Digital biquad, normalised (a0 = 1).
struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

class EqDesign {
public:
    static constexpr int kMaxSections = 8;

    // Analog sections of a band with the given gain (dB). Returns the count (0 = passes through).
    static int analog(const EqBand& b, float gainDb, AnalogSection* out) noexcept;
    // Digital biquads at sample rate fs. Returns the count.
    static int digital(const EqBand& b, float gainDb, double fs, Biquad* out) noexcept;

    static std::complex<double> analogResponse(const AnalogSection* s, int n, double hz, double fc) noexcept;
    static std::complex<double> digitalResponse(const Biquad* s, int n, double hz, double fs) noexcept;
    // Ideal response of a band at hz (brickwall cuts are a perfect step).
    static double idealMagnitude(const EqBand& b, float gainDb, double hz) noexcept;
};

}  // namespace pat::dsp
