#pragma once
#include "EraEditorBase.h"
#include "HistoryView.h"
#include "EraLProcessor.h"

namespace eral
{
class ReadoutPanel : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    float momentary = -100, shortTerm = -100, gr = 0, peak = -100;
};

class EraLEditor : public era::EraEditorBase, private juce::Timer
{
public:
    explicit EraLEditor (EraLProcessor&);
private:
    void timerCallback() override;
    struct Panel : juce::Component { void paint (juce::Graphics& g) override { era::drawPanel (g, getLocalBounds().toFloat()); } };

    EraLProcessor& proc;
    era::HistoryView history;
    ReadoutPanel readout;
    era::ChoiceSelector style;
    Panel panel;
    era::Knob gain, ceiling, release, link;
    era::ParamToggle unity;
    era::LevelMeter inMeter { era::LevelMeter::Mode::level, "IN" },
                    grMeter { era::LevelMeter::Mode::gainReduction, "GR" },
                    outMeter { era::LevelMeter::Mode::level, "OUT" };
    float grHold = 0, peakHold = -100;
    int holdCount = 0;
};
} // namespace eral
