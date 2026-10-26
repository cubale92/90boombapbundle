#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EraTheme.h"

namespace era
{
class EraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit EraLookAndFeel (juce::Colour accent);

    juce::Colour accent;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f, true); }
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.5f, true); }
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&) override {}
    juce::Label* createSliderTextBox (juce::Slider&) override;
};
} // namespace era
