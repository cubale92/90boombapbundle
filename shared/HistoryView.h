#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "DspUtils.h"
#include "EraTheme.h"

namespace era
{
/** Scrolling level + gain-reduction graph (Pro-C / Pro-L style). Pull frames from a
    HistoryFifo with update(), then it paints input (dim), output (accent) and GR (red). */
class HistoryView : public juce::Component
{
public:
    HistoryView (juce::Colour accentColour, int numFrames = 640) : accent (accentColour), frames ((size_t) numFrames) {}

    void update (HistoryFifo& fifo)
    {
        HistoryFrame f;
        bool any = false;
        while (fifo.pop (f))
        {
            frames[(size_t) head] = f;
            head = (head + 1) % (int) frames.size();
            any = true;
        }
        if (any) repaint();
    }

    float minDb = -48.0f, maxDb = 3.0f, grRange = 24.0f;
    std::function<float()> getLineDb;   // threshold / ceiling line, optional
    juce::String lineLabel;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::display);
        g.fillRoundedRectangle (r, 6.0f);
        auto plot = r.reduced (1.0f, 6.0f);

        auto yLevel = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, plot.getBottom(), plot.getY()); };
        auto yGr = [&] (float gr) { return juce::jmap (juce::jlimit (0.0f, grRange, gr), 0.0f, grRange, plot.getY(), plot.getBottom()); };

        // grid
        g.setFont (font (10.0f, true));
        for (float db = 0.0f; db > minDb; db -= 6.0f)
        {
            const float y = yLevel (db);
            g.setColour (db == 0.0f ? Colours::gridStrong : Colours::grid.withAlpha (0.55f));
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
            g.setColour (Colours::textDim.withAlpha (0.7f));
            g.drawText (juce::String ((int) db), juce::Rectangle<float> (plot.getRight() - 30, y - 7, 26, 14), juce::Justification::centredRight);
        }

        const int n = (int) frames.size();
        const float dx = plot.getWidth() / (float) (n - 1);
        juce::Path inP, outP, grP;
        inP.startNewSubPath (plot.getX(), plot.getBottom());
        outP.startNewSubPath (plot.getX(), plot.getBottom());
        for (int i = 0; i < n; ++i)
        {
            const auto& f = frames[(size_t) ((head + i) % n)];
            const float x = plot.getX() + (float) i * dx;
            inP.lineTo (x, yLevel (f.inDb));
            outP.lineTo (x, yLevel (f.outDb));
            if (i == 0) grP.startNewSubPath (x, yGr (f.grDb)); else grP.lineTo (x, yGr (f.grDb));
        }
        inP.lineTo (plot.getRight(), plot.getBottom()); inP.closeSubPath();
        outP.lineTo (plot.getRight(), plot.getBottom()); outP.closeSubPath();

        g.setColour (Colours::text.withAlpha (0.22f));
        g.fillPath (inP);
        g.setColour (Colours::text.withAlpha (0.35f));
        g.strokePath (inP, juce::PathStrokeType (0.8f));
        g.setColour (accent.withAlpha (0.45f));
        g.fillPath (outP);
        g.setColour (Colours::meterRed.withAlpha (0.25f));
        juce::Path grFill (grP);
        grFill.lineTo (plot.getRight(), plot.getY());
        grFill.lineTo (plot.getX(), plot.getY());
        grFill.closeSubPath();
        g.fillPath (grFill);
        g.setColour (Colours::meterRed);
        g.strokePath (grP, juce::PathStrokeType (1.8f));

        if (getLineDb)
        {
            const float y = yLevel (getLineDb());
            const float dashes[] = { 6.0f, 4.0f };
            g.setColour (accent.withAlpha (0.9f));
            g.drawDashedLine ({ plot.getX(), y, plot.getRight(), y }, dashes, 2, 1.2f);
            g.setFont (font (10.5f, true));
            g.drawText (lineLabel, juce::Rectangle<float> (plot.getX() + 6, y - 15, 160, 13), juce::Justification::centredLeft);
        }

        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }

private:
    juce::Colour accent;
    std::vector<HistoryFrame> frames;
    int head = 0;
};
} // namespace era
