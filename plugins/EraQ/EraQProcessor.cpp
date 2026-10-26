#include "EraQProcessor.h"
#include "EraQEditor.h"

namespace eraq
{
int makeBandStages (const BandSetting& b, double sr, era::SvfCoeffs* out)
{
    using era::SvfType;
    using era::SvfCoeffs;
    switch (b.type)
    {
        case Bell:      out[0] = SvfCoeffs::make (SvfType::Bell, b.freq, b.q, b.gain, sr); return 1;
        case LowShelf:  out[0] = SvfCoeffs::make (SvfType::LowShelf, b.freq, b.q, b.gain, sr); return 1;
        case HighShelf: out[0] = SvfCoeffs::make (SvfType::HighShelf, b.freq, b.q, b.gain, sr); return 1;
        case Notch:     out[0] = SvfCoeffs::make (SvfType::Notch, b.freq, b.q, 0, sr); return 1;
        case LowCut:
        case HighCut:
        {
            static const double q2[] = { 0.54119610, 1.30656296 };
            static const double q4[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
            const auto t = b.type == LowCut ? SvfType::HighPass : SvfType::LowPass;
            const double res = b.q / 0.70710678; // Q = 0.707 -> pure Butterworth
            const int n = b.slope == 0 ? 1 : (b.slope == 1 ? 2 : 4);
            for (int i = 0; i < n; ++i)
            {
                double q = n == 1 ? b.q : (n == 2 ? q2[i] : q4[i]);
                if (n > 1 && i == n - 1) q *= res;
                out[i] = SvfCoeffs::make (t, b.freq, q, 0, sr);
            }
            return n;
        }
        default: break;
    }
    return 0;
}

//==============================================================================
EraQProcessor::EraQProcessor() : EraProcessorBase ("Era-Q", createLayout())
{
    for (int i = 0; i < numBands; ++i)
        bp[(size_t) i] = { raw (pid (i, "on")), raw (pid (i, "type")), raw (pid (i, "freq")), raw (pid (i, "gain")),
                           raw (pid (i, "q")), raw (pid (i, "slope")), raw (pid (i, "place")) };
    outputParam = raw ("output");
    colorParam = raw ("color");
    initPresets (factoryPresets());
}

juce::AudioProcessorValueTreeState::ParameterLayout EraQProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const float defFreq[] = { 30, 90, 250, 700, 2000, 5000, 10000, 18000 };
    const int defType[] = { LowCut, LowShelf, Bell, Bell, Bell, Bell, HighShelf, HighCut };

    for (int i = 0; i < numBands; ++i)
    {
        const juce::String n = "Band " + juce::String (i + 1) + " ";
        layout.add (era::makeBool (pid (i, "on"), n + "On", false));
        layout.add (era::makeChoice (pid (i, "type"), n + "Type",
                                     { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" }, defType[i]));
        layout.add (era::makeFloat (pid (i, "freq"), n + "Freq", era::logRange (10.0f, 30000.0f), defFreq[i], "Hz", era::formatHz));
        layout.add (era::makeFloat (pid (i, "gain"), n + "Gain", { -30.0f, 30.0f, 0.01f }, 0.0f, "dB", era::formatDb));
        layout.add (era::makeFloat (pid (i, "q"), n + "Q", era::logRange (0.1f, 30.0f), 0.707f, "",
                                    [] (float v) { return juce::String (v, v < 10 ? 2 : 1); }));
        layout.add (era::makeChoice (pid (i, "slope"), n + "Slope", { "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 1));
        layout.add (era::makeChoice (pid (i, "place"), n + "Placement", { "Stereo", "Mid", "Side" }, 0));
    }
    layout.add (era::makeFloat ("output", "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB", era::formatDb));
    layout.add (era::makeFloat ("color", "Color", { 0.0f, 100.0f, 0.1f }, 0.0f, "%",
                                [] (float v) { return v < 0.05f ? juce::String ("Clean") : juce::String (v, 0) + " %"; }));
    layout.add (era::makeChoice ("analyzer", "Analyzer", { "Off", "Pre", "Post", "Pre+Post" }, 3));
    return layout;
}

BandSetting EraQProcessor::getBandSetting (int i) const
{
    const auto& p = bp[(size_t) i];
    BandSetting b;
    b.on = p.on->load() > 0.5f;
    b.type = (int) p.type->load();
    b.freq = p.freq->load();
    b.gain = p.gain->load();
    b.q = p.q->load();
    b.slope = (int) p.slope->load();
    b.place = (int) p.place->load();
    return b;
}

void EraQProcessor::prepareToPlay (double sr, int maxBlock)
{
    currentSr = sr;
    for (int i = 0; i < numBands; ++i)
    {
        auto& b = bands[(size_t) i];
        const auto s = getBandSetting (i);
        b.freq.reset (sr, 0.03);  b.freq.setCurrentAndTargetValue (juce::jmin (s.freq, (float) sr * 0.49f));
        b.q.reset (sr, 0.03);     b.q.setCurrentAndTargetValue (s.q);
        b.gain.reset (sr, 0.03);  b.gain.setCurrentAndTargetValue (s.gain);
        b.mix.reset (sr, 0.02);   b.mix.setCurrentAndTargetValue (s.on ? 1.0f : 0.0f);
        b.type = b.slope = b.place = -1;
        b.resetState();
    }
    outGain.reset (sr, 0.03); outGain.setCurrentAndTargetValue (era::dbToGain (outputParam->load()));
    color.reset (sr, 0.05);   color.setCurrentAndTargetValue (colorParam->load() * 0.01f);
    for (auto& d : dc) { d.prepare (sr); d.reset(); }
    monoScratch.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
}

void EraQProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (numCh == 0 || n == 0) return;
    if ((int) monoScratch.size() < n) monoScratch.resize ((size_t) n); // host exceeded announced block size

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    // Pre analyzer + input meter
    float inPeak = 0;
    for (int i = 0; i < n; ++i)
    {
        const float m = R ? 0.5f * (L[i] + R[i]) : L[i];
        monoScratch[(size_t) i] = m;
        inPeak = std::max (inPeak, std::max (std::abs (L[i]), R ? std::abs (R[i]) : 0.0f));
    }
    preFifo.push (monoScratch.data(), n);
    era::atomicMax (meterIn, inPeak);

    // Update targets
    for (int i = 0; i < numBands; ++i)
    {
        auto& b = bands[(size_t) i];
        const auto s = getBandSetting (i);
        b.freq.setTargetValue (juce::jlimit (10.0f, (float) currentSr * 0.49f, s.freq));
        b.q.setTargetValue (s.q);
        b.gain.setTargetValue (s.gain);
        b.mix.setTargetValue (s.on ? 1.0f : 0.0f);
        if (s.type != b.type || s.slope != b.slope || s.place != b.place)
        {
            if (s.type != b.type || s.slope != b.slope) b.resetState();
            b.type = s.type; b.slope = s.slope; b.place = s.place;
            BandSetting cur = s;
            cur.freq = b.freq.getCurrentValue(); cur.q = b.q.getCurrentValue(); cur.gain = b.gain.getCurrentValue();
            b.numStages = makeBandStages (cur, currentSr, b.stages);
        }
    }
    outGain.setTargetValue (era::dbToGain (outputParam->load()));
    color.setTargetValue (colorParam->load() * 0.01f);

    constexpr int sub = 32;
    for (int start = 0; start < n; start += sub)
    {
        const int len = juce::jmin (sub, n - start);
        float* l = L + start;
        float* r = R ? R + start : nullptr;

        for (auto& b : bands)
        {
            if (b.mix.getCurrentValue() <= 0.0f && b.mix.getTargetValue() <= 0.0f)
            {
                b.freq.setCurrentAndTargetValue (b.freq.getTargetValue());
                b.q.setCurrentAndTargetValue (b.q.getTargetValue());
                b.gain.setCurrentAndTargetValue (b.gain.getTargetValue());
                continue;
            }

            if (b.freq.isSmoothing() || b.q.isSmoothing() || b.gain.isSmoothing())
            {
                b.freq.skip (len); b.q.skip (len); b.gain.skip (len);
                BandSetting cur;
                cur.type = b.type; cur.slope = b.slope;
                cur.freq = b.freq.getCurrentValue(); cur.q = b.q.getCurrentValue(); cur.gain = b.gain.getCurrentValue();
                b.numStages = makeBandStages (cur, currentSr, b.stages);
            }

            const int ns = b.numStages;
            auto filt = [&] (float x, int ch)
            {
                for (int s = 0; s < ns; ++s) x = b.st[ch][s].process (x, b.stages[s]);
                return x;
            };

            for (int i = 0; i < len; ++i)
            {
                const float mix = b.mix.getNextValue();
                if (r == nullptr || b.place == Stereo)
                {
                    l[i] += mix * (filt (l[i], 0) - l[i]);
                    if (r) r[i] += mix * (filt (r[i], 1) - r[i]);
                }
                else
                {
                    float m = 0.5f * (l[i] + r[i]);
                    float s = 0.5f * (l[i] - r[i]);
                    if (b.place == Mid) m += mix * (filt (m, 0) - m);
                    else                s += mix * (filt (s, 1) - s);
                    l[i] = m + s;
                    r[i] = m - s;
                }
            }
            if (b.mix.getCurrentValue() <= 0.0f)
                b.resetState();
        }

        // Analog "color" stage: unity-slope asymmetric saturation (adds 2nd + 3rd harmonics)
        for (int i = 0; i < len; ++i)
        {
            const float c = color.getNextValue();
            const float g = outGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = ch == 0 ? l : r;
                float x = d[i];
                if (c > 0.0005f)
                {
                    const float drive = 1.0f + 2.5f * c;
                    const float bias = 0.12f * c;
                    const float tb = std::tanh (drive * bias);
                    const float slope0 = drive * (1.0f - tb * tb);
                    const float y = (std::tanh (drive * (x + bias)) - tb) / slope0;
                    x = dc[ch].process (x + c * (y - x));
                }
                d[i] = x * g;
            }
        }
    }

    float outPeak = 0;
    for (int i = 0; i < n; ++i)
    {
        monoScratch[(size_t) i] = R ? 0.5f * (L[i] + R[i]) : L[i];
        outPeak = std::max (outPeak, std::max (std::abs (L[i]), R ? std::abs (R[i]) : 0.0f));
    }
    postFifo.push (monoScratch.data(), n);
    era::atomicMax (meterOut, outPeak);
}

juce::AudioProcessorEditor* EraQProcessor::createEditor() { return new EraQEditor (*this); }
} // namespace eraq

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new eraq::EraQProcessor(); }
