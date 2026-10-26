#pragma once
#include <cmath>
#include <complex>
#include <algorithm>
#include <atomic>
#include <juce_core/juce_core.h>

namespace era
{
inline float dbToGain (float db)          { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g, float floorDb = -120.0f)
{
    return g > 0.0f ? std::max (floorDb, 20.0f * std::log10 (g)) : floorDb;
}

/** Atomic "max" helper used for meters written on the audio thread and read by the UI. */
inline void atomicMax (std::atomic<float>& a, float v)
{
    float cur = a.load (std::memory_order_relaxed);
    while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
}

//==============================================================================
/** Cytomic / Andrew Simper trapezoidal SVF. Modulation-stable, so it can be swept
    smoothly while the EQ nodes are dragged. The same coefficients are used to
    compute the exact frequency response for the display. */
enum class SvfType { Bell, LowShelf, HighShelf, LowPass, HighPass, Notch, BandPass };

struct SvfCoeffs
{
    double g = 0.1, k = 1.0, m0 = 1.0, m1 = 0.0, m2 = 0.0;
    float a1 = 0, a2 = 0, a3 = 0;
    float fm0 = 1, fm1 = 0, fm2 = 0;

    void finalise()
    {
        const double A1 = 1.0 / (1.0 + g * (g + k));
        a1 = (float) A1;
        a2 = (float) (g * A1);
        a3 = (float) (g * g * A1);
        fm0 = (float) m0; fm1 = (float) m1; fm2 = (float) m2;
    }

    /** Exact magnitude response (bilinear-transform prototype evaluated with prewarping). */
    std::complex<double> response (double freqHz, double sampleRate) const
    {
        const double w = std::tan (juce::MathConstants<double>::pi * std::min (freqHz, sampleRate * 0.4999) / sampleRate) / g;
        const std::complex<double> s (0.0, w);
        const auto den = s * s + k * s + 1.0;
        return (m0 * den + m1 * s + m2) / den;
    }

    static SvfCoeffs make (SvfType type, double freq, double q, double gainDb, double sr)
    {
        SvfCoeffs c;
        freq = juce::jlimit (5.0, sr * 0.49, freq);
        q = std::max (0.025, q);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double base = std::tan (juce::MathConstants<double>::pi * freq / sr);
        c.g = base;
        c.k = 1.0 / q;
        switch (type)
        {
            case SvfType::LowPass:   c.m0 = 0; c.m1 = 0;      c.m2 = 1;  break;
            case SvfType::HighPass:  c.m0 = 1; c.m1 = -c.k;   c.m2 = -1; break;
            case SvfType::BandPass:  c.m0 = 0; c.m1 = 1;      c.m2 = 0;  break;
            case SvfType::Notch:     c.m0 = 1; c.m1 = -c.k;   c.m2 = 0;  break;
            case SvfType::Bell:
                c.k = 1.0 / (q * A);
                c.m0 = 1; c.m1 = c.k * (A * A - 1.0); c.m2 = 0;
                break;
            case SvfType::LowShelf:
                c.g = base / std::sqrt (A);
                c.m0 = 1; c.m1 = c.k * (A - 1.0); c.m2 = A * A - 1.0;
                break;
            case SvfType::HighShelf:
                c.g = base * std::sqrt (A);
                c.m0 = A * A; c.m1 = c.k * (1.0 - A) * A; c.m2 = 1.0 - A * A;
                break;
        }
        c.finalise();
        return c;
    }
};

struct SvfState
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }

    inline float process (float v0, const SvfCoeffs& c) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return c.fm0 * v0 + c.fm1 * v1 + c.fm2 * v2;
    }
};

//==============================================================================
struct OnePole
{
    float a = 0, z = 0;
    void setTime (float ms, double sr) { a = ms <= 0 ? 0.0f : (float) std::exp (-1.0 / (ms * 0.001 * sr)); }
    inline float process (float x) noexcept { z = x + a * (z - x); return z; }
    void reset (float v = 0) { z = v; }
};

struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (double sr) { r = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 8.0 / sr); }
    inline float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

/** Simple RBJ biquad (Direct Form I) used where a fixed filter is enough. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void reset() { x1 = x2 = y1 = y2 = 0; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }

    void setCoefs (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    void highPass (double f, double q, double sr)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
        const double cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoefs ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void lowPass (double f, double q, double sr)
    {
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
        const double cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoefs ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void lowShelf (double f, double q, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
        const double cs = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2 * std::sqrt (A) * al;
        setCoefs (A * ((A + 1) - (A - 1) * cs + sa), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sa),
                  (A + 1) + (A - 1) * cs + sa, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sa);
    }
    void highShelf (double f, double q, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
        const double cs = std::cos (w), al = std::sin (w) / (2.0 * q), sa = 2 * std::sqrt (A) * al;
        setCoefs (A * ((A + 1) + (A - 1) * cs + sa), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sa),
                  (A + 1) - (A - 1) * cs + sa, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sa);
    }
    void peak (double f, double q, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0);
        const double w = juce::MathConstants<double>::twoPi * juce::jlimit (5.0, sr * 0.49, f) / sr;
        const double cs = std::cos (w), al = std::sin (w) / (2.0 * q);
        setCoefs (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
};

//==============================================================================
/** Lock-free single-producer/single-consumer sample FIFO for the analyzers. */
class AnalyzerFifo
{
public:
    AnalyzerFifo() : fifo (1 << 15) { buffer.resize (1 << 15); }

    void push (const float* data, int num) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (num, s1, n1, s2, n2);
        if (n1 > 0) std::copy (data, data + n1, buffer.data() + s1);
        if (n2 > 0) std::copy (data + n1, data + n1 + n2, buffer.data() + s2);
        fifo.finishedWrite (n1 + n2);
    }

    int pull (float* dest, int maxNum) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxNum, s1, n1, s2, n2);
        if (n1 > 0) std::copy (buffer.data() + s1, buffer.data() + s1 + n1, dest);
        if (n2 > 0) std::copy (buffer.data() + s2, buffer.data() + s2 + n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

    int available() const noexcept { return fifo.getNumReady(); }

private:
    juce::AbstractFifo fifo;
    std::vector<float> buffer;
};

/** History frames for the scrolling level / gain-reduction displays. */
struct HistoryFrame { float inDb = -100, outDb = -100, grDb = 0; };

class HistoryFifo
{
public:
    HistoryFifo() : fifo (2048) { frames.resize (2048); }

    void push (const HistoryFrame& f) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) frames[(size_t) s1] = f;
        fifo.finishedWrite (n1);
    }

    bool pop (HistoryFrame& f) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (1, s1, n1, s2, n2);
        if (n1 > 0) f = frames[(size_t) s1];
        fifo.finishedRead (n1);
        return n1 > 0;
    }

private:
    juce::AbstractFifo fifo;
    std::vector<HistoryFrame> frames;
};

/** Accumulates per-sample levels into fixed-time frames for HistoryFifo. */
struct HistoryAccumulator
{
    int frameLen = 256, count = 0;
    HistoryFrame cur;
    void prepare (double sr, double frameMs = 6.0) { frameLen = std::max (16, (int) (sr * frameMs * 0.001)); count = 0; cur = {}; }

    inline void add (float inAbs, float outAbs, float grDb, HistoryFifo& fifo) noexcept
    {
        cur.inDb = std::max (cur.inDb, gainToDb (inAbs, -100.0f));
        cur.outDb = std::max (cur.outDb, gainToDb (outAbs, -100.0f));
        cur.grDb = std::max (cur.grDb, grDb);
        if (++count >= frameLen) { fifo.push (cur); cur = {}; count = 0; }
    }
};
} // namespace era
