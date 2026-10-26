#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace era
{
/** A factory preset: values are in real parameter units (Hz, dB, ms, choice index, 0/1).
    Any parameter not listed is reset to its default. */
struct FactoryPreset
{
    juce::String category, name;
    std::vector<std::pair<juce::String, float>> values;
};

class PresetManager : public juce::ChangeBroadcaster,
                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    struct Entry
    {
        juce::String category, name;
        bool isFactory = true;
        int factoryIndex = -1;
        juce::File file;
    };

    PresetManager (juce::AudioProcessorValueTreeState&, const juce::String& pluginName, std::vector<FactoryPreset> factory);
    ~PresetManager() override;

    const std::vector<Entry>& getEntries() const { return entries; }
    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const;
    bool isModified() const { return modified.load(); }

    void load (int index);
    void loadNext();
    void loadPrevious();
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();
    juce::File getUserPresetFolder() const;

    /** Snapshot / restore of all parameters (normalised), used for A/B and presets. */
    std::map<juce::String, float> captureNormalised() const;
    void applyNormalised (const std::map<juce::String, float>&, bool resetOthersToDefault);

    /** Called from setStateInformation to restore the preset label. */
    void restoreLabel (const juce::String& name, bool wasModified);

    const std::vector<FactoryPreset>& getFactoryPresets() const { return factory; }

private:
    void parameterChanged (const juce::String&, float) override;
    void applyFactory (const FactoryPreset&);
    void finishLoad (int index);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String pluginName;
    std::vector<FactoryPreset> factory;
    std::vector<Entry> entries;
    int currentIndex = 0;
    juce::String customName;
    std::atomic<bool> modified { false };
    bool loading = false;
};
} // namespace era
