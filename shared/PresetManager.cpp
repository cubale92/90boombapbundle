#include "PresetManager.h"

namespace era
{
static const char* presetExtension = ".eraprst";

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s, const juce::String& name, std::vector<FactoryPreset> f)
    : apvts (s), pluginName (name), factory (std::move (f))
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.addParameterListener (rp->getParameterID(), this);

    rescanUserPresets();
}

PresetManager::~PresetManager()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            apvts.removeParameterListener (rp->getParameterID(), this);
}

juce::File PresetManager::getUserPresetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("8cee").getChildFile ("Golden Era").getChildFile (pluginName).getChildFile ("Presets");
}

void PresetManager::rescanUserPresets()
{
    entries.clear();
    for (int i = 0; i < (int) factory.size(); ++i)
        entries.push_back ({ factory[(size_t) i].category, factory[(size_t) i].name, true, i, {} });

    auto folder = getUserPresetFolder();
    if (folder.isDirectory())
    {
        auto files = folder.findChildFiles (juce::File::findFiles, false, juce::String ("*") + presetExtension);
        files.sort();
        for (auto& f : files)
            entries.push_back ({ "User", f.getFileNameWithoutExtension(), false, -1, f });
    }
    sendChangeMessage();
}

juce::String PresetManager::getCurrentName() const
{
    if (customName.isNotEmpty())
        return customName;
    if (juce::isPositiveAndBelow (currentIndex, (int) entries.size()))
        return entries[(size_t) currentIndex].name;
    return "Init";
}

std::map<juce::String, float> PresetManager::captureNormalised() const
{
    std::map<juce::String, float> m;
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            m[rp->getParameterID()] = rp->getValue();
    return m;
}

void PresetManager::applyNormalised (const std::map<juce::String, float>& values, bool resetOthers)
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;

        auto it = values.find (rp->getParameterID());
        float v;
        if (it != values.end())  v = it->second;
        else if (resetOthers)    v = rp->getDefaultValue();
        else                     continue;

        if (std::abs (rp->getValue() - v) > 1.0e-7f)
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (v);
            rp->endChangeGesture();
        }
    }
}

void PresetManager::applyFactory (const FactoryPreset& preset)
{
    std::map<juce::String, float> norm;
    for (auto& [id, value] : preset.values)
    {
        if (auto* rp = apvts.getParameter (id))
            norm[id] = rp->convertTo0to1 (value);
        else
            jassertfalse; // typo in a factory preset parameter ID
    }
    applyNormalised (norm, true);
}

void PresetManager::load (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) entries.size()))
        return;

    loading = true;
    const auto& e = entries[(size_t) index];
    if (e.isFactory)
    {
        applyFactory (factory[(size_t) e.factoryIndex]);
    }
    else if (auto xml = juce::XmlDocument::parse (e.file))
    {
        std::map<juce::String, float> norm;
        for (auto* child : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = child->getStringAttribute ("id");
            if (auto* rp = apvts.getParameter (id))
                norm[id] = rp->convertTo0to1 ((float) child->getDoubleAttribute ("value"));
        }
        applyNormalised (norm, true);
    }
    loading = false;
    finishLoad (index);
}

void PresetManager::finishLoad (int index)
{
    currentIndex = index;
    customName = {};
    modified = false;
    apvts.state.setProperty ("presetName", getCurrentName(), nullptr);
    sendChangeMessage();
}

void PresetManager::loadNext()
{
    if (! entries.empty())
        load ((currentIndex + 1) % (int) entries.size());
}

void PresetManager::loadPrevious()
{
    if (! entries.empty())
        load ((currentIndex - 1 + (int) entries.size()) % (int) entries.size());
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    auto folder = getUserPresetFolder();
    if (! folder.createDirectory())
        return false;

    juce::XmlElement xml ("ERA_PRESET");
    xml.setAttribute ("plugin", pluginName);
    xml.setAttribute ("version", ERA_VERSION_STRING);
    for (auto* p : apvts.processor.getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
        }
    }

    auto file = folder.getChildFile (name + presetExtension);
    if (! xml.writeTo (file))
        return false;

    rescanUserPresets();
    for (int i = 0; i < (int) entries.size(); ++i)
        if (! entries[(size_t) i].isFactory && entries[(size_t) i].file == file)
            finishLoad (i);
    return true;
}

void PresetManager::restoreLabel (const juce::String& name, bool wasModified)
{
    currentIndex = -1;
    for (int i = 0; i < (int) entries.size(); ++i)
        if (entries[(size_t) i].name == name) { currentIndex = i; break; }

    customName = currentIndex < 0 ? name : juce::String();
    modified = wasModified;
    sendChangeMessage();
}

void PresetManager::parameterChanged (const juce::String&, float)
{
    if (! loading)
        modified = true;
}
} // namespace era
