#include "EraEditorBase.h"

namespace era
{
static constexpr int headerH = 46;
static constexpr int footerH = 22;

//==============================================================================
Header::Header (EraProcessorBase& p, juce::String t, juce::String s, juce::Colour a)
    : proc (p), title (std::move (t)), subtitle (std::move (s)), accent (a)
{
    addAndMakeVisible (presetBox);
    for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &abBtn, &copyBtn, &folderBtn })
        addAndMakeVisible (b);

    presetBox.setTextWhenNothingSelected ("Init");
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 0 && id - 1 != proc.presets->getCurrentIndex())
            proc.presets->load (id - 1);
        else if (id > 0)
            proc.presets->load (id - 1); // reload = revert changes
    };
    prevBtn.onClick = [this] { proc.presets->loadPrevious(); };
    nextBtn.onClick = [this] { proc.presets->loadNext(); };
    saveBtn.onClick = [this] { showSaveDialog(); };
    folderBtn.onClick = [this]
    {
        auto f = proc.presets->getUserPresetFolder();
        f.createDirectory();
        proc.presets->rescanUserPresets();
        f.startAsProcess();
    };
    abBtn.onClick = [this] { proc.toggleAB(); refreshLabel(); };
    copyBtn.onClick = [this] { proc.copyCurrentToOther(); };

    prevBtn.setTooltip ("Previous preset");
    nextBtn.setTooltip ("Next preset");
    saveBtn.setTooltip ("Save the current settings as a user preset");
    folderBtn.setTooltip ("Open the user preset folder (and rescan it)");
    abBtn.setTooltip ("Switch between setting A and B");
    copyBtn.setTooltip ("Copy the current setting to the other A/B slot");

    proc.presets->addChangeListener (this);
    rebuildList();
    startTimerHz (8);
}

Header::~Header()
{
    proc.presets->removeChangeListener (this);
}

void Header::rebuildList()
{
    presetBox.clear (juce::dontSendNotification);
    const auto& entries = proc.presets->getEntries();
    juce::String lastCat;
    for (int i = 0; i < (int) entries.size(); ++i)
    {
        const auto& e = entries[(size_t) i];
        if (e.category != lastCat)
        {
            if (i > 0) presetBox.addSeparator();
            presetBox.addSectionHeading (e.category);
            lastCat = e.category;
        }
        presetBox.addItem (e.name, i + 1);
    }
    refreshLabel();
}

void Header::refreshLabel()
{
    const auto name = proc.presets->getCurrentName();
    lastModified = proc.presets->isModified();
    presetBox.setText (lastModified ? name + " *" : name, juce::dontSendNotification);
    abBtn.setButtonText (proc.getABSlot() == 0 ? "A" : "B");
    copyBtn.setButtonText (proc.getABSlot() == 0 ? "A>B" : "B>A");
}

void Header::timerCallback()
{
    if (proc.presets->isModified() != lastModified)
        refreshLabel();
}

void Header::showSaveDialog()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name your preset:", juce::MessageBoxIconType::NoIcon, this);
    w->setLookAndFeel (&getLookAndFeel());
    w->addTextEditor ("name", proc.presets->getCurrentName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<Header> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
            safe->proc.presets->saveUserPreset (w->getTextEditorContents ("name"));
    }), true);
}

void Header::resized()
{
    auto r = getLocalBounds().reduced (10, 9);
    folderBtn.setBounds (r.removeFromRight (30));
    r.removeFromRight (6);
    copyBtn.setBounds (r.removeFromRight (42));
    r.removeFromRight (4);
    abBtn.setBounds (r.removeFromRight (30));

    auto centre = getLocalBounds().withSizeKeepingCentre (400, getHeight()).reduced (0, 9);
    prevBtn.setBounds (centre.removeFromLeft (26));
    centre.removeFromLeft (4);
    saveBtn.setBounds (centre.removeFromRight (52));
    centre.removeFromRight (6);
    nextBtn.setBounds (centre.removeFromRight (26));
    centre.removeFromRight (4);
    presetBox.setBounds (centre);
}

void Header::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient grad (Colours::panelLight, 0, 0, Colours::panel, 0, r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (r);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRect (r.removeFromBottom (2.0f));

    // Logo: 8cee chip
    auto logo = juce::Rectangle<float> (12, 10, 44, 26);
    g.setColour (Colours::gold);
    g.fillRoundedRectangle (logo, 4.0f);
    g.setColour (Colours::display);
    g.setFont (font (16.0f, true).italicised());
    g.drawText ("8cee", logo, juce::Justification::centred);

    g.setColour (Colours::text);
    g.setFont (font (20.0f, true));
    g.drawText (title, 66, 6, 120, 22, juce::Justification::centredLeft);
    g.setColour (accent);
    g.setFont (font (10.5f, true));
    g.drawText (subtitle.toUpperCase(), 66, 26, 200, 14, juce::Justification::centredLeft);
}

//==============================================================================
void EraEditorBase::Content::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
    auto r = getLocalBounds().toFloat();
    auto foot = r.removeFromBottom ((float) footerH);
    g.setColour (Colours::panel);
    g.fillRect (foot);
    g.setColour (Colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("8cee GOLDEN ERA BUNDLE  " + juce::String (juce::CharPointer_UTF8 ("\xc2\xb7")) + "  v" ERA_VERSION_STRING,
                foot.reduced (10, 0), juce::Justification::centredLeft);
    g.setColour (Colours::textDim.withAlpha (0.7f));
    g.drawText (footerText, foot.reduced (10, 0).withTrimmedRight (18), juce::Justification::centredRight);
}

EraEditorBase::EraEditorBase (EraProcessorBase& p, int w, int h, juce::Colour a, juce::String title, juce::String subtitle)
    : juce::AudioProcessorEditor (p), lnf (a), accent (a), proc (p), baseW (w), baseH (h),
      content (a, "Double-click knob = reset  |  double-click value = type  |  drag corner = resize"),
      header (p, std::move (title), std::move (subtitle), a)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    content.setBounds (0, 0, baseW, baseH);
    content.addAndMakeVisible (header);
    header.setBounds (0, 0, baseW, headerH);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseW / (double) baseH);
        c->setSizeLimits (juce::roundToInt (baseW * 0.6), juce::roundToInt (baseH * 0.6), baseW * 2, baseH * 2);
    }
    setSize (juce::roundToInt ((float) baseW * proc.uiScale), juce::roundToInt ((float) baseH * proc.uiScale));
}

EraEditorBase::~EraEditorBase()
{
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> EraEditorBase::getBodyArea() const
{
    return { 0, headerH, baseW, baseH - headerH - footerH };
}

void EraEditorBase::resized()
{
    const float scale = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (scale));
    proc.uiScale = scale;
}
} // namespace era
