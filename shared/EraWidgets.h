#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "EraTheme.h"

namespace era
{
/** Rotary knob with title and live value readout, bound to one parameter. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title,
          bool bipolar = false, juce::Colour colour = {});

    void paint (juce::Graphics&) override;
    void resized() override;
    void setColour (juce::Colour c) { slider.setColour (juce::Slider::rotarySliderFillColourId, c); repaint(); }

    juce::Slider slider;

private:
    void mouseDoubleClick (const juce::MouseEvent&) override;
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    juce::Rectangle<int> valueArea;
};

/** Segmented selector for a choice parameter (FabFilter-style mode buttons). */
class ChoiceSelector : public juce::Component
{
public:
    ChoiceSelector (juce::AudioProcessorValueTreeState&, const juce::String& paramId,
                    juce::StringArray labels = {}, int columns = 0);
    void resized() override;
    std::function<void (int)> onChange;
    int getIndex() const { return current; }

private:
    void update (float v);
    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int columns = 0, current = 0;
};

/** Toggle button bound to a bool parameter. */
class ParamToggle : public juce::TextButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

/** Vertical peak meter with hold. Feed it with setLevel() from a UI timer. */
class LevelMeter : public juce::Component
{
public:
    enum class Mode { level, gainReduction };
    explicit LevelMeter (Mode m = Mode::level, juce::String label = {}) : mode (m), label (std::move (label)) {}

    void setLevel (float linearOrGrDb);
    void paint (juce::Graphics&) override;
    void setRange (float minDb, float maxDb) { rangeMin = minDb; rangeMax = maxDb; }

private:
    Mode mode;
    juce::String label;
    float display = -100.0f, hold = -100.0f;
    int holdCounter = 0;
    float rangeMin = -60.0f, rangeMax = 6.0f;
};

/** Small helper to draw a titled panel background. */
void drawPanel (juce::Graphics&, juce::Rectangle<float>, const juce::String& title = {}, juce::Colour accent = {});
} // namespace era
