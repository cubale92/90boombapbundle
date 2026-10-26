#pragma once
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DspUtils.h"

namespace era
{
/** UI-side FFT analyzer fed from an AnalyzerFifo. Call update() from a timer,
    then createPath() to draw. Uses a 4.5 dB/oct tilt so pink-ish music reads flat. */
class SpectrumAnalyzer
{
public:
    explicit SpectrumAnalyzer (int fftOrder = 12);

    void setSampleRate (double sr) { sampleRate = sr; }
    /** Returns true if new data was analysed. */
    bool update (AnalyzerFifo& fifo);

    juce::Path createPath (juce::Rectangle<float> area, float minDb, float maxDb,
                           float minHz, float maxHz, bool closed) const;
    float getDbAt (float hz) const;

private:
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    int fftSize;
    std::vector<float> ring, fftData, smoothed, scratch;
    int writePos = 0, newSamples = 0;
    double sampleRate = 44100.0;
};

/** Log-frequency helpers shared by all displays. */
inline float freqToX (float hz, float minHz, float maxHz, juce::Rectangle<float> r)
{
    return r.getX() + r.getWidth() * std::log (hz / minHz) / std::log (maxHz / minHz);
}
inline float xToFreq (float x, float minHz, float maxHz, juce::Rectangle<float> r)
{
    return minHz * std::pow (maxHz / minHz, (x - r.getX()) / r.getWidth());
}
void drawFrequencyGrid (juce::Graphics&, juce::Rectangle<float>, float minHz, float maxHz);
} // namespace era
