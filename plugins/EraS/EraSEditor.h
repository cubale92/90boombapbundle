#pragma once
#include "EraEditorBase.h"
#include "SpectrumAnalyzer.h"
#include "EraSProcessor.h"

namespace eras
{
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (EraSProcessor& p) : proc (p) {}
    void update() { an.setSampleRate (proc.getCurrentSampleRate()); if (an.update (proc.outFifo)) repaint(); }
    void paint (juce::Graphics&) override;
private:
    EraSProcessor& proc;
    era::SpectrumAnalyzer an { 12 };
};

class TransferView : public juce::Component
{
public:
    explicit TransferView (EraSProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    EraSProcessor& proc;
};

class EraSEditor : public era::EraEditorBase, private juce::Timer
{
public:
    explicit EraSEditor (EraSProcessor&);
private:
    void timerCallback() override;
    struct Panel : juce::Component { void paint (juce::Graphics& g) override { era::drawPanel (g, getLocalBounds().toFloat()); } };

    EraSProcessor& proc;
    SpectrumView spectrum;
    TransferView transfer;
    era::ChoiceSelector mode;
    Panel panel;
    era::Knob drive, bias, tone, lowCut, highCut, bits, rate, dust, mix, output;
    era::ParamToggle autoGain;
    era::LevelMeter inMeter { era::LevelMeter::Mode::level, "IN" }, outMeter { era::LevelMeter::Mode::level, "OUT" };
};
} // namespace eras
