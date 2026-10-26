#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "EraProcessorBase.h"
#include "EraLookAndFeel.h"
#include "EraWidgets.h"

namespace era
{
/** Top bar: logo, plug-in name, preset browser, save, A/B compare. */
class Header : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    Header (EraProcessorBase&, juce::String title, juce::String subtitle, juce::Colour accent);
    ~Header() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { rebuildList(); }
    void timerCallback() override;
    void rebuildList();
    void refreshLabel();
    void showSaveDialog();

    EraProcessorBase& proc;
    juce::String title, subtitle;
    juce::Colour accent;
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, abBtn { "A" }, copyBtn { "A>B" }, folderBtn { "..." };
    bool lastModified = false;
};

/** Base editor: draws the chassis, owns the header and footer and scales the whole UI. */
class EraEditorBase : public juce::AudioProcessorEditor
{
public:
    EraEditorBase (EraProcessorBase&, int baseWidth, int baseHeight, juce::Colour accent,
                   juce::String title, juce::String subtitle);
    ~EraEditorBase() override;

    void resized() override;
    void paint (juce::Graphics&) override {}

protected:
    /** Area available to the derived editor, in base (unscaled) coordinates. */
    juce::Rectangle<int> getBodyArea() const;
    juce::Component& body() { return content; }

    EraLookAndFeel lnf;
    juce::Colour accent;

private:
    struct Content : juce::Component
    {
        Content (juce::Colour a, juce::String t) : accent (a), footerText (std::move (t)) {}
        void paint (juce::Graphics&) override;
        juce::Colour accent;
        juce::String footerText;
    };

    EraProcessorBase& proc;
    const int baseW, baseH;
    Content content;
    Header header;
    juce::TooltipWindow tooltips { this, 700 };
};
} // namespace era
