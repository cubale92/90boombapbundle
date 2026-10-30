/* 8cee Era-L for MPC OS: look-ahead limiter with Clean / Punchy / Warm / Loud '95 styles, ceiling, stereo link and
 * unity-gain monitoring. Same gain path as the desktop Era-L (3 ms look-ahead, sliding minimum, box smoothing);
 * the loudness meter is desktop-only. */
#include "era_mpc.h"

using namespace era;

namespace {
enum { Clean = 0, Punchy, Warm, Loud95 };
const int kLook = 132;                 /* 3 ms at 44.1 kHz */
const float kRelLo = 1.0f, kRelHi = 1000.0f;

/* O(1) amortised sliding-window minimum over the last kLook + 1 values */
struct SlidingMin
{
    enum { W = kLook + 1, S = W + 2 };
    float val[S]; int idx[S]; int head, tail, t;
    SlidingMin() : head (0), tail (0), t (0) { for (int i = 0; i < S; i++) { val[i] = 1; idx[i] = 0; } }
    inline float push (float v)
    {
        while (head != tail && val[(tail - 1 + S) % S] >= v) tail = (tail - 1 + S) % S;
        val[tail] = v; idx[tail] = t; tail = (tail + 1) % S;
        while (idx[head] <= t - W) head = (head + 1) % S;
        if (++t > (1 << 30)) { for (int i = 0; i < S; i++) idx[i] -= (1 << 29); t -= (1 << 29); }
        return val[head];
    }
};

struct BoxAverage
{
    float buf[kLook]; int pos; double sum;
    BoxAverage() : pos (0), sum (kLook) { for (int i = 0; i < kLook; i++) buf[i] = 1; }
    inline float push (float v) { sum += v - buf[pos]; buf[pos] = v; pos = (pos + 1) % kLook; return (float) (sum / kLook); }
};
} // namespace

struct EraL
{
    ParamStore params;
    int pGain, pCeiling, pRelease, pStyle, pLink, pUnity;
    SlidingMin smin[2];
    BoxAverage box[2];
    float env[2], slowGr;
    float delay[2][kLook];
    int dpos;
    Ramp gain, ceiling;

    EraL() : slowGr (0), dpos (0)
    {
        pGain = findParam ("gain"); pCeiling = findParam ("ceiling"); pRelease = findParam ("release");
        pStyle = findParam ("style"); pLink = findParam ("link"); pUnity = findParam ("unity");
        env[0] = env[1] = 1.0f;
        memset (delay, 0, sizeof delay);
        gain.setTime (0.05f); ceiling.setTime (0.05f);
        gain.snap (dbToGain (params.get (pGain))); ceiling.snap (dbToGain (params.get (pCeiling)));
    }

    float releaseMs() const { return posToLog (params.get (pRelease), kRelLo, kRelHi); }

    void process (const int16_t* in, int16_t* out, int frames)
    {
        const float sr = kSampleRate;
        const int style = params.opt (pStyle);
        const float link = params.get (pLink) * 0.01f;
        const bool unity = params.opt (pUnity) != 0;
        float relMs = releaseMs();
        if (style == Punchy) relMs *= 0.5f;
        if (style == Loud95) relMs *= 0.7f;
        const float slowCoef = expf (-1.0f / (0.5f * sr));
        float relCoef = expf (-1.0f / (relMs * 0.001f * sr));
        gain.set (dbToGain (params.get (pGain)));
        ceiling.set (dbToGain (params.get (pCeiling)));

        for (int i = 0; i < frames; i++)
        {
            const float g = gain.next(), c = ceiling.next();
            float x[2], req[2];
            for (int ch = 0; ch < 2; ch++)
            {
                float v = s16ToF (in[2 * i + ch]) * g;
                if (style == Warm)
                {
                    /* soft knee about +1 dB over the ceiling: rounds the peaks like tape */
                    const float lvl = c * 1.12f, k = 0.55f * lvl, a = fabsf (v);
                    if (a > k) v = copysignf (k + (lvl - k) * fastTanh ((a - k) / (lvl - k)), v);
                }
                else if (style == Loud95)
                {
                    /* hard converter clip +2 dB over the ceiling: the brickwall crunch of mid-90s singles */
                    const float lvl = c * 1.26f;
                    v = clampf (v, -lvl, lvl);
                }
                x[ch] = v;
                const float a = fabsf (v);
                req[ch] = a > c ? c / a : 1.0f;
            }

            if (style == Clean && (i & 15) == 0)   /* program-dependent release, every 16 samples */
                relCoef = expf (-1.0f / (relMs * (0.6f + 0.8f * clampf (slowGr / 6.0f, 0.0f, 1.0f)) * 0.001f * sr));

            const float linked = fminf (req[0], req[1]);
            float gr = 1.0f;
            for (int ch = 0; ch < 2; ch++)
            {
                const float r = linked * link + req[ch] * (1.0f - link);
                const float m = smin[ch].push (r);
                env[ch] = m < env[ch] ? m : m + relCoef * (env[ch] - m);
                const float gg = box[ch].push (env[ch]);
                const float delayed = delay[ch][dpos];
                delay[ch][dpos] = x[ch];
                float o = clampf (delayed * gg, -c, c);
                if (unity) o /= g;
                out[2 * i + ch] = fToS16 (o);
                gr = fminf (gr, gg);
            }
            dpos = (dpos + 1) % kLook;
            const float grDb = gr < 0.999999f ? -8.6858896f * logf (gr) : 0.0f;
            slowGr = grDb + slowCoef * (slowGr - grDb);
        }
    }

    int display (int idx, char* b, int n)
    {
        const float v = params.get (idx);
        if (idx == pRelease) return fmtMs (b, n, releaseMs());
        if (idx == pGain) return snprintf (b, (size_t) n, "+%.1f dB", v);
        if (idx == pCeiling) return snprintf (b, (size_t) n, "%.1f dB", v);
        if (idx == pLink) return snprintf (b, (size_t) n, "%.0f %%", v);
        return 0;
    }
};

extern "C" const mpc_engine_t* mpc_engine (void) { return EngineGlue<EraL>::get(); }
