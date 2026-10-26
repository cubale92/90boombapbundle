#include "EraQEditor.h"

namespace eraq
{
using namespace era;

//==============================================================================
EqDisplay::EqDisplay (EraQProcessor& p) : proc (p), preAn (12), postAn (12)
{
    startTimerHz (30);
    setRepaintsOnMouseActivity (false);
}

void EqDisplay::timerCallback()
{
    preAn.setSampleRate (proc.getCurrentSampleRate());
    postAn.setSampleRate (proc.getCurrentSampleRate());
    preAn.update (proc.preFifo);
    postAn.update (proc.postFifo);
    repaint();
}

float EqDisplay::dbToY (float db) const
{
    const auto r = plotArea();
    return juce::jmap (db, -rangeDb, rangeDb, r.getBottom() - 16.0f, r.getY() + 8.0f);
}

float EqDisplay::yToDb (float y) const
{
    const auto r = plotArea();
    return juce::jmap (y, r.getBottom() - 16.0f, r.getY() + 8.0f, -rangeDb, rangeDb);
}

juce::Point<float> EqDisplay::nodePos (int band) const
{
    const auto b = proc.getBandSetting (band);
    const float x = freqToX (juce::jlimit (minHz, maxHz, b.freq), minHz, maxHz, plotArea());
    const float y = typeUsesGain (b.type) ? dbToY (juce::jlimit (-rangeDb, rangeDb, b.gain)) : dbToY (0.0f);
    return { x, y };
}

int EqDisplay::hitNode (juce::Point<float> p) const
{
    // Selected band gets priority, then enabled bands
    if (proc.getBandSetting (selected).on && nodePos (selected).getDistanceFrom (p) < 11.0f)
        return selected;
    for (int i = numBands - 1; i >= 0; --i)
        if (proc.getBandSetting (i).on && nodePos (i).getDistanceFrom (p) < 11.0f)
            return i;
    return -1;
}

float EqDisplay::bandDbAt (const BandSetting&, const era::SvfCoeffs* st, int ns, float hz) const
{
    double mag = 1.0;
    for (int s = 0; s < ns; ++s)
        mag *= std::abs (st[s].response (hz, proc.getCurrentSampleRate()));
    return (float) (20.0 * std::log10 (std::max (1.0e-9, mag)));
}

void EqDisplay::setParam (int band, const char* name, float value)
{
    if (auto* p = proc.param (pid (band, name)))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void EqDisplay::paint (juce::Graphics& g)
{
    const auto r = plotArea();
    g.setColour (Colours::display);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

    // Grid
    drawFrequencyGrid (g, r, minHz, maxHz);
    g.setFont (font (10.0f, true));
    for (float db = -rangeDb + 6.0f; db < rangeDb; db += 6.0f)
    {
        const float y = dbToY (db);
        g.setColour (db == 0.0f ? Colours::gridStrong : Colours::grid.withAlpha (0.6f));
        g.drawHorizontalLine (juce::roundToInt (y), r.getX(), r.getRight());
        g.setColour (Colours::textDim.withAlpha (0.8f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (r.getRight() - 28, y - 7, 24, 14),
                    juce::Justification::centredRight);
    }

    // Analyzer (-84..+6 dB maps to the plot height)
    const int an = (int) proc.apvts.getRawParameterValue ("analyzer")->load();
    auto anArea = r.withTrimmedBottom (16.0f);
    if (an == 1 || an == 3)
    {
        auto pre = preAn.createPath (anArea, -84.0f, 6.0f, minHz, maxHz, true);
        g.setColour (Colours::textDim.withAlpha (0.18f));
        g.fillPath (pre);
    }
    if (an >= 2)
    {
        auto post = postAn.createPath (anArea, -84.0f, 6.0f, minHz, maxHz, true);
        g.setColour (Colours::teal.withAlpha (0.16f));
        g.fillPath (post);
        auto line = postAn.createPath (anArea, -84.0f, 6.0f, minHz, maxHz, false);
        g.setColour (Colours::teal.withAlpha (0.45f));
        g.strokePath (line, juce::PathStrokeType (1.0f));
    }

    // Per-band stages
    BandSetting settings[numBands];
    era::SvfCoeffs stages[numBands][4];
    int ns[numBands] {};
    bool anySide = false;
    for (int i = 0; i < numBands; ++i)
    {
        settings[i] = proc.getBandSetting (i);
        ns[i] = settings[i].on ? makeBandStages (settings[i], proc.getCurrentSampleRate(), stages[i]) : 0;
        anySide |= (settings[i].on && settings[i].place == Side);
    }

    // Selected band fill
    if (settings[selected].on)
    {
        juce::Path fill;
        const float y0 = dbToY (0.0f);
        fill.startNewSubPath (r.getX(), y0);
        for (float x = r.getX(); x <= r.getRight(); x += 2.0f)
        {
            const float db = bandDbAt (settings[selected], stages[selected], ns[selected], xToFreq (x, minHz, maxHz, r));
            fill.lineTo (x, dbToY (juce::jlimit (-rangeDb - 2.0f, rangeDb + 2.0f, db)));
        }
        fill.lineTo (r.getRight(), y0);
        fill.closeSubPath();
        g.setColour (Colours::band (selected).withAlpha (0.22f));
        g.fillPath (fill);
    }

    // Total curves (stereo + mid, and side)
    auto makeCurve = [&] (bool side)
    {
        juce::Path p;
        for (float x = r.getX(); x <= r.getRight(); x += 1.5f)
        {
            const float hz = xToFreq (x, minHz, maxHz, r);
            float db = 0;
            for (int i = 0; i < numBands; ++i)
                if (ns[i] > 0 && ((settings[i].place == Side) == side))
                    db += bandDbAt (settings[i], stages[i], ns[i], hz);
            const float y = dbToY (juce::jlimit (-rangeDb - 4.0f, rangeDb + 4.0f, db));
            if (x == r.getX()) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    if (anySide)
    {
        g.setColour (Colours::magenta.withAlpha (0.8f));
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, makeCurve (true), dashes, 2);
        g.fillPath (dashed);
    }
    g.setColour (Colours::gold.withAlpha (0.25f));
    auto curve = makeCurve (false);
    g.strokePath (curve, juce::PathStrokeType (5.0f));
    g.setColour (Colours::gold);
    g.strokePath (curve, juce::PathStrokeType (2.0f));

    // Nodes
    for (int i = 0; i < numBands; ++i)
    {
        if (! settings[i].on) continue;
        const auto p = nodePos (i);
        const float rad = (i == selected ? 9.0f : 7.5f);
        g.setColour (Colours::band (i).withAlpha (i == hovered || i == dragging ? 1.0f : 0.85f));
        g.fillEllipse (p.x - rad, p.y - rad, rad * 2, rad * 2);
        if (i == selected)
        {
            g.setColour (Colours::text);
            g.drawEllipse (p.x - rad - 2.5f, p.y - rad - 2.5f, (rad + 2.5f) * 2, (rad + 2.5f) * 2, 1.5f);
        }
        g.setColour (Colours::display);
        g.setFont (font (11.0f, true));
        g.drawText (juce::String (i + 1), juce::Rectangle<float> (p.x - rad, p.y - rad, rad * 2, rad * 2), juce::Justification::centred);
        if (settings[i].place != Stereo)
        {
            g.setColour (Colours::text);
            g.setFont (font (9.5f, true));
            g.drawText (settings[i].place == Mid ? "M" : "S", juce::Rectangle<float> (p.x + rad, p.y - rad - 8, 12, 10),
                        juce::Justification::centredLeft);
        }
    }

    // Readout for hovered / dragged node
    const int info = dragging >= 0 ? dragging : hovered;
    if (info >= 0 && settings[info].on)
    {
        const auto p = nodePos (info);
        const auto& s = settings[info];
        static const char* types[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
        juce::String txt = juce::String (types[s.type]) + "  " + formatHz (s.freq);
        if (typeUsesGain (s.type)) txt << "  " << formatDb (s.gain);
        txt << "  Q " << juce::String (s.q, 2);
        auto box = juce::Rectangle<float> (0, 0, 220, 20).withCentre ({ p.x, p.y - 26 });
        box = box.constrainedWithin (r.reduced (4));
        g.setColour (Colours::panelLight.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (Colours::text);
        g.setFont (font (11.5f, true));
        g.drawText (txt, box, juce::Justification::centred);
    }

    if (std::none_of (settings, settings + numBands, [] (const BandSetting& b) { return b.on; }))
    {
        g.setColour (Colours::textDim);
        g.setFont (font (13.0f, true));
        g.drawText ("Double-click anywhere to add a band  |  drag = freq/gain  |  wheel = Q  |  right-click = options",
                    r.withTrimmedTop (r.getHeight() * 0.6f).withHeight (20), juce::Justification::centred);
    }

    g.setColour (Colours::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
}

void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h < 0) return;
    selected = h;
    if (onSelect) onSelect (h);

    if (e.mods.isPopupMenu()) { showBandMenu (h); return; }

    dragging = h;
    proc.param (pid (h, "freq"))->beginChangeGesture();
    proc.param (pid (h, "gain"))->beginChangeGesture();
    repaint();
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    const auto r = plotArea();
    auto pos = e.position;
    if (e.mods.isShiftDown()) // fine adjust
        pos = e.mouseDownPosition + (e.position - e.mouseDownPosition) * 0.2f;

    const auto b = proc.getBandSetting (dragging);
    const bool lockFreq = e.mods.isAltDown();
    if (! lockFreq)
        setParam (dragging, "freq", juce::jlimit (minHz, 22000.0f, xToFreq (pos.x, minHz, maxHz, r)));
    if (typeUsesGain (b.type))
        setParam (dragging, "gain", juce::jlimit (-30.0f, 30.0f, yToDb (pos.y)));
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        proc.param (pid (dragging, "freq"))->endChangeGesture();
        proc.param (pid (dragging, "gain"))->endChangeGesture();
    }
    dragging = -1;
    repaint();
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0)
    {
        setParam (h, "on", 0.0f); // double-click a node = remove it
        return;
    }

    // Add a band at the clicked spot: prefer a free band, pick a sensible type
    int free = -1;
    for (int i = 0; i < numBands && free < 0; ++i)
        if (! proc.getBandSetting (i).on) free = i;
    if (free < 0) return;

    const float hz = juce::jlimit (minHz, 22000.0f, xToFreq (e.position.x, minHz, maxHz, plotArea()));
    const int type = hz < 35.0f ? LowCut : (hz > 16000.0f ? HighCut : Bell);
    setParam (free, "type", (float) type);
    setParam (free, "freq", hz);
    setParam (free, "gain", type == Bell ? juce::jlimit (-30.0f, 30.0f, yToDb (e.position.y)) : 0.0f);
    setParam (free, "q", 1.0f);
    setParam (free, "place", 0.0f);
    setParam (free, "on", 1.0f);
    selected = free;
    if (onSelect) onSelect (free);
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int band = hitNode (e.position);
    if (band < 0 && proc.getBandSetting (selected).on) band = selected;
    if (band < 0) return;
    auto* p = proc.param (pid (band, "q"));
    const float q = proc.getBandSetting (band).q * std::pow (2.0f, w.deltaY * (e.mods.isShiftDown() ? 0.25f : 1.0f));
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.1f, 30.0f, q)));
    p->endChangeGesture();
}

void EqDisplay::showBandMenu (int band)
{
    const auto b = proc.getBandSetting (band);
    juce::PopupMenu m, types, slopes, places;
    const char* typeNames[] = { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
    for (int i = 0; i < 6; ++i) types.addItem (100 + i, typeNames[i], true, b.type == i);
    const char* slopeNames[] = { "12 dB/oct", "24 dB/oct", "48 dB/oct" };
    for (int i = 0; i < 3; ++i) slopes.addItem (200 + i, slopeNames[i], typeUsesSlope (b.type), b.slope == i);
    const char* placeNames[] = { "Stereo", "Mid", "Side" };
    for (int i = 0; i < 3; ++i) places.addItem (300 + i, placeNames[i], true, b.place == i);
    m.addSectionHeader ("Band " + juce::String (band + 1));
    m.addSubMenu ("Shape", types);
    m.addSubMenu ("Slope", slopes);
    m.addSubMenu ("Placement", places);
    m.addSeparator();
    m.addItem (1, "Reset gain to 0 dB", typeUsesGain (b.type));
    m.addItem (2, "Remove band");

    juce::Component::SafePointer<EqDisplay> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, band] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& d = *safe;
        auto set = [&] (const char* n, float v) { auto* p = d.proc.param (pid (band, n)); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); };
        if (r >= 300) set ("place", (float) (r - 300));
        else if (r >= 200) set ("slope", (float) (r - 200));
        else if (r >= 100) set ("type", (float) (r - 100));
        else if (r == 1) set ("gain", 0.0f);
        else if (r == 2) set ("on", 0.0f);
    });
}

//==============================================================================
namespace
{
struct BandButton : juce::TextButton
{
    BandButton (int b) : juce::TextButton (juce::String (b + 1)), band (b) {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const auto c = Colours::band (band);
        g.setColour (isOn ? c.withAlpha (over ? 1.0f : 0.85f) : Colours::panelLight.brighter (over ? 0.15f : 0.0f));
        g.fillRoundedRectangle (r, 4.0f);
        if (getToggleState())
        {
            g.setColour (Colours::text);
            g.drawRoundedRectangle (r, 4.0f, 2.0f);
        }
        else if (! isOn)
        {
            g.setColour (c.withAlpha (0.6f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        g.setColour (isOn ? Colours::display : Colours::textDim);
        g.setFont (font (12.5f, true));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
    int band;
    bool isOn = false;
};

// Panel geometry (base coordinates)
const juce::Rectangle<int> panelR { 12, 398, 936, 168 };
}

void EraQEditor::BandPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r);
    g.setColour (Colours::textDim);
    g.setFont (font (11.0f, true));
    auto label = [&] (const char* t, int x, int y, int w) { g.drawText (t, x, y, w, 14, juce::Justification::centredLeft); };
    label ("SHAPE", 12, 42, 100);
    label ("SLOPE", 560, 42, 100);
    label ("PLACEMENT", 560, 98, 120);
    label ("GLOBAL", 724, 10, 100);

    g.setColour (Colours::outline);
    g.drawVerticalLine (710, 10.0f, r.getBottom() - 10.0f);
}

EraQEditor::EraQEditor (EraQProcessor& p)
    : EraEditorBase (p, 960, 600, Colours::teal, "Era-Q", "Golden Era Equalizer"),
      proc (p), display (p),
      colorKnob (p.apvts, "color", "Color", false, Colours::orange),
      outKnob (p.apvts, "output", "Output", true),
      analyzerSel (p.apvts, "analyzer", juce::StringArray { "Off", "Pre", "Post", "Both" })
{
    body().addAndMakeVisible (display);
    display.setBounds (12, 56, 886, 334);
    display.onSelect = [this] (int b) { selectBand (b); };

    body().addAndMakeVisible (inMeter);
    body().addAndMakeVisible (outMeter);
    inMeter.setBounds (904, 56, 20, 334);
    outMeter.setBounds (928, 56, 20, 334);

    body().addAndMakeVisible (panel);
    panel.setBounds (panelR);

    for (int i = 0; i < numBands; ++i)
    {
        auto* b = bandButtons.add (new BandButton (i));
        b->onClick = [this, i] { selectBand (i); display.selected = i; };
        panel.addAndMakeVisible (b);
        b->setBounds (92 + i * 33, 10, 29, 24);
    }

    panel.addAndMakeVisible (colorKnob);
    panel.addAndMakeVisible (outKnob);
    panel.addAndMakeVisible (analyzerSel);
    colorKnob.setBounds (724, 34, 90, 124);
    outKnob.setBounds (826, 34, 90, 124);
    analyzerSel.setBounds (790, 8, 134, 22);

    selectBand (0);
    startTimerHz (20);
}

void EraQEditor::selectBand (int b)
{
    panel.band = b;
    panel.colour = Colours::band (b);
    for (int i = 0; i < bandButtons.size(); ++i)
        bandButtons[i]->setToggleState (i == b, juce::dontSendNotification);

    auto& s = proc.apvts;
    powerBtn = std::make_unique<ParamToggle> (s, pid (b, "on"), "ON");
    typeSel = std::make_unique<ChoiceSelector> (s, pid (b, "type"), juce::StringArray { "Bell", "Lo Shelf", "Hi Shelf", "Lo Cut", "Hi Cut", "Notch" }, 3);
    slopeSel = std::make_unique<ChoiceSelector> (s, pid (b, "slope"), juce::StringArray { "12", "24", "48" });
    placeSel = std::make_unique<ChoiceSelector> (s, pid (b, "place"), juce::StringArray { "Stereo", "Mid", "Side" });
    const auto c = Colours::band (b);
    freqKnob = std::make_unique<Knob> (s, pid (b, "freq"), "Freq", false, c);
    gainKnob = std::make_unique<Knob> (s, pid (b, "gain"), "Gain", true, c);
    qKnob = std::make_unique<Knob> (s, pid (b, "q"), "Q", false, c);

    powerBtn->setColour (juce::TextButton::buttonOnColourId, c);
    for (juce::Component* comp : std::initializer_list<juce::Component*> { powerBtn.get(), typeSel.get(), slopeSel.get(), placeSel.get(),
                                                                           freqKnob.get(), gainKnob.get(), qKnob.get() })
        panel.addAndMakeVisible (comp);

    powerBtn->setBounds (12, 10, 66, 24);
    typeSel->setBounds (12, 60, 236, 56);
    freqKnob->setBounds (262, 34, 92, 124);
    gainKnob->setBounds (360, 34, 92, 124);
    qKnob->setBounds (458, 34, 92, 124);
    slopeSel->setBounds (560, 60, 136, 26);
    placeSel->setBounds (560, 116, 136, 26);
    panel.repaint();
    timerCallback();
}

void EraQEditor::timerCallback()
{
    for (int i = 0; i < bandButtons.size(); ++i)
    {
        auto* bb = static_cast<BandButton*> (bandButtons[i]);
        const bool on = proc.getBandSetting (i).on;
        if (bb->isOn != on) { bb->isOn = on; bb->repaint(); }
    }
    const auto sel = proc.getBandSetting (panel.band);
    if (display.selected != panel.band) selectBand (display.selected);
    gainKnob->setEnabled (typeUsesGain (sel.type));
    slopeSel->setEnabled (typeUsesSlope (sel.type));

    inMeter.setLevel (proc.meterIn.exchange (0.0f));
    outMeter.setLevel (proc.meterOut.exchange (0.0f));
}
} // namespace eraq
