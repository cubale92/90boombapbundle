/* 8cee Era-S for MPC OS: Tape / Tube / Console saturation and SP 26k / Sixty 40k / Vari-Rate sampler grit,
 * with tone tilt, cuts, vinyl dust and auto gain. Same shapers and converter model as the desktop Era-S; the
 * non-linear stage runs at 2x through a linear-phase half-band FIR (the desktop uses JUCE's 4x), 0.7 ms latency. */
#include "era_mpc.h"

using namespace era;

namespace {
enum { Tape = 0, Tube, Console, Sampler26, Sampler40, VariRate };
inline bool isSampler (int m) { return m >= Sampler26; }

inline float baseShape (int mode, float v)
{
    switch (mode)
    {
        case Tape:    return fastTanh (v);
        case Tube:    return v >= 0.0f ? fastTanh (v) : v / (1.0f - 0.6f * v);
        case Console: { const float c = clampf (v, -1.5f, 1.5f); return c - c * c * c / 6.75f; }
        default:      return v;
    }
}

struct Shaper
{
    int mode; float b, fb, norm;
    Shaper() : mode (Tape), b (0), fb (0), norm (1) {}
    void set (int m, float bias)
    {
        mode = m; b = bias * 0.35f; fb = baseShape (m, b);
        const float d = (baseShape (m, b + 1.0e-3f) - baseShape (m, b - 1.0e-3f)) / 2.0e-3f;
        norm = d > 1.0e-4f ? 1.0f / d : 1.0f;
    }
    inline float operator() (float x) const { return (baseShape (mode, x + b) - fb) * norm; }
};

inline float converterClip (float x)
{
    const float a = fabsf (x);
    if (a < 0.95f) return x;
    return copysignf (0.95f + 0.05f * fastTanh ((a - 0.95f) / 0.05f), x);
}
inline float shapeSampler (int mode, float x, int bits)
{
    const float q = (float) (1 << (bits < 2 ? 1 : (bits > 16 ? 15 : bits - 1)));
    x = converterClip (x);
    return mode == Sampler26 ? floorf (x * q) / q : roundf (x * q) / q;   /* the 26k mode truncates like the old 12-bit samplers */
}

/* ---- 2x linear-phase half-band oversampler (Kaiser-windowed, 63 taps: flat to 19 kHz, aliases -52 dB) ---- */
const int kTaps = 63, kCentre = 31;
float gTaps[kTaps];
bool gTapsReady = false;

double besselI0 (double x)
{
    double s = 1, t = 1;
    for (int k = 1; k < 30; k++) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
    return s;
}
void makeTaps()
{
    if (gTapsReady) return;
    const double beta = 8.0;
    double sum = 0;
    for (int n = 0; n < kTaps; n++)
    {
        const int k = n - kCentre;
        double h = k == 0 ? 0.5 : ((k & 1) ? sin (kPi * k / 2.0) / (kPi * k) : 0.0);
        const double r = (double) k / kCentre;
        h *= besselI0 (beta * sqrt (1.0 - r * r)) / besselI0 (beta);
        gTaps[n] = (float) h;
        sum += h;
    }
    for (int n = 0; n < kTaps; n++) gTaps[n] = (float) (gTaps[n] / sum);   /* unity DC gain */
    gTapsReady = true;
}

struct Halfband
{
    float buf[2 * kTaps];   /* doubled ring so a convolution never wraps */
    int pos;
    Halfband() : pos (0) { memset (buf, 0, sizeof buf); }
    inline void push (float x) { pos = pos == 0 ? kTaps - 1 : pos - 1; buf[pos] = buf[pos + kTaps] = x; }
    inline float conv() const
    {
        const float* p = buf + pos;
        float acc = gTaps[kCentre] * p[kCentre];
        for (int n = 0; n < kCentre; n += 2)   /* odd offsets from the centre only: the even ones are zero */
            acc += gTaps[n] * (p[n] + p[kTaps - 1 - n]);
        return acc;
    }
};

const float kLowLo = 10.0f, kLowHi = 500.0f, kHighLo = 1000.0f, kHighHi = 22000.0f, kRateLo = 2000.0f, kRateHi = 48000.0f;
const int kLatency = kCentre;   /* base-rate samples through up + down (31 + 31 at 2x) */
} // namespace

struct EraS
{
    ParamStore params;
    int pMode, pDrive, pBias, pTone, pLowCut, pHighCut, pBits, pRate, pDust, pMix, pOut, pAuto;

    struct Chan
    {
        Halfband up, down;
        Biquad preEmph, deEmph, aaIn[2], aaOut[2];
        Biquad lowCut, highCut, tiltLo, tiltHi, headBump, clickHp;
        DcBlocker dc;
        float holdPhase, held, clickEnv, clickAmp, hissLp;
        float dry[kLatency + 1]; int dryPos;
        Chan() : holdPhase (1), held (0), clickEnv (0), clickAmp (0), hissLp (0), dryPos (0) { memset (dry, 0, sizeof dry); }
    } ch[2];

    Ramp drive, mix, outGain;
    float inMs, wetMs, autoG;
    Rng rng;
    int lastMode; float lastLow, lastHigh, lastTone, lastRate;

    EraS() : inMs (0), wetMs (0), autoG (1), lastMode (-1), lastLow (-1), lastHigh (-1), lastTone (-100), lastRate (-1)
    {
        makeTaps();
        pMode = findParam ("mode"); pDrive = findParam ("drive"); pBias = findParam ("bias"); pTone = findParam ("tone");
        pLowCut = findParam ("lowcut"); pHighCut = findParam ("highcut"); pBits = findParam ("bits"); pRate = findParam ("rate");
        pDust = findParam ("dust"); pMix = findParam ("mix"); pOut = findParam ("output"); pAuto = findParam ("autogain");
        const double osSr = 2.0 * kSampleRate;
        for (int c = 0; c < 2; c++)
        {
            ch[c].clickHp.highPass (1200.0, 0.7, kSampleRate);
            ch[c].preEmph.highShelf (2500.0, 0.7, 5.0, osSr);
            ch[c].deEmph.highShelf (2500.0, 0.7, -5.0, osSr);
            ch[c].headBump.peak (75.0, 0.8, 1.5, kSampleRate);
        }
        rng.s ^= (uint32_t) (uintptr_t) this;   /* different crackle per instance */
        drive.setTime (0.05f); mix.setTime (0.05f); outGain.setTime (0.05f);
        drive.snap (dbToGain (params.get (pDrive))); mix.snap (params.get (pMix) * 0.01f); outGain.snap (dbToGain (params.get (pOut)));
    }

    float lowCutHz() const { return posToLog (params.get (pLowCut), kLowLo, kLowHi); }
    float highCutHz() const { return posToLog (params.get (pHighCut), kHighLo, kHighHi); }
    float rateHz() const { return posToLog (params.get (pRate), kRateLo, kRateHi); }

    void updateFilters (int mode, float lowCut, float highCut, float tone, float rate)
    {
        if (mode == lastMode && lowCut == lastLow && highCut == lastHigh && tone == lastTone && rate == lastRate) return;
        const bool modeChanged = mode != lastMode;
        lastMode = mode; lastLow = lowCut; lastHigh = highCut; lastTone = tone; lastRate = rate;
        const double sr = kSampleRate, osSr = 2.0 * kSampleRate;
        for (int c = 0; c < 2; c++)
        {
            Chan& s = ch[c];
            s.lowCut.highPass (lowCut, 0.707, sr);
            s.highCut.lowPass (highCut, 0.707, sr);
            s.tiltLo.lowShelf (450.0, 0.6, -tone * 0.5, sr);
            s.tiltHi.highShelf (2200.0, 0.6, tone * 0.5, sr);
            double aaIn = osSr * 0.45, aaOut = osSr * 0.45;
            if (mode == Sampler40) { aaIn = 17500.0; aaOut = 18500.0; }
            if (mode == VariRate)  { aaIn = aaOut = rate * 0.45; }
            s.aaIn[0].lowPass (aaIn, 0.54119610, osSr);  s.aaIn[1].lowPass (aaIn, 1.30656296, osSr);
            s.aaOut[0].lowPass (aaOut, 0.54119610, osSr); s.aaOut[1].lowPass (aaOut, 1.30656296, osSr);
            if (modeChanged)
            {
                s.aaIn[0].reset(); s.aaIn[1].reset(); s.aaOut[0].reset(); s.aaOut[1].reset();
                s.preEmph.reset(); s.deEmph.reset();
            }
        }
    }

    /* the oversampled non-linear stage, one 2x-rate sample */
    inline float nonlinear (Chan& s, float v, int mode, const Shaper& shaper, float holdInc, float biasOffset, int bits)
    {
        if (! isSampler (mode))
            return mode == Tape ? s.deEmph.process (shaper (s.preEmph.process (v))) : shaper (v);
        const bool filterIn = mode != Sampler26;   /* no anti-alias filter in the 26k mode: that's the grit */
        if (filterIn) v = s.aaIn[1].process (s.aaIn[0].process (v));
        s.holdPhase += holdInc;
        if (s.holdPhase >= 1.0f) { s.holdPhase -= 1.0f; s.held = shapeSampler (mode, v + biasOffset, bits); }
        v = s.held;
        if (filterIn) v = s.aaOut[1].process (s.aaOut[0].process (v));
        return v;
    }

    void process (const int16_t* in, int16_t* out, int frames)
    {
        const float sr = kSampleRate;
        const int mode = params.opt (pMode);
        const int bits = (int) lroundf (params.get (pBits));
        const float lowCut = lowCutHz(), highCut = highCutHz(), tone = params.get (pTone);
        const bool useLow = lowCut > 10.5f, useHigh = highCut < 21900.0f;
        const float rate = mode == Sampler26 ? 26040.0f : (mode == Sampler40 ? 40000.0f : rateHz());
        const float dust = params.get (pDust) * 0.01f;
        const bool autoGain = params.opt (pAuto) != 0;
        updateFilters (mode, lowCut, highCut, tone, rate);

        drive.set (dbToGain (params.get (pDrive)));
        mix.set (params.get (pMix) * 0.01f);
        outGain.set (dbToGain (params.get (pOut)));

        Shaper shaper;
        shaper.set (mode, params.get (pBias) * 0.01f);
        const float holdInc = rate / (2.0f * sr);
        const float biasOffset = params.get (pBias) * 0.001f;
        const float msCoef = expf (-1.0f / (0.3f * sr));
        const float agCoef = expf (-1.0f / (0.05f * sr));
        const float clickDecay = expf (-1.0f / (0.00035f * sr));
        const float clickProb = (dust * dust * 38.0f + dust * 1.5f) / sr;

        for (int i = 0; i < frames; i++)
        {
            const float d = drive.next();
            float dryIn[2], wet[2];
            float inSq = 0, wetSq = 0;
            for (int c = 0; c < 2; c++)
            {
                Chan& s = ch[c];
                const float x = s16ToF (in[2 * i + c]);

                /* up 2x: the zero-stuffed stream through the half-band (gain 2), shape, then back down */
                s.up.push (x * d * 2.0f);
                const float u0 = s.up.conv();
                s.up.push (0.0f);
                const float u1 = s.up.conv();
                s.down.push (nonlinear (s, u0, mode, shaper, holdInc, biasOffset, bits));
                s.down.push (nonlinear (s, u1, mode, shaper, holdInc, biasOffset, bits));
                float v = s.down.conv();

                v = s.dc.process (v);
                if (mode == Tape) v = s.headBump.process (v);
                if (useLow) v = s.lowCut.process (v);
                if (useHigh) v = s.highCut.process (v);
                if (fabsf (tone) > 0.01f) v = s.tiltHi.process (s.tiltLo.process (v));
                wet[c] = v;

                /* dry, delayed to line up with the oversampled path */
                s.dry[s.dryPos] = x;
                s.dryPos = (s.dryPos + 1) % (kLatency + 1);
                dryIn[c] = s.dry[s.dryPos];
                inSq += dryIn[c] * dryIn[c];
                wetSq += v * v;
            }

            /* auto gain: match the wet RMS to the dry RMS (slow, linked) */
            inMs = inSq + msCoef * (inMs - inSq);
            wetMs = wetSq + msCoef * (wetMs - wetSq);
            float target = 1.0f;
            if (autoGain && inMs > 1.0e-9f) target = clampf (sqrtf ((inMs + 1.0e-12f) / (wetMs + 1.0e-12f)), 0.0631f, 4.0f);
            autoG = target + agCoef * (autoG - target);

            const float m = mix.next(), og = outGain.next();
            for (int c = 0; c < 2; c++)
            {
                Chan& s = ch[c];
                float w = wet[c] * autoG;
                if (dust > 0.0f)
                {
                    /* vinyl: hiss + crackle + the odd pop */
                    const float white = rng.uni() * 2.0f - 1.0f;
                    s.hissLp += 0.35f * (white - s.hissLp);
                    w += s.hissLp * 0.0035f * dust;
                    if (rng.uni() < clickProb)
                    {
                        const float r = rng.uni();
                        s.clickAmp = ((rng.next() & 1) ? 1.0f : -1.0f) * (0.02f + 0.3f * r * r * r) * (0.4f + 0.6f * dust);
                        s.clickEnv = 1.0f;
                    }
                    if (s.clickEnv > 1.0e-4f)
                    {
                        w += s.clickHp.process (s.clickAmp * s.clickEnv * (0.6f + 0.4f * white));
                        s.clickEnv *= clickDecay;
                    }
                }
                out[2 * i + c] = fToS16 ((dryIn[c] + m * (w - dryIn[c])) * og);
            }
        }
    }

    int display (int idx, char* b, int n)
    {
        const float v = params.get (idx);
        if (idx == pLowCut) { const float h = lowCutHz(); return h <= 10.5f ? snprintf (b, (size_t) n, "Off") : fmtHz (b, n, h); }
        if (idx == pHighCut) { const float h = highCutHz(); return h >= 21900.0f ? snprintf (b, (size_t) n, "Off") : fmtHz (b, n, h); }
        if (idx == pRate)
        {
            const int mode = params.opt (pMode);
            return fmtHz (b, n, mode == Sampler26 ? 26040.0f : (mode == Sampler40 ? 40000.0f : rateHz()));
        }
        if (idx == pDrive) return snprintf (b, (size_t) n, "+%.1f dB", v);
        if (idx == pOut) return fmtDb (b, n, v);
        if (idx == pBias || idx == pMix) return snprintf (b, (size_t) n, "%.0f %%", v);
        if (idx == pDust) return v < 0.05f ? snprintf (b, (size_t) n, "Off") : snprintf (b, (size_t) n, "%.0f %%", v);
        if (idx == pBits) return snprintf (b, (size_t) n, "%d bit", (int) lroundf (v));
        if (idx == pTone)
            return fabsf (v) < 0.05f ? snprintf (b, (size_t) n, "Flat") : snprintf (b, (size_t) n, "%s %.1f", v < 0 ? "Dark" : "Bright", fabsf (v));
        return 0;
    }
};

extern "C" const mpc_engine_t* mpc_engine (void) { return EngineGlue<EraS>::get(); }
