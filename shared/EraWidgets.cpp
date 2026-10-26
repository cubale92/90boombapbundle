#include "EraWidgets.h"
#include "DspUtils.h"

namespace era
{
//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& t,
            bool bipolar, juce::Colour colour)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setMouseDragSensitivity (220);
    slider.getProperties().set ("bipolar", bipolar);
    if (! colour.isTransparent())
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    slider.onValueChange = [this] { repaint (valueArea); };
    slider.setPopupMenuEnabled (false);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (16);
    valueArea = r.removeFromBottom (17);
    const int side = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (isEnabled() ? Colours::textDim : Colours::textDim.withAlpha (0.4f));
    g.setFont (font (11.5f, true));
    g.drawFittedText (title.toUpperCase(), getLocalBounds().removeFromTop (16), juce::Justification::centred, 1);

    g.setColour (isEnabled() ? Colours::text : Colours::textDim.withAlpha (0.5f));
    g.setFont (font (12.5f, true));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()), valueArea, juce::Justification::centred, 1);
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click on the value readout = type in a value
    if (! valueArea.contains (e.getPosition()))
        return;

    auto* ed = new juce::TextEditor();
    ed->setJustification (juce::Justification::centred);
    ed->setFont (font (12.5f, true));
    ed->setText (slider.getTextFromValue (slider.getValue()), false);
    ed->selectAll();
    ed->setBounds (valueArea.reduced (2, 0));
    addAndMakeVisible (ed);
    ed->grabKeyboardFocus();
    auto done = std::make_shared<bool> (false);
    auto finish = [this, ed, done] (bool apply)
    {
        if (*done) return;
        *done = true;
        if (apply)
            slider.setValue (slider.getValueFromText (ed->getText()), juce::sendNotificationSync);
        juce::MessageManager::callAsync ([ed] { delete ed; });
    };
    ed->onReturnKey = [finish] { finish (true); };
    ed->onEscapeKey = [finish] { finish (false); };
    ed->onFocusLost = [finish] { finish (true); };
}

//==============================================================================
ChoiceSelector::ChoiceSelector (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                                juce::StringArray labels, int cols)
    : columns (cols)
{
    auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id));
    jassert (p != nullptr);
    if (labels.isEmpty())
        labels = p->choices;

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { if (attachment) attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*p, [this] (float v) { update (v); });
    attachment->sendInitialUpdate();
}

void ChoiceSelector::update (float v)
{
    current = juce::roundToInt (v);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == current, juce::dontSendNotification);
    if (onChange) onChange (current);
}

void ChoiceSelector::resized()
{
    const int n = buttons.size();
    const int cols = columns > 0 ? columns : n;
    const int rows = (n + cols - 1) / cols;
    const float gap = 4.0f;
    const float bw = ((float) getWidth() - gap * (float) (cols - 1)) / (float) cols;
    const float bh = ((float) getHeight() - gap * (float) (rows - 1)) / (float) rows;
    for (int i = 0; i < n; ++i)
    {
        const int r = i / cols, c = i % cols;
        buttons[i]->setBounds (juce::Rectangle<float> ((float) c * (bw + gap), (float) r * (bh + gap), bw, bh).toNearestInt());
    }
}

//==============================================================================
ParamToggle::ParamToggle (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& label)
    : juce::TextButton (label)
{
    setClickingTogglesState (true);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *this);
}

//==============================================================================
void LevelMeter::setLevel (float v)
{
    const float db = mode == Mode::level ? gainToDb (v, -100.0f) : v;
    if (mode == Mode::level)
        display = db > display ? db : juce::jmax (db, display - 1.2f);
    else
        display = db > display ? db : juce::jmax (db, display - 0.6f);

    if (db >= hold) { hold = db; holdCounter = 45; }
    else if (--holdCounter <= 0) hold = juce::jmax (display, hold - 0.8f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::Rectangle<float> labelArea;
    if (label.isNotEmpty())
        labelArea = r.removeFromBottom (14.0f);

    g.setColour (Colours::display);
    g.fillRoundedRectangle (r, 2.5f);

    auto inner = r.reduced (2.0f);
    auto toY = [&] (float db)
    {
        return juce::jmap (juce::jlimit (rangeMin, rangeMax, db), rangeMin, rangeMax, inner.getBottom(), inner.getY());
    };

    if (mode == Mode::level)
    {
        const float y = toY (display);
        if (display > rangeMin)
        {
            juce::ColourGradient grad (Colours::meterRed, 0, toY (0.0f), Colours::meterGreen, 0, inner.getBottom(), false);
            grad.addColour (juce::jlimit (0.0, 1.0, (double) ((toY (-6.0f) - toY (0.0f)) / (inner.getBottom() - toY (0.0f)))), Colours::meterYellow);
            g.setGradientFill (grad);
            g.fillRect (inner.withTop (y));
        }
        if (hold > rangeMin)
        {
            g.setColour (hold > 0.0f ? Colours::meterRed : Colours::text.withAlpha (0.8f));
            g.fillRect (inner.getX(), toY (hold) - 1.0f, inner.getWidth(), 1.5f);
        }
    }
    else
    {
        // Gain reduction: grows downwards from the top, range 0..rangeMax dB
        auto toYgr = [&] (float db) { return juce::jmap (juce::jlimit (0.0f, rangeMax, db), 0.0f, rangeMax, inner.getY(), inner.getBottom()); };
        if (display > 0.01f)
        {
            g.setColour (Colours::meterRed.withAlpha (0.85f));
            g.fillRect (inner.withBottom (toYgr (display)));
        }
        if (hold > 0.05f)
        {
            g.setColour (Colours::text.withAlpha (0.8f));
            g.fillRect (inner.getX(), toYgr (hold) - 1.0f, inner.getWidth(), 1.5f);
        }
    }

    if (label.isNotEmpty())
    {
        g.setColour (Colours::textDim);
        g.setFont (font (9.5f, true));
        g.drawFittedText (label, labelArea.toNearestInt().expanded (3, 0), juce::Justification::centred, 1, 0.7f);
    }
}

//==============================================================================
void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, juce::Colour accent)
{
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        auto t = r.reduced (10.0f, 6.0f).removeFromTop (14.0f);
        if (! accent.isTransparent())
        {
            g.setColour (accent);
            g.fillRoundedRectangle (t.removeFromLeft (3.0f), 1.5f);
            t.removeFromLeft (6.0f);
        }
        g.setColour (Colours::textDim);
        g.setFont (font (11.0f, true));
        g.drawText (title.toUpperCase(), t, juce::Justification::centredLeft);
    }
}
} // namespace era
