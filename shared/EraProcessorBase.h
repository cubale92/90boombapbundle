#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PresetManager.h"
#include "DspUtils.h"

namespace era
{
/** Common base for every plug-in in the bundle: parameter tree, preset manager,
    A/B comparison, state save/restore and the standard bus layout. */
class EraProcessorBase : public juce::AudioProcessor
{
public:
    EraProcessorBase (const juce::String& pluginName, juce::AudioProcessorValueTreeState::ParameterLayout);
    ~EraProcessorBase() override;

    /** Must be called at the end of the derived constructor. */
    void initPresets (std::vector<FactoryPreset> factory);

    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<PresetManager> presets;

    // A/B compare
    int getABSlot() const { return abSlot; }
    void toggleAB();
    void copyCurrentToOther();

    // Editor size memory
    float uiScale = 1.0f;

    // Standard meters (peak, linear) written by the audio thread
    std::atomic<float> meterIn { 0 }, meterOut { 0 }, meterGr { 0 };

    //==========================================================================
    const juce::String getName() const override { return pluginName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return true; }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Helpers for derived classes
    std::atomic<float>* raw (const juce::String& id) const;
    juce::RangedAudioParameter* param (const juce::String& id) const { return apvts.getParameter (id); }

protected:
    juce::String pluginName;

private:
    int abSlot = 0;
    std::map<juce::String, float> otherSlot;
    juce::String otherSlotName;
};

//==============================================================================
// Parameter-layout helpers
inline juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return juce::NormalisableRange<float> (lo, hi,
        [] (float a, float b, float t) { return a * std::pow (b / a, t); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float def,
                                                      const juce::String& unit,
                                                      std::function<juce::String (float)> toText = nullptr);
std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                        juce::StringArray choices, int def);
std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def);

juce::String formatHz (float hz);
juce::String formatMs (float ms);
juce::String formatDb (float db);
} // namespace era
