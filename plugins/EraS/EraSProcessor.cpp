#include "EraSProcessor.h"
#include "EraSEditor.h"

namespace eras
{
namespace
{
inline float baseShape (int mode, float v)
{
    switch (mode)
    {
        case Tape:    return std::tanh (v);
        case Tube:    return v >= 0.0f ? std::tanh (v) : v / (1.0f - 0.6f * v);   // softer negative half -> even harmonics
        case Console: { const float c = juce::jlimit (-1.5f, 1.5f, v); return c - c * c * c / 6.75f; }
        default:      return v;
    }
}

/** Pre-computed shaper: bias offset removed and small-signal gain normalised to 1. */
struct Shaper
{
    int mode = Tape;
    float b = 0, fb = 0, norm = 1;
    void set (int m, float bias)
    {
        mode = m;
        b = bias * 0.35f;
        fb = baseShape (m, b);
        const float d = (baseShape (m, b + 1.0e-3f) - baseShape (m, b - 1.0e-3f)) / 2.0e-3f;
        norm = d > 1.0e-4f ? 1.0f / d : 1.0f;
    }
    inline float operator() (float x) const noexcept { return (baseShape (mode, x + b) - fb) * norm; }
};

inline float converterClip (float x)
{
    const float a = std::abs (x);
    if (a < 0.95f) return x;
    return std::copysign (0.95f + 0.05f * std::tanh ((a - 0.95f) / 0.05f), x);
}
} // namespace

float shapeAnalog (int mode, float x, float bias)
{
    Shaper s;
    s.set (mode, bias);
    return s (x);
}

float shapeSampler (int mode, float x, int bits)
{
    const float q = (float) (1 << (juce::jlimit (2, 24, bits) - 1));
    x = converterClip (x);
    // The 26k mode truncates like the classic 12-bit drum samplers, the others round
    return mode == Sampler26 ? std::floor (x * q) / q : std::round (x * q) / q;
}

//==============================================================================
EraSProcessor::EraSProcessor() : EraProcessorBase ("Era-S", createLayout())
{
    pMode = raw ("mode"); pDrive = raw ("drive"); pBias = raw ("bias"); pTone = raw ("tone");
    pLowCut = raw ("lowcut"); pHighCut = raw ("highcut"); pBits = raw ("bits"); pRate = raw ("rate");
    pDust = raw ("dust"); pMix = raw ("mix"); pOut = raw ("output"); pAuto = raw ("autogain");
    initPresets (factoryPresets());
}

juce::AudioProcessorValueTreeState::ParameterLayout EraSProcessor::createLayout()
{
    using namespace era;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (makeChoice ("mode", "Mode", { "Tape", "Tube", "Console", "Sampler 26k", "Sampler 40k", "Vari-Rate" }, 0));
    l.add (makeFloat ("drive", "Drive", { 0.0f, 36.0f, 0.01f }, 6.0f, "dB", [] (float v) { return "+" + juce::String (v, 1) + " dB"; }));
    l.add (makeFloat ("bias", "Bias", { -100.0f, 100.0f, 0.1f }, 0.0f, "%", [] (float v) { return juce::String (v, 0) + " %"; }));
    l.add (makeFloat ("tone", "Tone", { -12.0f, 12.0f, 0.01f }, 0.0f, "dB",
                      [] (float v) { return std::abs (v) < 0.05f ? juce::String ("Flat") : (v < 0 ? "Dark " : "Bright ") + juce::String (std::abs (v), 1); }));
    l.add (makeFloat ("lowcut", "Low Cut", logRange (10.0f, 500.0f), 10.0f, "Hz",
                      [] (float v) { return v <= 10.5f ? juce::String ("Off") : formatHz (v); }));
    l.add (makeFloat ("highcut", "High Cut", logRange (1000.0f, 22000.0f), 22000.0f, "Hz",
                      [] (float v) { return v >= 21900.0f ? juce::String ("Off") : formatHz (v); }));
    l.add (makeFloat ("bits", "Bits", { 4.0f, 16.0f, 1.0f }, 12.0f, "bit", [] (float v) { return juce::String ((int) v) + " bit"; }));
    l.add (makeFloat ("rate", "Rate", logRange (2000.0f, 48000.0f), 26040.0f, "Hz", formatHz));
    l.add (makeFloat ("dust", "Dust", { 0.0f, 100.0f, 0.1f }, 0.0f, "%", [] (float v) { return v < 0.05f ? juce::String ("Off") : juce::String (v, 0) + " %"; }));
    l.add (makeFloat ("mix", "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", [] (float v) { return juce::String (v, 0) + " %"; }));
    l.add (makeFloat ("output", "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB", formatDb));
    l.add (makeBool ("autogain", "Auto Gain", true));
    return l;
}

void EraSProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    os = std::make_unique<juce::dsp::Oversampling<float>> (2, osFactorLog2,
             juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    os->initProcessing ((size_t) juce::jmax (1, maxBlock));
    os->reset();
    osSr = sr * (1 << osFactorLog2);
    latency = juce::roundToInt (os->getLatencyInSamples());
    setLatencySamples (latency);

    for (auto& c : ch)
    {
        c = Chan();
        c.dc.prepare (sr);
        c.dryDelay.assign ((size_t) latency + 1, 0.0f);
        c.clickHp.highPass (1200.0, 0.7, sr);
        c.preEmph.highShelf (2500.0, 0.7, 5.0, osSr);
        c.deEmph.highShelf (2500.0, 0.7, -5.0, osSr);
        c.headBump.peak (75.0, 0.8, 1.5, sr);
    }
    drive.reset (sr, 0.05);   drive.setCurrentAndTargetValue (era::dbToGain (pDrive->load()));
    mix.reset (sr, 0.05);     mix.setCurrentAndTargetValue (pMix->load() * 0.01f);
    outGain.reset (sr, 0.05); outGain.setCurrentAndTargetValue (era::dbToGain (pOut->load()));
    inMs = wetMs = 0; autoG = 1;
    lastMode = -1; lastLow = lastHigh = lastRate = -1; lastTone = -100;
    monoScratch.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    dryBuf.setSize (2, juce::jmax (1, maxBlock));
}

void EraSProcessor::updateFilters (int mode, float lowCut, float highCut, float tone, float rate)
{
    if (mode == lastMode && lowCut == lastLow && highCut == lastHigh && tone == lastTone && rate == lastRate)
        return;

    const bool modeChanged = mode != lastMode;
    lastMode = mode; lastLow = lowCut; lastHigh = highCut; lastTone = tone; lastRate = rate;

    for (auto& c : ch)
    {
        c.lowCut.highPass (lowCut, 0.707, sr);
        c.highCut.lowPass (highCut, 0.707, sr);
        c.tiltLo.lowShelf (450.0, 0.6, -tone * 0.5, sr);
        c.tiltHi.highShelf (2200.0, 0.6, tone * 0.5, sr);

        double aaIn = osSr * 0.45, aaOut = osSr * 0.45;
        if (mode == Sampler40) { aaIn = 17500.0; aaOut = 18500.0; }
        if (mode == VariRate)  { aaIn = aaOut = rate * 0.45; }
        c.aaIn[0].lowPass (aaIn, 0.54119610, osSr);  c.aaIn[1].lowPass (aaIn, 1.30656296, osSr);
        c.aaOut[0].lowPass (aaOut, 0.54119610, osSr); c.aaOut[1].lowPass (aaOut, 1.30656296, osSr);
        if (modeChanged)
        {
            c.aaIn[0].reset(); c.aaIn[1].reset(); c.aaOut[0].reset(); c.aaOut[1].reset();
            c.preEmph.reset(); c.deEmph.reset();
        }
    }
}

void EraSProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;
    if (os == nullptr || n > (int) monoScratch.size()) prepareToPlay (sr, n); // host exceeded announced block size

    const int mode = (int) pMode->load();
    const int bits = (int) pBits->load();
    const float lowCut = pLowCut->load(), highCut = pHighCut->load();
    const bool useLow = lowCut > 10.5f, useHigh = highCut < 21900.0f;
    const float tone = pTone->load();
    const float rate = mode == Sampler26 ? 26040.0f : (mode == Sampler40 ? 40000.0f : pRate->load());
    const float dust = pDust->load() * 0.01f;
    const bool autoGain = pAuto->load() > 0.5f;
    updateFilters (mode, lowCut, highCut, tone, rate);

    drive.setTargetValue (era::dbToGain (pDrive->load()));
    mix.setTargetValue (pMix->load() * 0.01f);
    outGain.setTargetValue (era::dbToGain (pOut->load()));

    // Keep the dry signal (for mix + auto gain) and apply drive
    float inPeak = 0;
    auto& dryCopy = dryBuf;
    for (int c = 0; c < numCh; ++c)
        dryCopy.copyFrom (c, 0, buffer, c, 0, n);
    for (int i = 0; i < n; ++i)
    {
        const float d = drive.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            auto* x = buffer.getWritePointer (c);
            inPeak = std::max (inPeak, std::abs (x[i]));
            x[i] *= d;
        }
    }
    era::atomicMax (meterIn, inPeak);
    driveDisplay = inPeak * drive.getCurrentValue();

    // ---- oversampled non-linear stage ---------------------------------------
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = os->processSamplesUp (block);
    const int upN = (int) up.getNumSamples();

    Shaper shaper;
    shaper.set (mode, pBias->load() * 0.01f);
    const float holdInc = rate / (float) osSr;
    const float biasOffset = pBias->load() * 0.001f;

    for (int c = 0; c < numCh; ++c)
    {
        auto* x = up.getChannelPointer ((size_t) c);
        auto& s = ch[c];
        if (! isSampler (mode))
        {
            for (int i = 0; i < upN; ++i)
            {
                float v = x[i];
                if (mode == Tape)
                    v = s.deEmph.process (shaper (s.preEmph.process (v)));   // highs saturate first, like tape
                else
                    v = shaper (v);
                x[i] = v;
            }
        }
        else
        {
            const bool filterIn = mode != Sampler26;   // the 26k mode has no anti-alias filter: that's the grit
            for (int i = 0; i < upN; ++i)
            {
                float v = x[i];
                if (filterIn) v = s.aaIn[1].process (s.aaIn[0].process (v));
                s.holdPhase += holdInc;
                if (s.holdPhase >= 1.0f)
                {
                    s.holdPhase -= 1.0f;
                    s.held = shapeSampler (mode, v + biasOffset, bits);   // bias = DC offset into the converter
                }
                v = s.held;
                if (filterIn) v = s.aaOut[1].process (s.aaOut[0].process (v));
                x[i] = v;
            }
        }
    }
    os->processSamplesDown (block);

    // ---- base-rate post processing, auto gain, dust, mix ----------------------
    const float msCoef = std::exp (-1.0f / (0.3f * (float) sr));
    const float agCoef = std::exp (-1.0f / (0.05f * (float) sr));
    const float clickDecay = std::exp (-1.0f / (0.00035f * (float) sr));
    const float clickProb = (dust * dust * 38.0f + dust * 1.5f) / (float) sr;
    float outPeak = 0;

    for (int i = 0; i < n; ++i)
    {
        float dryIn[2] = { 0, 0 }, wet[2] = { 0, 0 };
        float inSq = 0, wetSq = 0;
        for (int c = 0; c < numCh; ++c)
        {
            auto& s = ch[c];
            float v = buffer.getSample (c, i);
            v = s.dc.process (v);
            if (mode == Tape) v = s.headBump.process (v);
            if (useLow)  v = s.lowCut.process (v);
            if (useHigh) v = s.highCut.process (v);
            if (std::abs (tone) > 0.01f) v = s.tiltHi.process (s.tiltLo.process (v));
            wet[c] = v;

            // dry, delayed to line up with the oversampled path
            const float d = dryCopy.getSample (c, i);
            if (latency > 0)
            {
                s.dryDelay[(size_t) s.dryPos] = d;
                s.dryPos = (s.dryPos + 1) % (int) s.dryDelay.size();
                dryIn[c] = s.dryDelay[(size_t) s.dryPos];
            }
            else dryIn[c] = d;

            inSq += dryIn[c] * dryIn[c];
            wetSq += v * v;
        }

        // auto gain: match the wet RMS to the dry RMS (slow, linked)
        inMs = inSq + msCoef * (inMs - inSq);
        wetMs = wetSq + msCoef * (wetMs - wetSq);
        float target = 1.0f;
        if (autoGain && inMs > 1.0e-9f)
            target = juce::jlimit (0.0631f, 4.0f, std::sqrt ((inMs + 1.0e-12f) / (wetMs + 1.0e-12f)));
        autoG = target + agCoef * (autoG - target);

        const float m = mix.getNextValue();
        const float og = outGain.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            auto& s = ch[c];
            float w = wet[c] * autoG;

            if (dust > 0.0f)
            {
                // vinyl: hiss + crackle + the odd pop
                const float white = rng.nextFloat() * 2.0f - 1.0f;
                s.hissLp += 0.35f * (white - s.hissLp);
                w += s.hissLp * 0.0035f * dust;
                if (rng.nextFloat() < clickProb)
                {
                    const float r = rng.nextFloat();
                    s.clickAmp = (rng.nextBool() ? 1.0f : -1.0f) * (0.02f + 0.3f * r * r * r) * (0.4f + 0.6f * dust);
                    s.clickEnv = 1.0f;
                }
                if (s.clickEnv > 1.0e-4f)
                {
                    w += s.clickHp.process (s.clickAmp * s.clickEnv * (0.6f + 0.4f * white));
                    s.clickEnv *= clickDecay;
                }
            }

            const float out = (dryIn[c] + m * (w - dryIn[c])) * og;
            buffer.setSample (c, i, out);
            outPeak = std::max (outPeak, std::abs (out));
        }
    }

    era::atomicMax (meterOut, outPeak);
    for (int i = 0; i < n; ++i)
        monoScratch[(size_t) i] = numCh > 1 ? 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)) : buffer.getSample (0, i);
    outFifo.push (monoScratch.data(), n);
}

juce::AudioProcessorEditor* EraSProcessor::createEditor() { return new EraSEditor (*this); }
} // namespace eras

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new eras::EraSProcessor(); }
