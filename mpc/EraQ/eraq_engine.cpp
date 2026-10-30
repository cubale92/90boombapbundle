/* 8cee Era-Q for MPC OS: 8-band EQ with per-band Stereo/Mid/Side and an analog Color stage.
 * Same filters as the desktop Era-Q (plugins/EraQ/EraQProcessor.cpp); the curve display is not available on MPC. */
#include "era_mpc.h"

using namespace era;

namespace {
enum { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };
enum { Stereo = 0, Mid, Side };
const int kBands = 8;
const float kFreqLo = 20.0f, kFreqHi = 20000.0f, kQLo = 0.1f, kQHi = 30.0f;

struct Band
{
    int pOn, pType, pFreq, pGain, pQ, pSlope, pPlace;
    int type, slope, place, nStages;
    float freq, gain, q;        /* smoothed values the coefficients were built from */
    float mix, mixTarget;
    SvfCoeffs st[4];
    SvfState s[2][4];
    Band() : type (-1), slope (-1), place (-1), nStages (0), freq (1000), gain (0), q (0.707f), mix (0), mixTarget (0) {}
    void resetState() { for (int c = 0; c < 2; c++) for (int i = 0; i < 4; i++) s[c][i].reset(); }
};

int buildStages (int type, int slope, float freq, float gain, float q, SvfCoeffs* out)
{
    const double sr = kSampleRate;
    switch (type)
    {
        case Bell:      out[0] = SvfCoeffs::make (SvfBell, freq, q, gain, sr); return 1;
        case LowShelf:  out[0] = SvfCoeffs::make (SvfLowShelf, freq, q, gain, sr); return 1;
        case HighShelf: out[0] = SvfCoeffs::make (SvfHighShelf, freq, q, gain, sr); return 1;
        case Notch:     out[0] = SvfCoeffs::make (SvfNotch, freq, q, 0, sr); return 1;
        case LowCut: case HighCut:
        {
            static const double q2[] = { 0.54119610, 1.30656296 };
            static const double q4[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
            const SvfType t = type == LowCut ? SvfHighPass : SvfLowPass;
            const double res = q / 0.70710678;   /* Q 0.707 = pure Butterworth; higher adds a resonant bump */
            const int n = slope == 0 ? 1 : (slope == 1 ? 2 : 4);
            for (int i = 0; i < n; i++)
            {
                double qq = n == 1 ? q : (n == 2 ? q2[i] : q4[i]);
                if (n > 1 && i == n - 1) qq *= res;
                out[i] = SvfCoeffs::make (t, freq, qq, 0, sr);
            }
            return n;
        }
    }
    return 0;
}
} // namespace

struct EraQ
{
    ParamStore params;
    Band band[kBands];
    int pColor, pOutput;
    Ramp outGain, color;
    DcBlocker dc[2];
    unsigned seenRev;

    EraQ() : seenRev (0)
    {
        char k[32];
        for (int b = 0; b < kBands; b++)
        {
            Band& B = band[b];
#define P(field, name) snprintf (k, sizeof k, "b%d_" name, b + 1); B.field = findParam (k);
            P (pOn, "on") P (pType, "type") P (pFreq, "freq") P (pGain, "gain") P (pQ, "q") P (pSlope, "slope") P (pPlace, "place")
#undef P
        }
        pColor = findParam ("color");
        pOutput = findParam ("output");
        outGain.setTime (0.03f); color.setTime (0.05f);
        outGain.snap (dbToGain (params.get (pOutput)));
        color.snap (params.get (pColor) * 0.01f);
        for (int b = 0; b < kBands; b++) syncBand (band[b], true);
    }

    float targetFreq (const Band& B) const { return posToLog (params.get (B.pFreq), kFreqLo, kFreqHi); }
    float targetQ (const Band& B) const { return posToLog (params.get (B.pQ), kQLo, kQHi); }

    /* Pick up shape changes immediately (with a state reset), values via smoothing. */
    void syncBand (Band& B, bool snap)
    {
        const int type = params.opt (B.pType), slope = params.opt (B.pSlope), place = params.opt (B.pPlace);
        B.mixTarget = params.opt (B.pOn) ? 1.0f : 0.0f;
        if (snap)
        {
            B.freq = targetFreq (B); B.gain = params.get (B.pGain); B.q = targetQ (B);
            B.mix = B.mixTarget;
        }
        if (snap || type != B.type || slope != B.slope || place != B.place)
        {
            if (type != B.type || slope != B.slope) B.resetState();
            B.type = type; B.slope = slope; B.place = place;
            B.nStages = buildStages (type, slope, B.freq, B.gain, B.q, B.st);
        }
    }

    void process (const int16_t* in, int16_t* out, int frames)
    {
        if (params.rev != seenRev)
        {
            seenRev = params.rev;
            for (int b = 0; b < kBands; b++) syncBand (band[b], false);
            outGain.set (dbToGain (params.get (pOutput)));
            color.set (params.get (pColor) * 0.01f);
        }

        float L[128], R[128];
        for (int i = 0; i < frames; i++) { L[i] = s16ToF (in[2 * i]); R[i] = s16ToF (in[2 * i + 1]); }

        const int sub = 32;
        for (int start = 0; start < frames; start += sub)
        {
            const int len = frames - start < sub ? frames - start : sub;
            float* l = L + start;
            float* r = R + start;

            for (int b = 0; b < kBands; b++)
            {
                Band& B = band[b];
                if (B.mix <= 0.0f && B.mixTarget <= 0.0f) continue;

                /* glide freq (in log), gain and Q toward the knob, ~30 ms */
                const float tf = targetFreq (B), tg = params.get (B.pGain), tq = targetQ (B);
                if (fabsf (tf - B.freq) > B.freq * 1.0e-4f || fabsf (tg - B.gain) > 1.0e-3f || fabsf (tq - B.q) > B.q * 1.0e-4f)
                {
                    const float a = 0.55f;   /* per 32 samples */
                    B.freq = expf (logf (B.freq) + a * (logf (tf) - logf (B.freq)));
                    B.gain += a * (tg - B.gain);
                    B.q = expf (logf (B.q) + a * (logf (tq) - logf (B.q)));
                    B.nStages = buildStages (B.type, B.slope, B.freq, B.gain, B.q, B.st);
                }

                const int ns = B.nStages;
                for (int i = 0; i < len; i++)
                {
                    /* ~15 ms linear fade when a band is switched on or off */
                    if (B.mix != B.mixTarget)
                        B.mix = B.mixTarget > B.mix ? fminf (B.mixTarget, B.mix + 1.0f / 660.0f)
                                                    : fmaxf (B.mixTarget, B.mix - 1.0f / 660.0f);
                    const float m = B.mix;
                    if (B.place == Stereo)
                    {
                        float yl = l[i], yr = r[i];
                        for (int s = 0; s < ns; s++) { yl = B.s[0][s].process (yl, B.st[s]); yr = B.s[1][s].process (yr, B.st[s]); }
                        l[i] += m * (yl - l[i]);
                        r[i] += m * (yr - r[i]);
                    }
                    else
                    {
                        float mid = 0.5f * (l[i] + r[i]), side = 0.5f * (l[i] - r[i]);
                        const int c = B.place == Mid ? 0 : 1;
                        float y = c == 0 ? mid : side;
                        for (int s = 0; s < ns; s++) y = B.s[c][s].process (y, B.st[s]);
                        if (c == 0) mid += m * (y - mid); else side += m * (y - side);
                        l[i] = mid + side;
                        r[i] = mid - side;
                    }
                }
                if (B.mix <= 0.0f && B.mixTarget <= 0.0f) B.resetState();
            }
        }

        /* Color: unity-slope asymmetric saturation (2nd + 3rd harmonics), then output gain */
        for (int i = 0; i < frames; i++)
        {
            const float c = color.next(), g = outGain.next();
            float x[2] = { L[i], R[i] };
            if (c > 0.0005f)
            {
                const float drive = 1.0f + 2.5f * c, bias = 0.12f * c;
                const float tb = fastTanh (drive * bias);
                const float slope0 = drive * (1.0f - tb * tb);
                for (int ch = 0; ch < 2; ch++)
                {
                    const float y = (fastTanh (drive * (x[ch] + bias)) - tb) / slope0;
                    x[ch] = dc[ch].process (x[ch] + c * (y - x[ch]));
                }
            }
            out[2 * i] = fToS16 (x[0] * g);
            out[2 * i + 1] = fToS16 (x[1] * g);
        }
    }

    int display (int idx, char* b, int n)
    {
        for (int k = 0; k < kBands; k++)
        {
            const Band& B = band[k];
            if (idx == B.pFreq) return fmtHz (b, n, targetFreq (B));
            if (idx == B.pQ) { const float q = targetQ (B); return snprintf (b, (size_t) n, q < 10 ? "%.2f" : "%.1f", q); }
            if (idx == B.pGain) return fmtDb (b, n, params.get (idx));
        }
        if (idx == pOutput) return fmtDb (b, n, params.get (idx));
        if (idx == pColor) { const float c = params.get (idx); return c < 0.05f ? snprintf (b, (size_t) n, "Clean") : snprintf (b, (size_t) n, "%.0f %%", c); }
        return 0;
    }
};

extern "C" const mpc_engine_t* mpc_engine (void) { return EngineGlue<EraQ>::get(); }
