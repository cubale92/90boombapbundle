#include "EraCProcessor.h"
#include "EraCEditor.h"

namespace erac
{
EraCProcessor::EraCProcessor() : EraProcessorBase ("Era-C", createLayout())
{
    pStyle = raw ("style");   pThr = raw ("threshold"); pRatio = raw ("ratio");
    pAtt = raw ("attack");    pRel = raw ("release");   pAutoRel = raw ("autorel");
    pKnee = raw ("knee");     pMakeup = raw ("makeup"); pAutoMk = raw ("automakeup");
    pMix = raw ("mix");       pScHpf = raw ("schpf");
    initPresets (factoryPresets());
}

juce::AudioProcessorValueTreeState::ParameterLayout EraCProcessor::createLayout()
{
    using namespace era;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (makeChoice ("style", "Style", { "Clean", "Punch", "Opto", "Slam" }, 0));
    l.add (makeFloat ("threshold", "Threshold", { -60.0f, 0.0f, 0.01f }, -18.0f, "dB", formatDb));
    {
        juce::NormalisableRange<float> rr (1.0f, 20.0f, 0.01f);
        rr.setSkewForCentre (4.0f);
        l.add (makeFloat ("ratio", "Ratio", rr, 4.0f, "", [] (float v) { return juce::String (v, v < 10 ? 1 : 0) + ":1"; }));
    }
    l.add (makeFloat ("attack", "Attack", logRange (0.05f, 250.0f), 10.0f, "ms", formatMs));
    l.add (makeFloat ("release", "Release", logRange (5.0f, 2500.0f), 150.0f, "ms", formatMs));
    l.add (makeBool ("autorel", "Auto Release", false));
    l.add (makeFloat ("knee", "Knee", { 0.0f, 24.0f, 0.01f }, 6.0f, "dB", [] (float v) { return juce::String (v, 1) + " dB"; }));
    l.add (makeFloat ("makeup", "Makeup", { -12.0f, 24.0f, 0.01f }, 0.0f, "dB", formatDb));
    l.add (makeBool ("automakeup", "Auto Makeup", false));
    l.add (makeFloat ("mix", "Mix", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", [] (float v) { return juce::String (v, 0) + " %"; }));
    l.add (makeFloat ("schpf", "Sidechain HPF", logRange (20.0f, 500.0f), 20.0f, "Hz",
                      [] (float v) { return v <= 20.5f ? juce::String ("Off") : formatHz (v); }));
    return l;
}

float EraCProcessor::getAutoMakeup() const
{
    if (pAutoMk->load() < 0.5f) return 0.0f;
    // Half of the reduction a full-scale signal would get: keeps perceived level close
    const float gr0 = computeGr (0.0f, pThr->load(), pRatio->load(), effectiveKnee ((int) pStyle->load(), pKnee->load()));
    return juce::jmin (24.0f, gr0 * 0.5f);
}

void EraCProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    thr.reset (sr, 0.05);    thr.setCurrentAndTargetValue (pThr->load());
    makeup.reset (sr, 0.05); makeup.setCurrentAndTargetValue (pMakeup->load() + getAutoMakeup());
    mix.reset (sr, 0.05);    mix.setCurrentAndTargetValue (pMix->load() * 0.01f);
    for (auto& ch : scHp) for (auto& f : ch) f.reset();
    lastHpf = -1;
    ms = 0; env = 0; slowGr = 0; fbGain = 1;
    for (auto& d : dc) { d.prepare (sr); d.reset(); }
    acc.prepare (sr);
}

void EraCProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const int style = (int) pStyle->load();
    const float ratio = pRatio->load();
    const float knee = effectiveKnee (style, pKnee->load());
    const bool autoRel = pAutoRel->load() > 0.5f || style == Opto;
    const float hpf = pScHpf->load();
    const bool useHpf = hpf > 20.5f;

    if (useHpf && std::abs (hpf - lastHpf) > 0.01f)
    {
        for (auto& ch : scHp) { ch[0].highPass (hpf, 0.54119610, sr); ch[1].highPass (hpf, 1.30656296, sr); }
        lastHpf = hpf;
    }

    // Style-dependent ballistics
    float attMs = pAtt->load(), relMs = pRel->load();
    float rmsMs = 0.0f;
    switch (style)
    {
        case Punch: rmsMs = 4.0f; break;
        case Opto:  rmsMs = 20.0f; attMs = juce::jmax (attMs, 8.0f) * 1.5f; break;
        case Slam:  attMs *= 0.5f; break;
        default: break;
    }
    const float attCoef = std::exp (-1.0f / (juce::jmax (0.02f, attMs) * 0.001f * (float) sr));
    const float msCoef = rmsMs > 0 ? std::exp (-1.0f / (rmsMs * 0.001f * (float) sr)) : 0.0f;
    const float slowCoef = std::exp (-1.0f / (0.8f * (float) sr));

    thr.setTargetValue (pThr->load());
    makeup.setTargetValue (pMakeup->load() + getAutoMakeup());
    mix.setTargetValue (pMix->load() * 0.01f);

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float inPeak = 0, outPeak = 0, grMax = 0, lastLevel = -100;

    for (int i = 0; i < n; ++i)
    {
        const float xl = L[i], xr = R ? R[i] : xl;

        // --- side chain ---------------------------------------------------
        float sl = xl, sr2 = xr;
        if (useHpf)
        {
            sl = scHp[0][1].process (scHp[0][0].process (sl));
            sr2 = scHp[1][1].process (scHp[1][0].process (sr2));
        }
        if (style == Punch) { sl *= fbGain; sr2 *= fbGain; }   // feedback topology, bus-comp style

        float det = std::max (std::abs (sl), std::abs (sr2));
        if (rmsMs > 0)
        {
            ms = det * det + msCoef * (ms - det * det);
            det = std::sqrt (2.0f * ms);   // RMS aligned to peak for sines
        }
        const float levelDb = era::gainToDb (det, -100.0f);
        lastLevel = levelDb;

        float target = computeGr (levelDb, thr.getNextValue(), ratio, knee);
        if (style == Punch)
            // Feedback detector sees the compressed signal: gr = (R-1)*(out-T) gives the
            // same static curve as feed-forward, but with that softer, "breathing" bus feel.
            target = juce::jmin (40.0f, target * ratio);

        // --- envelope -----------------------------------------------------
        slowGr = target + slowCoef * (slowGr - target);
        float rel = relMs;
        if (autoRel)
            rel = relMs * (0.3f + 1.7f * juce::jlimit (0.0f, 1.0f, slowGr / 8.0f)); // transients release fast, sustain slow
        const float relCoef = std::exp (-1.0f / (rel * 0.001f * (float) sr));
        env = target > env ? target + attCoef * (env - target) : target + relCoef * (env - target);

        const float grGain = era::dbToGain (-env);
        fbGain = grGain;
        const float g = grGain * era::dbToGain (makeup.getNextValue());
        const float m = mix.getNextValue();

        float wl = xl * g, wr = xr * g;

        // --- colour -------------------------------------------------------
        if (style == Slam)
        {
            // FET-style: grit grows with the amount of gain reduction
            const float amt = 0.08f + 0.5f * juce::jlimit (0.0f, 1.0f, env / 14.0f);
            auto sat = [amt] (float x) { const float d = 1.0f + 3.0f * amt; return x + amt * (std::tanh (d * (x + 0.08f)) / d - std::tanh (d * 0.08f) / d - x); };
            wl = dc[0].process (sat (wl));
            wr = dc[1].process (sat (wr));
        }
        else if (style == Opto)
        {
            // gentle tube make-up stage: a touch of 2nd harmonic
            wl = dc[0].process (wl + 0.06f * wl * wl);
            wr = dc[1].process (wr + 0.06f * wr * wr);
        }

        const float ol = xl + m * (wl - xl);
        const float orr = xr + m * (wr - xr);
        L[i] = ol;
        if (R) R[i] = orr;

        const float ia = std::max (std::abs (xl), std::abs (xr));
        const float oa = std::max (std::abs (ol), std::abs (orr));
        inPeak = std::max (inPeak, ia);
        outPeak = std::max (outPeak, oa);
        grMax = std::max (grMax, env);
        acc.add (ia, oa, env, history);
    }

    era::atomicMax (meterIn, inPeak);
    era::atomicMax (meterOut, outPeak);
    era::atomicMax (meterGr, grMax);
    detectorDb = lastLevel;
}

juce::AudioProcessorEditor* EraCProcessor::createEditor() { return new EraCEditor (*this); }
} // namespace erac

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new erac::EraCProcessor(); }
