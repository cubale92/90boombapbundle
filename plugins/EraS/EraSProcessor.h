#pragma once
#include "EraProcessorBase.h"
#include <juce_dsp/juce_dsp.h>

namespace eras
{
enum Mode { Tape = 0, Tube, Console, Sampler26, Sampler40, VariRate };
inline bool isSampler (int m) { return m >= Sampler26; }

/** Static shaper for the analog modes (normalised to unity small-signal gain). */
float shapeAnalog (int mode, float x, float bias);
/** Converter model for the sampler modes (clip + quantise). */
float shapeSampler (int mode, float x, int bits);

std::vector<era::FactoryPreset> factoryPresets();

class EraSProcessor : public era::EraProcessorBase
{
public:
    EraSProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    era::AnalyzerFifo outFifo;
    double getCurrentSampleRate() const { return sr; }
    std::atomic<float> driveDisplay { 0 };   // last input level after drive, for the curve dot

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters (int mode, float lowCut, float highCut, float tone, float rate);

    std::atomic<float>* pMode, *pDrive, *pBias, *pTone, *pLowCut, *pHighCut, *pBits, *pRate, *pDust, *pMix, *pOut, *pAuto;

    static constexpr int osFactorLog2 = 2;   // 4x
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    double sr = 44100.0, osSr = 176400.0;
    int latency = 0;

    // per-channel state
    struct Chan
    {
        era::Biquad preEmph, deEmph, aaIn[2], aaOut[2];      // oversampled domain
        era::Biquad lowCut, highCut, tiltLo, tiltHi, headBump; // base rate
        era::DcBlocker dc;
        float holdPhase = 1.0f, held = 0.0f;
        // dust
        float clickEnv = 0, clickAmp = 0, hissLp = 0;
        era::Biquad clickHp;
        std::vector<float> dryDelay;
        int dryPos = 0;
    };
    Chan ch[2];
    juce::SmoothedValue<float> drive, mix, outGain, bias;
    float inMs = 0, wetMs = 0, autoG = 1;
    juce::Random rng;
    int lastMode = -1;
    float lastLow = -1, lastHigh = -1, lastTone = -100, lastRate = -1;
    std::vector<float> monoScratch;
    juce::AudioBuffer<float> dryBuf;
};
} // namespace eras
