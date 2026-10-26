#include "EraProcessorBase.h"

namespace era
{
EraProcessorBase::EraProcessorBase (const juce::String& name, juce::AudioProcessorValueTreeState::ParameterLayout layout)
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", std::move (layout)),
      pluginName (name)
{
}

EraProcessorBase::~EraProcessorBase() = default;

void EraProcessorBase::initPresets (std::vector<FactoryPreset> factory)
{
    presets = std::make_unique<PresetManager> (apvts, pluginName, std::move (factory));
}

std::atomic<float>* EraProcessorBase::raw (const juce::String& id) const
{
    auto* p = apvts.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

bool EraProcessorBase::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void EraProcessorBase::toggleAB()
{
    auto current = presets->captureNormalised();
    const auto currentName = presets->getCurrentName();
    const bool currentModified = presets->isModified();

    if (otherSlot.empty())
        otherSlot = current; // first switch: B starts as a copy of A

    presets->applyNormalised (otherSlot, false);
    presets->restoreLabel (otherSlotName.isNotEmpty() ? otherSlotName : currentName, currentModified);

    otherSlot = std::move (current);
    otherSlotName = currentName;
    abSlot = 1 - abSlot;
}

void EraProcessorBase::copyCurrentToOther()
{
    otherSlot = presets->captureNormalised();
    otherSlotName = presets->getCurrentName();
}

void EraProcessorBase::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presets->getCurrentName(), nullptr);
    state.setProperty ("presetModified", presets->isModified(), nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void EraProcessorBase::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (tree);
    uiScale = juce::jlimit (0.6f, 2.0f, (float) tree.getProperty ("uiScale", 1.0f));
    presets->restoreLabel (tree.getProperty ("presetName", "Init").toString(),
                           (bool) tree.getProperty ("presetModified", false));
}

//==============================================================================
std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id, const juce::String& name,
                                                      juce::NormalisableRange<float> range, float def,
                                                      const juce::String& unit,
                                                      std::function<juce::String (float)> toText)
{
    auto attrs = juce::AudioParameterFloatAttributes().withLabel (unit);
    if (toText)
        attrs = attrs.withStringFromValueFunction ([toText] (float v, int) { return toText (v); });
    return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attrs);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id, const juce::String& name,
                                                        juce::StringArray choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, choices, def);
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

juce::String formatHz (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz";
    return juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz";
}

juce::String formatMs (float ms)
{
    if (ms >= 1000.0f) return juce::String (ms / 1000.0f, 2) + " s";
    if (ms < 1.0f)     return juce::String (ms, 2) + " ms";
    if (ms < 10.0f)    return juce::String (ms, 1) + " ms";
    return juce::String (juce::roundToInt (ms)) + " ms";
}

juce::String formatDb (float db)
{
    return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
}
} // namespace era
