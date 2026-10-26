#pragma once
#include "EraEditorBase.h"
#include "SpectrumAnalyzer.h"
#include "EraQProcessor.h"

namespace eraq
{
/** The big interactive EQ display: analyzer, curve and draggable band nodes. */
class EqDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EqDisplay (EraQProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override {}

    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }

    int selected = 0;
    std::function<void (int)> onSelect;

    static constexpr float minHz = 10.0f, maxHz = 30000.0f, rangeDb = 18.0f;

private:
    void timerCallback() override;
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (1.0f); }
    juce::Point<float> nodePos (int band) const;
    int hitNode (juce::Point<float>) const;
    float yToDb (float y) const;
    float dbToY (float db) const;
    void setParam (int band, const char* name, float realValue);
    void showBandMenu (int band);
    float bandDbAt (const BandSetting&, const era::SvfCoeffs*, int nStages, float hz) const;

    EraQProcessor& proc;
    era::SpectrumAnalyzer preAn, postAn;
    int hovered = -1, dragging = -1;
};

class EraQEditor : public era::EraEditorBase, private juce::Timer
{
public:
    explicit EraQEditor (EraQProcessor&);
    void paintOverChildren (juce::Graphics&) override {}

private:
    struct BandPanel : juce::Component
    {
        void paint (juce::Graphics&) override;
        juce::Colour colour;
        int band = 0;
    };

    void selectBand (int band);
    void timerCallback() override;

    EraQProcessor& proc;
    EqDisplay display;
    BandPanel panel;
    juce::OwnedArray<juce::TextButton> bandButtons;
    std::unique_ptr<era::ParamToggle> powerBtn;
    std::unique_ptr<era::ChoiceSelector> typeSel, slopeSel, placeSel;
    std::unique_ptr<era::Knob> freqKnob, gainKnob, qKnob;
    era::Knob colorKnob, outKnob;
    era::ChoiceSelector analyzerSel;
    era::LevelMeter inMeter { era::LevelMeter::Mode::level, "IN" }, outMeter { era::LevelMeter::Mode::level, "OUT" };
};
} // namespace eraq
