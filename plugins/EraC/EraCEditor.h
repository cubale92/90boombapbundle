#pragma once
#include "EraEditorBase.h"
#include "HistoryView.h"
#include "EraCProcessor.h"

namespace erac
{
class CurveView : public juce::Component
{
public:
    explicit CurveView (EraCProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    EraCProcessor& proc;
};

class EraCEditor : public era::EraEditorBase, private juce::Timer
{
public:
    explicit EraCEditor (EraCProcessor&);
private:
    void timerCallback() override;
    struct Panel : juce::Component { void paint (juce::Graphics& g) override { era::drawPanel (g, getLocalBounds().toFloat()); } };

    EraCProcessor& proc;
    era::HistoryView history;
    CurveView curve;
    era::ChoiceSelector style;
    Panel panel;
    era::Knob thr, ratio, knee, attack, release, makeup, mix, schpf;
    era::ParamToggle autoRel, autoMk;
    era::LevelMeter inMeter { era::LevelMeter::Mode::level, "IN" },
                    grMeter { era::LevelMeter::Mode::gainReduction, "GR" },
                    outMeter { era::LevelMeter::Mode::level, "OUT" };
};
} // namespace erac
