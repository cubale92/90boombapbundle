#include "EraLookAndFeel.h"

namespace era
{
EraLookAndFeel::EraLookAndFeel (juce::Colour a) : accent (a)
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Colours::background);
    setColour (PopupMenu::backgroundColourId, Colours::panelLight);
    setColour (PopupMenu::textColourId, Colours::text);
    setColour (PopupMenu::headerTextColourId, accent);
    setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, Colours::text);
    setColour (ComboBox::textColourId, Colours::text);
    setColour (ComboBox::backgroundColourId, Colours::display);
    setColour (ComboBox::outlineColourId, Colours::outline);
    setColour (ComboBox::arrowColourId, Colours::textDim);
    setColour (TextButton::textColourOffId, Colours::textDim);
    setColour (TextButton::textColourOnId, Colours::display);
    setColour (TextButton::buttonColourId, Colours::panelLight);
    setColour (TextButton::buttonOnColourId, accent);
    setColour (Slider::rotarySliderFillColourId, accent);
    setColour (Slider::textBoxTextColourId, Colours::text);
    setColour (Slider::textBoxBackgroundColourId, Colours::display);
    setColour (Slider::textBoxOutlineColourId, accent.withAlpha (0.6f));
    setColour (TextEditor::backgroundColourId, Colours::display);
    setColour (TextEditor::textColourId, Colours::text);
    setColour (TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (TextEditor::outlineColourId, Colours::outline);
    setColour (TextEditor::focusedOutlineColourId, accent);
    setColour (CaretComponent::caretColourId, accent);
    setColour (Label::textColourId, Colours::text);
    setColour (AlertWindow::backgroundColourId, Colours::panel);
    setColour (AlertWindow::textColourId, Colours::text);
    setColour (AlertWindow::outlineColourId, accent);
    setDefaultSansSerifTypefaceName ({});
}

void EraLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace juce;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcRadius = radius - 3.0f;
    const auto fill = slider.findColour (Slider::rotarySliderFillColourId);
    const float angle = startAngle + pos * (endAngle - startAngle);
    const bool enabled = slider.isEnabled();

    // Track
    Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::grid);
    g.strokePath (track, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));

    // Value arc (bipolar params start at 12 o'clock)
    const bool bipolar = slider.getProperties().getWithDefault ("bipolar", false);
    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (enabled ? fill : Colours::textDim.withAlpha (0.4f));
        g.strokePath (arc, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Knob body
    const float bodyR = arcRadius - 7.0f;
    ColourGradient grad (Colour (0xff35334a), centre.x, centre.y - bodyR, Colour (0xff15141d), centre.x, centre.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Colours::outline.brighter (0.2f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    // Pointer
    Path ptr;
    const float pw = 2.6f;
    ptr.addRoundedRectangle (-pw * 0.5f, -bodyR + 3.0f, pw, bodyR * 0.48f, pw * 0.5f);
    g.setColour (enabled ? Colours::text : Colours::textDim);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (centre));

    if (slider.isMouseOverOrDragging() && enabled)
    {
        g.setColour (fill.withAlpha (0.12f));
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2, bodyR * 2);
    }
}

void EraLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    using namespace juce;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto base = on ? b.findColour (TextButton::buttonOnColourId) : b.findColour (TextButton::buttonColourId);
    if (! b.isEnabled()) base = base.withMultipliedAlpha (0.4f);
    if (down)      base = base.darker (0.2f);
    else if (over) base = base.brighter (on ? 0.1f : 0.25f);
    g.setColour (base);
    g.fillRoundedRectangle (r, 4.0f);
    if (! on)
    {
        g.setColour (Colours::outline);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
}

void EraLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    using namespace juce;
    g.setFont (getTextButtonFont (b, b.getHeight()));
    auto c = b.findColour (b.getToggleState() ? TextButton::textColourOnId : TextButton::textColourOffId);
    if (! b.isEnabled()) c = c.withMultipliedAlpha (0.5f);
    g.setColour (c);
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (3, 1), Justification::centred, 1);
}

void EraLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver (true) ? accent.withAlpha (0.7f) : box.findColour (ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.findColour (ComboBox::arrowColourId));
    g.fillPath (arrow);
}

void EraLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void EraLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Colours::panelLight);
    g.setColour (Colours::outline);
    g.drawRect (0, 0, w, h);
}

void EraLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (Colours::display);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 3.0f);
    g.setColour (accent.withAlpha (0.6f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 3.0f, 1.0f);
}

juce::Label* EraLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (12.0f, true));
    return l;
}
} // namespace era
