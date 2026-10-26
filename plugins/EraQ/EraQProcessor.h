#pragma once
#include "EraProcessorBase.h"

namespace eraq
{
constexpr int numBands = 8;
enum BandType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };
enum Placement { Stereo = 0, Mid, Side };

inline juce::String pid (int band, const char* name) { return "b" + juce::String (band + 1) + "_" + name; }

struct BandSetting
{
    bool on = false;
    int type = Bell;
    float freq = 1000, gain = 0, q = 0.707f;
    int slope = 0, place = Stereo;
};

/** Builds the cascaded SVF stages for one band. Shared by DSP and display. */
int makeBandStages (const BandSetting&, double sampleRate, era::SvfCoeffs* out4);
inline bool typeUsesGain (int t) { return t == Bell || t == LowShelf || t == HighShelf; }
inline bool typeUsesSlope (int t) { return t == LowCut || t == HighCut; }

std::vector<era::FactoryPreset> factoryPresets();

class EraQProcessor : public era::EraProcessorBase
{
public:
    EraQProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    BandSetting getBandSetting (int band) const;
    double getCurrentSampleRate() const { return currentSr; }

    era::AnalyzerFifo preFifo, postFifo;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct BandParams { std::atomic<float>* on, *type, *freq, *gain, *q, *slope, *place; };
    std::array<BandParams, numBands> bp {};
    std::atomic<float>* outputParam = nullptr, *colorParam = nullptr;

    struct BandDsp
    {
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> freq, q;
        juce::SmoothedValue<float> gain, mix;
        int type = -1, slope = -1, place = -1;
        era::SvfCoeffs stages[4];
        int numStages = 0;
        era::SvfState st[2][4];
        void resetState() { for (auto& c : st) for (auto& s : c) s.reset(); }
    };
    std::array<BandDsp, numBands> bands;
    juce::SmoothedValue<float> outGain, color;
    era::DcBlocker dc[2];
    std::vector<float> monoScratch;
    double currentSr = 44100.0;
};
} // namespace eraq
