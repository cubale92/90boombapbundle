// Offline test runner: renders a 90s-style test loop through every factory preset,
// checks for NaN/Inf/blow-ups, measures Init-preset transparency and optionally
// saves editor screenshots.  Usage:
//   TestEraX                       -> preset table + checks
//   TestEraX --shot out.png [n]    -> screenshot with preset n loaded and audio running
#include <juce_audio_utils/juce_audio_utils.h>
#include "EraProcessorBase.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static constexpr double sr = 48000.0;
static constexpr int blockSize = 512;

/** 92 BPM boom bap loop: kick, snare, hats, sub bass and a "sample" chord. */
static juce::AudioBuffer<float> makeLoop (double seconds)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    juce::Random rng (1234);
    const double beat = 60.0 / 92.0;
    float lpS = 0, hpH = 0, prevH = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double inBeat = std::fmod (t, beat);
        const int beatIdx = (int) (t / beat) % 4;
        float x = 0;
        // kick on 1 and the "and" of 2 + 3
        const double kickT = std::fmod (t, beat * 2.0);
        if (kickT < 0.35) { const double f = 50 + 70 * std::exp (-kickT * 30); x += (float) (0.8 * std::exp (-kickT * 9) * std::sin (juce::MathConstants<double>::twoPi * f * kickT)); }
        // snare on 2 and 4
        if ((beatIdx == 1 || beatIdx == 3) && inBeat < 0.25)
        {
            const float noise = rng.nextFloat() * 2 - 1;
            lpS += 0.35f * (noise - lpS);
            x += (float) (0.55 * std::exp (-inBeat * 18) * (lpS + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 190 * inBeat)));
        }
        // hats on 8ths with swing
        const double eighth = beat / 2.0;
        const double hT = std::fmod (t + (std::fmod (t, beat) > eighth ? -0.03 : 0.0), eighth);
        if (hT < 0.06) { const float nz = rng.nextFloat() * 2 - 1; hpH = nz - prevH; prevH = nz; x += (float) (0.18 * std::exp (-hT * 70) * hpH); }
        // bass + chord sample
        const float bass = (float) (0.28 * std::sin (juce::MathConstants<double>::twoPi * 55.0 * t));
        const float chord = (float) (0.08 * (std::sin (juce::MathConstants<double>::twoPi * 261.6 * t) + std::sin (juce::MathConstants<double>::twoPi * 329.6 * t) + std::sin (juce::MathConstants<double>::twoPi * 392.0 * t + 0.3)));
        b.setSample (0, i, x + bass + chord * 1.1f);
        b.setSample (1, i, x + bass + chord * 0.9f);
    }
    return b;
}

static void processAll (juce::AudioProcessor& p, juce::AudioBuffer<float>& buf, std::function<void()> between = nullptr)
{
    juce::MidiBuffer midi;
    for (int pos = 0; pos < buf.getNumSamples(); pos += blockSize)
    {
        const int len = juce::jmin (blockSize, buf.getNumSamples() - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
        p.processBlock (view, midi);
        if (between) between();
    }
}

static float peakDb (const juce::AudioBuffer<float>& b, int start = 0)
{
    float m = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        m = juce::jmax (m, b.getMagnitude (c, start, b.getNumSamples() - start));
    return juce::Decibels::gainToDecibels (m, -120.0f);
}

static float rmsDb (const juce::AudioBuffer<float>& b, int start = 0)
{
    double s = 0;
    for (int c = 0; c < b.getNumChannels(); ++c)
        s += std::pow ((double) b.getRMSLevel (c, start, b.getNumSamples() - start), 2.0);
    return juce::Decibels::gainToDecibels ((float) std::sqrt (s / b.getNumChannels()), -120.0f);
}

static bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (c, i))) return false;
    return true;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    auto& era = dynamic_cast<era::EraProcessorBase&> (*proc);
    proc->setPlayConfigDetails (2, 2, sr, blockSize);
    proc->prepareToPlay (sr, blockSize);
    const auto& entries = era.presets->getEntries();

    if (argc >= 3 && juce::String (argv[1]) == "--shot")
    {
        const int idx = argc >= 4 ? juce::String (argv[3]).getIntValue() : 0;
        era.presets->load (idx);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        auto loop = makeLoop (2.5);
        processAll (*proc, loop, [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (8); });
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::cout << "screenshot " << out.getFullPathName() << " preset '" << entries[(size_t) idx].name << "'\n";
        ed.reset();
        return 0;
    }

    int failures = 0;
    std::cout << ERA_PLUGIN_NAME << "  latency " << proc->getLatencySamples() << " samples, "
              << entries.size() << " presets\n";

    // 1) Init preset transparency on sines
    era.presets->load (0);
    for (float hz : { 60.0f, 1000.0f, 8000.0f })
    {
        proc->prepareToPlay (sr, blockSize);
        juce::AudioBuffer<float> b (2, (int) sr);
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, 0.1f * std::sin (juce::MathConstants<float>::twoPi * hz * (float) i / (float) sr)),
            b.setSample (1, i, b.getSample (0, i));
        processAll (*proc, b);
        const float g = rmsDb (b, b.getNumSamples() / 2) - juce::Decibels::gainToDecibels (0.1f / std::sqrt (2.0f));
        std::cout << "  Init gain @" << hz << " Hz: " << juce::String (g, 2) << " dB\n";
        if (std::abs (g) > 0.5f) { std::cout << "    FAIL: init preset not transparent\n"; ++failures; }
    }

    // 2) Every preset on the test loop
    const auto dry = makeLoop (4.0);
    std::cout << "  dry loop: peak " << juce::String (peakDb (dry), 1) << " dBFS, rms " << juce::String (rmsDb (dry), 1) << " dBFS\n";
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        proc->prepareToPlay (sr, blockSize);
        era.presets->load (i);
        auto b = dry;
        processAll (*proc, b);
        const bool ok = allFinite (b);
        const float pk = peakDb (b, (int) sr), rms = rmsDb (b, (int) sr);
        const bool loud = pk > 12.0f || rms < -60.0f;
        std::cout << "  [" << juce::String (i).paddedLeft ('0', 2) << "] " << (entries[(size_t) i].category + " / " + entries[(size_t) i].name).paddedRight (' ', 46)
                  << " peak " << juce::String (pk, 1).paddedLeft (' ', 6) << "  rms " << juce::String (rms, 1).paddedLeft (' ', 6)
                  << (ok && ! loud ? "" : "   <-- CHECK") << "\n";
        if (! ok || loud) ++failures;
    }

    // 3) State round trip
    era.presets->load ((int) entries.size() - 1);
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    era.presets->load (0);
    proc->setStateInformation (state.getData(), (int) state.getSize());
    if (era.presets->getCurrentName() != entries.back().name) { std::cout << "  FAIL: state round trip lost preset name\n"; ++failures; }

    std::cout << (failures == 0 ? "  ALL OK\n" : "  FAILURES: " + std::to_string (failures) + "\n");
    return failures == 0 ? 0 : 1;
}
