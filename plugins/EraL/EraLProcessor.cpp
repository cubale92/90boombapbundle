#include "EraLProcessor.h"
#include "EraLEditor.h"

namespace eral
{
EraLProcessor::EraLProcessor() : EraProcessorBase ("Era-L", createLayout())
{
    pGain = raw ("gain"); pCeiling = raw ("ceiling"); pRelease = raw ("release");
    pStyle = raw ("style"); pLink = raw ("link"); pUnity = raw ("unity");
    initPresets (factoryPresets());
}

juce::AudioProcessorValueTreeState::ParameterLayout EraLProcessor::createLayout()
{
    using namespace era;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (makeFloat ("gain", "Gain", { 0.0f, 24.0f, 0.01f }, 0.0f, "dB", [] (float v) { return "+" + juce::String (v, 1) + " dB"; }));
    l.add (makeFloat ("ceiling", "Ceiling", { -12.0f, 0.0f, 0.01f }, -1.0f, "dB", [] (float v) { return juce::String (v, 1) + " dB"; }));
    l.add (makeFloat ("release", "Release", logRange (1.0f, 1000.0f), 100.0f, "ms", formatMs));
    l.add (makeChoice ("style", "Style", { "Clean", "Punchy", "Warm", "Loud '95" }, 0));
    l.add (makeFloat ("link", "Stereo Link", { 0.0f, 100.0f, 0.1f }, 100.0f, "%", [] (float v) { return juce::String (v, 0) + " %"; }));
    l.add (makeBool ("unity", "Unity Monitor", false));
    return l;
}

static void kWeighting (era::Biquad& shelf, era::Biquad& hp, double fs)
{
    // ITU-R BS.1770 pre-filter, recomputed for any sample rate
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.setCoefs (Vh + Vb * K / Q + K * K, 2.0 * (K * K - Vh), Vh - Vb * K / Q + K * K, a0, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        hp.setCoefs (a0 * 1.0, a0 * -2.0, a0 * 1.0, a0, 2.0 * (K * K - 1.0), 1.0 - K / Q + K * K);
    }
}

void EraLProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    la = juce::jmax (1, (int) std::round (lookaheadMs * 0.001 * sr));
    setLatencySamples (la);
    for (int c = 0; c < 2; ++c)
    {
        smin[c].prepare (la + 1);
        box[c].prepare (la);
        env[c] = 1.0f;
        delay[c].assign ((size_t) la, 0.0f);
        kWeighting (kShelf[c], kHp[c], sr);
        kShelf[c].reset(); kHp[c].reset();
    }
    dpos = 0;
    slowGr = 0;
    gain.reset (sr, 0.05);    gain.setCurrentAndTargetValue (era::dbToGain (pGain->load()));
    ceiling.reset (sr, 0.05); ceiling.setCurrentAndTargetValue (era::dbToGain (pCeiling->load()));
    blockLen = juce::jmax (1, (int) (sr * 0.1));
    blockSum = 0; blockCount = 0; blockIdx = 0; blocksFilled = 0;
    blocks.fill (0.0);
    acc.prepare (sr);
}

void EraLProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0) return;

    const int style = (int) pStyle->load();
    const float link = pLink->load() * 0.01f;
    const bool unity = pUnity->load() > 0.5f;
    float relMs = pRelease->load();
    if (style == Punchy) relMs *= 0.5f;
    if (style == Loud95) relMs *= 0.7f;
    const float slowCoef = std::exp (-1.0f / (0.5f * (float) sr));

    gain.setTargetValue (era::dbToGain (pGain->load()));
    ceiling.setTargetValue (era::dbToGain (pCeiling->load()));

    float* d[2] = { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float inPeak = 0, outPeak = 0, grMax = 0;

    for (int i = 0; i < n; ++i)
    {
        const float g = gain.getNextValue();
        const float c = ceiling.getNextValue();
        float x[2], req[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            float v = (ch < numCh ? d[ch][i] : d[0][i]) * g;
            inPeak = std::max (inPeak, std::abs (v / g));

            // Pre-limiter colour stages
            if (style == Warm)
            {
                // soft knee clipper about +1 dB above the ceiling: rounds the peaks like tape
                const float lvl = c * 1.12f, k = 0.55f * lvl, a = std::abs (v);
                if (a > k) v = std::copysign (k + (lvl - k) * std::tanh ((a - k) / (lvl - k)), v);
            }
            else if (style == Loud95)
            {
                // hard converter-style clip +2 dB over the ceiling: the brickwall crunch of mid-90s singles
                const float lvl = c * 1.26f;
                v = juce::jlimit (-lvl, lvl, v);
            }
            x[ch] = v;
            const float a = std::abs (v);
            req[ch] = a > c ? c / a : 1.0f;
        }

        const float linked = std::min (req[0], req[1]);
        float gr = 1.0f, a = 0, b = 0;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const float r = linked * link + req[ch] * (1.0f - link);
            const float m = smin[ch].push (r);

            // program-dependent release: longer when the limiter has been working hard for a while
            float rel = relMs;
            if (style == Clean) rel *= 0.6f + 0.8f * juce::jlimit (0.0f, 1.0f, slowGr / 6.0f);
            const float relCoef = std::exp (-1.0f / (rel * 0.001f * (float) sr));
            env[ch] = m < env[ch] ? m : m + relCoef * (env[ch] - m);
            const float gg = box[ch].push (env[ch]);

            // delayed signal * gain, then a final safety clip
            const float delayed = delay[ch][(size_t) dpos];
            delay[ch][(size_t) dpos] = x[ch];
            float out = juce::jlimit (-c, c, delayed * gg);
            if (unity) out /= g;
            d[ch][i] = out;
            gr = std::min (gr, gg);
            outPeak = std::max (outPeak, std::abs (out));
            if (ch == 0) a = out; else b = out;
        }
        if (numCh == 1) b = a;
        dpos = (dpos + 1) % la;

        const float grDb = -era::gainToDb (gr, -60.0f);
        slowGr = grDb + slowCoef * (slowGr - grDb);
        grMax = std::max (grMax, grDb);
        acc.add (std::max (std::abs (x[0]), std::abs (x[1])), std::max (std::abs (a), std::abs (b)), grDb, history);

        // loudness of what leaves the plug-in
        const float ka = kHp[0].process (kShelf[0].process (a));
        const float kb = kHp[1].process (kShelf[1].process (b));
        blockSum += (double) ka * ka + (double) kb * kb;
        if (++blockCount >= blockLen)
        {
            blocks[(size_t) blockIdx] = blockSum / blockCount;
            blockIdx = (blockIdx + 1) % (int) blocks.size();
            blocksFilled = std::min (blocksFilled + 1, (int) blocks.size());
            blockSum = 0; blockCount = 0;

            auto meanOf = [&] (int count)
            {
                count = std::min (count, blocksFilled);
                double s = 0;
                for (int k = 1; k <= count; ++k) s += blocks[(size_t) ((blockIdx - k + (int) blocks.size()) % (int) blocks.size())];
                return count > 0 ? s / count : 0.0;
            };
            auto toLufs = [] (double ms) { return ms > 1.0e-12 ? (float) (-0.691 + 10.0 * std::log10 (ms)) : -100.0f; };
            lufsMomentary = toLufs (meanOf (4));
            lufsShort = toLufs (meanOf (30));
        }
    }

    era::atomicMax (meterIn, inPeak);
    era::atomicMax (meterOut, outPeak);
    era::atomicMax (meterGr, grMax);
}

juce::AudioProcessorEditor* EraLProcessor::createEditor() { return new EraLEditor (*this); }
} // namespace eral

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new eral::EraLProcessor(); }
