// Live spectrum analyser fed by a lock-free FIFO from the audio thread.
//   Low:    FFT 1024 reduced to 48 bands, repaint only when new audio arrived
//   Medium: FFT 2048, one point per pixel, smoothed fall
//   High:   FFT 4096, smoothed, glowing curve
// "Neon fixed": same curve without glow or falling animation, refreshed every 100 ms.
// Optional highlighted band (e.g. the de-esser's sibilance zone).
#pragma once

#include "common/core/Fft.h"
#include "common/core/SpscFifo.h"
#include "common/ui/LayeredCanvas.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace pat::ui {

class SpectrumView final : public LayeredCanvas {
public:
    SpectrumView(PerfSettings& settings, SpscFifo<float>& fifo, std::function<double()> sampleRate, juce::Colour colour);

    void tick(float dt);  // pull audio, analyse, repaint if needed (call from the FrameClock)
    void setHighlight(float lowHz, float highHz, juce::Colour c);
    void clearHighlight() { highlight_ = false; }
    void useTestSignal(bool on) { testSignal_ = on; }  // diagnostic snapshots

    static float xForHz(float hz, float width) noexcept;  // log axis 20 Hz .. 20 kHz

protected:
    void paintStatic(juce::Graphics&) override;
    void paintDynamic(juce::Graphics&) override;

private:
    const Fft& fftFor(int size);

    SpscFifo<float>& fifo_;
    std::function<double()> sampleRate_;
    juce::Colour colour_;
    std::map<int, std::unique_ptr<Fft>> ffts_;
    std::vector<float> history_, scratchIn_, mags_, window_;
    std::vector<std::complex<float>> scratch_;
    std::vector<float> display_;  // dB per display point
    bool highlight_ = false, testSignal_ = false;
    float hlLow_ = 0, hlHigh_ = 0;
    juce::Colour hlColour_;
    double testPhase_ = 0;
    juce::Random rng_;
};

}  // namespace pat::ui
