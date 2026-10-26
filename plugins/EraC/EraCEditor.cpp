#include "EraCEditor.h"

namespace erac
{
using namespace era;

void CurveView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, 6.0f);
    auto plot = r.reduced (10.0f);
    const float lo = -60.0f, hi = 0.0f;
    auto X = [&] (float db) { return juce::jmap (db, lo, hi, plot.getX(), plot.getRight()); };
    auto Y = [&] (float db) { return juce::jmap (db, lo, hi, plot.getBottom(), plot.getY()); };

    for (float db = lo; db <= hi; db += 12.0f)
    {
        g.setColour (Colours::grid.withAlpha (0.7f));
        g.drawVerticalLine (juce::roundToInt (X (db)), plot.getY(), plot.getBottom());
        g.drawHorizontalLine (juce::roundToInt (Y (db)), plot.getX(), plot.getRight());
    }
    g.setColour (Colours::gridStrong);
    g.drawLine (X (lo), Y (lo), X (hi), Y (hi), 1.0f);

    const int style = (int) proc.apvts.getRawParameterValue ("style")->load();
    const float thr = proc.apvts.getRawParameterValue ("threshold")->load();
    const float ratio = proc.apvts.getRawParameterValue ("ratio")->load();
    const float knee = effectiveKnee (style, proc.apvts.getRawParameterValue ("knee")->load());

    juce::Path p;
    for (float in = lo; in <= hi; in += 0.5f)
    {
        const float out = in - computeGr (in, thr, ratio, knee);
        if (in == lo) p.startNewSubPath (X (in), Y (out)); else p.lineTo (X (in), Y (out));
    }
    g.setColour (Colours::gold.withAlpha (0.25f));
    g.strokePath (p, juce::PathStrokeType (5.0f));
    g.setColour (Colours::gold);
    g.strokePath (p, juce::PathStrokeType (2.0f));

    // threshold marker
    const float dashes[] = { 4.0f, 4.0f };
    g.setColour (Colours::gold.withAlpha (0.5f));
    g.drawDashedLine ({ X (thr), plot.getY(), X (thr), plot.getBottom() }, dashes, 2, 1.0f);

    // live detector dot
    const float d = proc.detectorDb.load();
    if (d > lo)
    {
        const float o = d - computeGr (d, thr, ratio, knee);
        g.setColour (Colours::text);
        g.fillEllipse (X (d) - 4.5f, Y (o) - 4.5f, 9.0f, 9.0f);
    }

    g.setColour (Colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("IN", plot.removeFromBottom (14).removeFromRight (30), juce::Justification::centredRight);
    g.drawText ("OUT", r.reduced (12).removeFromTop (14), juce::Justification::centredLeft);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
}

EraCEditor::EraCEditor (EraCProcessor& p)
    : EraEditorBase (p, 920, 550, Colours::gold, "Era-C", "Golden Era Compressor"),
      proc (p), history (Colours::gold), curve (p), style (p.apvts, "style", {}, 2),
      thr (p.apvts, "threshold", "Threshold"), ratio (p.apvts, "ratio", "Ratio"), knee (p.apvts, "knee", "Knee"),
      attack (p.apvts, "attack", "Attack"), release (p.apvts, "release", "Release"),
      makeup (p.apvts, "makeup", "Makeup", true), mix (p.apvts, "mix", "Mix", false, Colours::magenta),
      schpf (p.apvts, "schpf", "SC Hi-Pass", false, Colours::teal),
      autoRel (p.apvts, "autorel", "AUTO REL"), autoMk (p.apvts, "automakeup", "AUTO GAIN")
{
    history.minDb = -48.0f;
    history.getLineDb = [this] { return proc.apvts.getRawParameterValue ("threshold")->load(); };
    history.lineLabel = "THRESHOLD";

    for (juce::Component* c : std::initializer_list<juce::Component*> { &history, &curve, &style, &inMeter, &grMeter, &outMeter, &panel })
        body().addAndMakeVisible (c);
    history.setBounds (12, 56, 588, 300);
    curve.setBounds (608, 56, 236, 236);
    style.setBounds (608, 300, 236, 56);
    inMeter.setBounds (852, 56, 16, 300);
    grMeter.setBounds (872, 56, 16, 300);
    outMeter.setBounds (892, 56, 16, 300);
    grMeter.setRange (0, 24.0f);

    panel.setBounds (12, 366, 896, 152);
    int x = 12;
    for (auto* k : { &thr, &ratio, &knee, &attack, &release, &makeup, &mix, &schpf })
    {
        panel.addAndMakeVisible (k);
        k->setBounds (x, 12, 92, 130);
        x += 94;
    }
    panel.addAndMakeVisible (autoRel);
    panel.addAndMakeVisible (autoMk);
    autoRel.setBounds (776, 40, 108, 30);
    autoMk.setBounds (776, 82, 108, 30);
    autoRel.setTooltip ("Program-dependent release: fast on transients, slower on sustained parts");
    autoMk.setTooltip ("Automatic make-up gain");

    startTimerHz (30);
}

void EraCEditor::timerCallback()
{
    history.update (proc.history);
    curve.repaint();
    inMeter.setLevel (proc.meterIn.exchange (0.0f));
    outMeter.setLevel (proc.meterOut.exchange (0.0f));
    grMeter.setLevel (proc.meterGr.exchange (0.0f));
    makeup.setEnabled (true);
}
} // namespace erac
