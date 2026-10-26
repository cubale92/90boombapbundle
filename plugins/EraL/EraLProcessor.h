#pragma once
#include "EraProcessorBase.h"

namespace eral
{
enum Style { Clean = 0, Punchy, Warm, Loud95 };

std::vector<era::FactoryPreset> factoryPresets();

class EraLProcessor : public era::EraProcessorBase
{
public:
    EraLProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;

    era::HistoryFifo history;
    std::atomic<float> lufsMomentary { -100.0f }, lufsShort { -100.0f };
    static constexpr double lookaheadMs = 3.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** O(1) amortised sliding-window minimum. */
    struct SlidingMin
    {
        std::vector<float> val; std::vector<int> idx;
        int head = 0, tail = 0, size = 0, window = 1, t = 0;
        void prepare (int w) { window = w; size = w + 2; val.assign ((size_t) size, 1.0f); idx.assign ((size_t) size, 0); head = tail = t = 0; }
        float push (float v)
        {
            while (head != tail && val[(size_t) ((tail - 1 + size) % size)] >= v) tail = (tail - 1 + size) % size;
            val[(size_t) tail] = v; idx[(size_t) tail] = t; tail = (tail + 1) % size;
            while (idx[(size_t) head] <= t - window) head = (head + 1) % size;
            ++t;
            if (t > (1 << 30)) { for (auto& i : idx) i -= (1 << 29); t -= (1 << 29); }
            return val[(size_t) head];
        }
    };

    struct BoxAverage
    {
        std::vector<float> buf; int pos = 0; double sum = 0; int len = 1;
        void prepare (int l) { len = l; buf.assign ((size_t) l, 1.0f); sum = l; pos = 0; }
        float push (float v) { sum += v - buf[(size_t) pos]; buf[(size_t) pos] = v; pos = (pos + 1) % len; return (float) (sum / len); }
    };

    std::atomic<float>* pGain, *pCeiling, *pRelease, *pStyle, *pLink, *pUnity;

    double sr = 44100.0;
    int la = 1;
    SlidingMin smin[2];
    BoxAverage box[2];
    float env[2] { 1, 1 }, slowGr = 0;
    std::vector<float> delay[2];
    int dpos = 0;
    juce::SmoothedValue<float> gain, ceiling;

    // loudness (BS.1770 K-weighting)
    era::Biquad kShelf[2], kHp[2];
    double blockSum = 0; int blockCount = 0, blockLen = 4800;
    std::array<double, 30> blocks {}; int blockIdx = 0, blocksFilled = 0;
    era::HistoryAccumulator acc;
};
} // namespace eral
