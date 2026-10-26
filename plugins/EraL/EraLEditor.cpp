#include "EraLEditor.h"

namespace eral
{
using namespace era;

void ReadoutPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    auto rows = r.reduced (14.0f, 10.0f);
    const float h = rows.getHeight() / 4.0f;
    auto row = [&] (const juce::String& label, const juce::String& value, juce::Colour c)
    {
        auto a = rows.removeFromTop (h);
        g.setColour (Colours::textDim);
        g.setFont (font (10.5f, true));
        g.drawText (label, a.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (font (24.0f, true));
        g.drawText (value, a, juce::Justification::centredLeft);
    };
    auto lufs = [] (float v) { return v <= -99.0f ? juce::String ("--") : juce::String (v, 1) + " LUFS"; };
    row ("MOMENTARY", lufs (momentary), Colours::text);
    row ("SHORT-TERM", lufs (shortTerm), Colours::purple.brighter (0.3f));
    row ("GAIN REDUCTION", gr > 0.05f ? "-" + juce::String (gr, 1) + " dB" : juce::String ("0.0 dB"), Colours::meterRed);
    row ("OUTPUT PEAK (SAMPLE)", peak <= -99.0f ? juce::String ("--") : juce::String (peak, 1) + " dBFS", Colours::text);
}

EraLEditor::EraLEditor (EraLProcessor& p)
    : EraEditorBase (p, 920, 550, Colours::purple, "Era-L", "Golden Era Limiter"),
      proc (p), history (Colours::purple), style (p.apvts, "style", {}, 2),
      gain (p.apvts, "gain", "Gain", false, Colours::orange), ceiling (p.apvts, "ceiling", "Ceiling"),
      release (p.apvts, "release", "Release"), link (p.apvts, "link", "Stereo Link", false, Colours::teal),
      unity (p.apvts, "unity", "UNITY MONITOR")
{
    history.minDb = -30.0f;
    history.maxDb = 3.0f;
    history.grRange = 12.0f;
    history.getLineDb = [this] { return proc.apvts.getRawParameterValue ("ceiling")->load(); };
    history.lineLabel = "CEILING";

    for (juce::Component* c : std::initializer_list<juce::Component*> { &history, &readout, &style, &inMeter, &grMeter, &outMeter, &panel })
        body().addAndMakeVisible (c);
    history.setBounds (12, 56, 588, 300);
    readout.setBounds (608, 56, 236, 236);
    style.setBounds (608, 300, 236, 56);
    inMeter.setBounds (852, 56, 16, 300);
    grMeter.setBounds (872, 56, 16, 300);
    outMeter.setBounds (892, 56, 16, 300);
    grMeter.setRange (0, 12.0f);
    inMeter.setRange (-30.0f, 6.0f);
    outMeter.setRange (-30.0f, 6.0f);

    panel.setBounds (12, 366, 896, 152);
    int x = 30;
    for (auto* k : { &gain, &ceiling, &release, &link })
    {
        panel.addAndMakeVisible (k);
        k->setBounds (x, 10, 140, 134);
        x += 160;
    }
    panel.addAndMakeVisible (unity);
    unity.setBounds (700, 58, 160, 32);
    unity.setTooltip ("Turns the output down by the Gain amount so you can judge the limiting at equal loudness");

    startTimerHz (30);
}

void EraLEditor::timerCallback()
{
    history.update (proc.history);
    const float in = proc.meterIn.exchange (0.0f), out = proc.meterOut.exchange (0.0f), gr = proc.meterGr.exchange (0.0f);
    inMeter.setLevel (in);
    outMeter.setLevel (out);
    grMeter.setLevel (gr);

    // readouts hold their max for ~1 s so they are readable
    const float pk = gainToDb (out, -100.0f);
    if (++holdCount > 30) { grHold = gr; peakHold = pk; holdCount = 0; }
    else { grHold = juce::jmax (grHold, gr); peakHold = juce::jmax (peakHold, pk); }
    readout.momentary = proc.lufsMomentary.load();
    readout.shortTerm = proc.lufsShort.load();
    readout.gr = grHold;
    readout.peak = peakHold;
    readout.repaint();
}
} // namespace eral
