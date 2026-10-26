#include "EraSEditor.h"

namespace eras
{
using namespace era;

void SpectrumView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, 6.0f);
    auto plot = r.reduced (1.0f);
    const float minHz = 20.0f, maxHz = 22000.0f;
    drawFrequencyGrid (g, plot, minHz, maxHz);

    auto area = plot.withTrimmedBottom (16.0f).withTrimmedTop (8.0f);
    for (float db = -72.0f; db <= 0.0f; db += 12.0f)
    {
        const float y = juce::jmap (db, -84.0f, 6.0f, area.getBottom(), area.getY());
        g.setColour (Colours::grid.withAlpha (0.5f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    auto fill = an.createPath (area, -84.0f, 6.0f, minHz, maxHz, true);
    juce::ColourGradient grad (Colours::magenta.withAlpha (0.45f), 0, area.getY(), Colours::orange.withAlpha (0.12f), 0, area.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (fill);
    g.setColour (Colours::magenta);
    g.strokePath (an.createPath (area, -84.0f, 6.0f, minHz, maxHz, false), juce::PathStrokeType (1.4f));

    // sampler rate marker (Nyquist of the emulated converter)
    const int m = (int) proc.apvts.getRawParameterValue ("mode")->load();
    if (isSampler (m))
    {
        const float rate = m == Sampler26 ? 26040.0f : (m == Sampler40 ? 40000.0f : proc.apvts.getRawParameterValue ("rate")->load());
        const float ny = rate * 0.5f;
        if (ny < maxHz)
        {
            const float x = freqToX (ny, minHz, maxHz, plot);
            const float dashes[] = { 5.0f, 4.0f };
            g.setColour (Colours::gold.withAlpha (0.85f));
            g.drawDashedLine ({ x, plot.getY(), x, plot.getBottom() }, dashes, 2, 1.2f);
            g.setFont (font (10.5f, true));
            g.drawText ("NYQUIST " + formatHz (ny), juce::Rectangle<float> (x - 130, plot.getY() + 6, 124, 14), juce::Justification::centredRight);
        }
    }

    g.setColour (Colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("OUTPUT SPECTRUM", plot.reduced (10, 6).removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

void TransferView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, 6.0f);
    auto plot = r.reduced (12.0f);
    auto X = [&] (float v) { return juce::jmap (v, -1.0f, 1.0f, plot.getX(), plot.getRight()); };
    auto Y = [&] (float v) { return juce::jmap (juce::jlimit (-1.25f, 1.25f, v), -1.25f, 1.25f, plot.getBottom(), plot.getY()); };

    g.setColour (Colours::grid);
    g.drawHorizontalLine (juce::roundToInt (Y (0)), plot.getX(), plot.getRight());
    g.drawVerticalLine (juce::roundToInt (X (0)), plot.getY(), plot.getBottom());
    g.setColour (Colours::grid.withAlpha (0.5f));
    g.drawLine (X (-1), Y (-1), X (1), Y (1));

    const int mode = (int) proc.apvts.getRawParameterValue ("mode")->load();
    const float drive = dbToGain (proc.apvts.getRawParameterValue ("drive")->load());
    const float bias = proc.apvts.getRawParameterValue ("bias")->load() * 0.01f;
    const int bits = (int) proc.apvts.getRawParameterValue ("bits")->load();

    juce::Path p;
    const int steps = 400;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) steps;
        const float in = x * drive;
        const float y = isSampler (mode) ? shapeSampler (mode, in + bias * 0.1f, bits) : shapeAnalog (mode, in, bias);
        // normalise the display so the curve's shape (not its gain) is what you see
        const float yn = y / juce::jmax (1.0f, drive * 0.85f);
        if (i == 0) p.startNewSubPath (X (x), Y (yn)); else p.lineTo (X (x), Y (yn));
    }
    g.setColour (Colours::magenta.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (5.0f));
    g.setColour (Colours::magenta);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    const float lvl = juce::jlimit (0.0f, 1.0f, proc.driveDisplay.load() / drive);
    if (lvl > 0.001f)
    {
        const float in = lvl * drive;
        const float y = isSampler (mode) ? shapeSampler (mode, in + bias * 0.1f, bits) : shapeAnalog (mode, in, bias);
        g.setColour (Colours::text);
        g.fillEllipse (X (lvl) - 4.5f, Y (y / juce::jmax (1.0f, drive * 0.85f)) - 4.5f, 9.0f, 9.0f);
    }

    g.setColour (Colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("TRANSFER", r.reduced (10, 8).removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

EraSEditor::EraSEditor (EraSProcessor& p)
    : EraEditorBase (p, 920, 550, Colours::magenta, "Era-S", "Golden Era Saturation"),
      proc (p), spectrum (p), transfer (p),
      mode (p.apvts, "mode", { "Tape", "Tube", "Console", "SP 26k", "Sixty 40k", "Vari-Rate" }, 3),
      drive (p.apvts, "drive", "Drive", false, Colours::orange), bias (p.apvts, "bias", "Bias", true),
      tone (p.apvts, "tone", "Tone", true), lowCut (p.apvts, "lowcut", "Low Cut"), highCut (p.apvts, "highcut", "High Cut"),
      bits (p.apvts, "bits", "Bits", false, Colours::gold), rate (p.apvts, "rate", "Rate", false, Colours::gold),
      dust (p.apvts, "dust", "Dust", false, Colours::teal), mix (p.apvts, "mix", "Mix"), output (p.apvts, "output", "Output", true),
      autoGain (p.apvts, "autogain", "AUTO GAIN")
{
    for (juce::Component* c : std::initializer_list<juce::Component*> { &spectrum, &transfer, &mode, &inMeter, &outMeter, &panel })
        body().addAndMakeVisible (c);
    spectrum.setBounds (12, 56, 588, 300);
    transfer.setBounds (608, 56, 236, 236);
    mode.setBounds (608, 300, 236, 56);
    inMeter.setBounds (854, 56, 24, 300);
    outMeter.setBounds (884, 56, 24, 300);

    panel.setBounds (12, 366, 896, 152);
    int x = 8;
    for (auto* k : { &drive, &bias, &tone, &lowCut, &highCut, &bits, &rate, &dust, &mix, &output })
    {
        panel.addAndMakeVisible (k);
        k->setBounds (x, 12, 80, 130);
        x += 80;
    }
    panel.addAndMakeVisible (autoGain);
    autoGain.setBounds (812, 60, 76, 30);
    autoGain.setTooltip ("Keeps the output level matched to the input while you drive harder");
    bits.slider.setTooltip ("Converter resolution for the sampler modes");
    rate.slider.setTooltip ("Sample rate for Vari-Rate mode (the SP 26k / Sixty 40k modes have fixed rates)");

    startTimerHz (30);
}

void EraSEditor::timerCallback()
{
    spectrum.update();
    transfer.repaint();
    const int m = mode.getIndex();
    bits.setEnabled (isSampler (m));
    rate.setEnabled (m == VariRate);
    bias.setEnabled (true);
    inMeter.setLevel (proc.meterIn.exchange (0.0f));
    outMeter.setLevel (proc.meterOut.exchange (0.0f));
}
} // namespace eras
