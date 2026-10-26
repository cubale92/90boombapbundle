#include "SpectrumAnalyzer.h"
#include "EraTheme.h"

namespace era
{
SpectrumAnalyzer::SpectrumAnalyzer (int order)
    : fft (order), window ((size_t) (1 << order), juce::dsp::WindowingFunction<float>::hann, true), fftSize (1 << order)
{
    ring.assign ((size_t) fftSize, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    smoothed.assign ((size_t) fftSize / 2 + 1, -120.0f);
    scratch.assign (8192, 0.0f);
}

bool SpectrumAnalyzer::update (AnalyzerFifo& fifo)
{
    int got;
    while ((got = fifo.pull (scratch.data(), (int) scratch.size())) > 0)
    {
        for (int i = 0; i < got; ++i)
        {
            ring[(size_t) writePos] = scratch[(size_t) i];
            writePos = (writePos + 1) % fftSize;
        }
        newSamples += got;
    }

    if (newSamples < fftSize / 4)
        return false;
    newSamples = 0;

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((writePos + i) % fftSize)];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const float norm = 2.0f / (float) fftSize; // window is normalised, so a full-scale sine reads 0 dB
    for (int i = 0; i <= fftSize / 2; ++i)
    {
        const float hz = (float) i * (float) sampleRate / (float) fftSize;
        const float tilt = hz > 1.0f ? 4.5f * std::log2 (hz / 1000.0f) : 0.0f;
        const float db = gainToDb (fftData[(size_t) i] * norm, -140.0f) + tilt;
        auto& s = smoothed[(size_t) i];
        s = db > s ? s + 0.6f * (db - s) : s + 0.25f * (db - s);
    }
    return true;
}

float SpectrumAnalyzer::getDbAt (float hz) const
{
    const float bin = hz * (float) fftSize / (float) sampleRate;
    const int b0 = juce::jlimit (0, fftSize / 2 - 1, (int) bin);
    const float frac = juce::jlimit (0.0f, 1.0f, bin - (float) b0);
    return smoothed[(size_t) b0] + frac * (smoothed[(size_t) b0 + 1] - smoothed[(size_t) b0]);
}

juce::Path SpectrumAnalyzer::createPath (juce::Rectangle<float> r, float minDb, float maxDb,
                                         float minHz, float maxHz, bool closed) const
{
    juce::Path p;
    const float binHz = (float) sampleRate / (float) fftSize;
    bool started = false;
    for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
    {
        const float f0 = xToFreq (x - 1.0f, minHz, maxHz, r);
        const float f1 = xToFreq (x + 1.0f, minHz, maxHz, r);
        float db;
        if (f1 - f0 > binHz)
        {
            // several bins per pixel: take the max so peaks stay visible
            db = -200.0f;
            for (int b = (int) (f0 / binHz); b <= (int) (f1 / binHz) && b <= fftSize / 2; ++b)
                db = std::max (db, smoothed[(size_t) juce::jmax (0, b)]);
        }
        else
            db = getDbAt (xToFreq (x, minHz, maxHz, r));

        const float y = juce::jmap (juce::jlimit (minDb - 6.0f, maxDb, db), minDb, maxDb, r.getBottom(), r.getY());
        if (! started) { p.startNewSubPath (x, closed ? r.getBottom() : y); if (closed) p.lineTo (x, y); started = true; }
        else p.lineTo (x, y);
    }
    if (closed && started)
    {
        p.lineTo (r.getRight(), r.getBottom());
        p.closeSubPath();
    }
    return p;
}

void drawFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> r, float minHz, float maxHz)
{
    const float freqs[] = { 20, 30, 40, 50, 60, 80, 100, 200, 300, 400, 500, 600, 800, 1000,
                            2000, 3000, 4000, 5000, 6000, 8000, 10000, 20000 };
    for (float f : freqs)
    {
        if (f < minHz || f > maxHz) continue;
        const float x = freqToX (f, minHz, maxHz, r);
        const bool major = (f == 100 || f == 1000 || f == 10000);
        g.setColour (major ? Colours::gridStrong : Colours::grid.withAlpha (0.6f));
        g.drawVerticalLine (juce::roundToInt (x), r.getY(), r.getBottom());
    }
    g.setFont (font (10.0f, true));
    g.setColour (Colours::textDim);
    const float labels[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (float f : labels)
    {
        const float x = freqToX (f, minHz, maxHz, r);
        const auto txt = f >= 1000 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f);
        g.drawText (txt, juce::Rectangle<float> (x - 20, r.getBottom() - 15, 40, 13), juce::Justification::centred);
    }
}
} // namespace era
