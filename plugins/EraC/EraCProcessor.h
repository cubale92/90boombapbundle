#pragma once
#include "EraProcessorBase.h"

namespace erac
{
enum Style { Clean = 0, Punch, Opto, Slam };

/** Static gain computer: returns gain reduction in dB (>= 0). */
inline float computeGr (float levelDb, float thr, float ratio, float knee)
{
    if (ratio <= 1.0001f) return 0.0f;
    const float over = levelDb - thr;
    const float slope = 1.0f - 1.0f / ratio;
    if (knee > 0.01f && std::abs (over) * 2.0f <= knee)
    {
        const float t = over + knee * 0.5f;
        return slope * t * t / (2.0f * knee);
    }
    return over > 0.0f ? slope * over : 0.0f;
}

/** Opto mode widens the knee for that smooth, program-dependent leveling. */
inline float effectiveKnee (int style, float knee) { return style == Opto ? std::max (knee, 12.0f) : knee; }

std::vector<era::FactoryPreset> factoryPresets();

class EraCProcessor : public era::EraProcessorBase
{
public:
    EraCProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    float getAutoMakeup() const;
    era::HistoryFifo history;
    std::atomic<float> detectorDb { -100.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float>* pStyle, *pThr, *pRatio, *pAtt, *pRel, *pAutoRel, *pKnee, *pMakeup, *pAutoMk, *pMix, *pScHpf;

    double sr = 44100.0;
    juce::SmoothedValue<float> thr, makeup, mix;
    era::Biquad scHp[2][2];      // 2nd-order x2 = 24 dB/oct side-chain filter
    float lastHpf = -1;
    float ms = 0, env = 0, slowGr = 0, fbGain = 1;
    era::DcBlocker dc[2];
    era::HistoryAccumulator acc;
};
} // namespace erac
