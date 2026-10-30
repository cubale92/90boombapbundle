/* 8cee Era-C for MPC OS: compressor with Clean / Punch / Opto / Slam styles, auto release, auto makeup,
 * parallel mix and a side-chain high-pass. Same detector and gain computer as the desktop Era-C. */
#include "era_mpc.h"

using namespace era;

namespace {
enum { Clean = 0, Punch, Opto, Slam };

float computeGr (float levelDb, float thr, float ratio, float knee)
{
    if (ratio <= 1.0001f) return 0.0f;
    const float over = levelDb - thr, slope = 1.0f - 1.0f / ratio;
    if (knee > 0.01f && fabsf (over) * 2.0f <= knee)
    {
        const float t = over + knee * 0.5f;
        return slope * t * t / (2.0f * knee);
    }
    return over > 0.0f ? slope * over : 0.0f;
}
inline float effectiveKnee (int style, float knee) { return style == Opto ? fmaxf (knee, 12.0f) : knee; }
inline float fastDbToGain (float db) { return expf (db * 0.11512925f); }
inline float fastGainToDb (float g) { return g > 1.0e-5f ? 8.6858896f * logf (g) : -100.0f; }

const float kAttLo = 0.05f, kAttHi = 250.0f, kRelLo = 5.0f, kRelHi = 2500.0f, kHpfLo = 20.0f, kHpfHi = 500.0f;
} // namespace

struct EraC
{
    ParamStore params;
    int pStyle, pThr, pRatio, pAtt, pRel, pAutoRel, pKnee, pMakeup, pAutoMk, pMix, pScHpf;
    Ramp thr, makeup, mix;
    Biquad scHp[2][2];
    float lastHpf, ms, env, slowGr, fbGain;
    DcBlocker dc[2];

    EraC() : lastHpf (-1), ms (0), env (0), slowGr (0), fbGain (1)
    {
        pStyle = findParam ("style"); pThr = findParam ("threshold"); pRatio = findParam ("ratio");
        pAtt = findParam ("attack"); pRel = findParam ("release"); pAutoRel = findParam ("autorel");
        pKnee = findParam ("knee"); pMakeup = findParam ("makeup"); pAutoMk = findParam ("automakeup");
        pMix = findParam ("mix"); pScHpf = findParam ("schpf");
        thr.setTime (0.05f); makeup.setTime (0.05f); mix.setTime (0.05f);
        thr.snap (params.get (pThr)); makeup.snap (params.get (pMakeup) + autoMakeup()); mix.snap (params.get (pMix) * 0.01f);
    }

    float attackMs() const { return posToLog (params.get (pAtt), kAttLo, kAttHi); }
    float releaseMs() const { return posToLog (params.get (pRel), kRelLo, kRelHi); }
    float hpfHz() const { return posToLog (params.get (pScHpf), kHpfLo, kHpfHi); }

    float autoMakeup() const
    {
        if (! params.opt (pAutoMk)) return 0.0f;
        const float gr0 = computeGr (0.0f, params.get (pThr), params.get (pRatio), effectiveKnee (params.opt (pStyle), params.get (pKnee)));
        return fminf (24.0f, gr0 * 0.5f);
    }

    void process (const int16_t* in, int16_t* out, int frames)
    {
        const float sr = kSampleRate;
        const int style = params.opt (pStyle);
        const float ratio = params.get (pRatio);
        const float knee = effectiveKnee (style, params.get (pKnee));
        const bool autoRel = params.opt (pAutoRel) || style == Opto;
        const float hpf = hpfHz();
        const bool useHpf = hpf > 20.5f;
        if (useHpf && fabsf (hpf - lastHpf) > 0.01f)
        {
            for (int c = 0; c < 2; c++) { scHp[c][0].highPass (hpf, 0.54119610, sr); scHp[c][1].highPass (hpf, 1.30656296, sr); }
            lastHpf = hpf;
        }

        float attMs = attackMs(), relMs = releaseMs(), rmsMs = 0.0f;
        switch (style)
        {
            case Punch: rmsMs = 4.0f; break;
            case Opto:  rmsMs = 20.0f; attMs = fmaxf (attMs, 8.0f) * 1.5f; break;
            case Slam:  attMs *= 0.5f; break;
            default: break;
        }
        const float attCoef = expf (-1.0f / (fmaxf (0.02f, attMs) * 0.001f * sr));
        const float msCoef = rmsMs > 0 ? expf (-1.0f / (rmsMs * 0.001f * sr)) : 0.0f;
        const float slowCoef = expf (-1.0f / (0.8f * sr));
        float relCoef = expf (-1.0f / (relMs * 0.001f * sr));

        thr.set (params.get (pThr));
        makeup.set (params.get (pMakeup) + autoMakeup());
        mix.set (params.get (pMix) * 0.01f);

        for (int i = 0; i < frames; i++)
        {
            const float xl = s16ToF (in[2 * i]), xr = s16ToF (in[2 * i + 1]);

            float sl = xl, sr2 = xr;
            if (useHpf)
            {
                sl = scHp[0][1].process (scHp[0][0].process (sl));
                sr2 = scHp[1][1].process (scHp[1][0].process (sr2));
            }
            if (style == Punch) { sl *= fbGain; sr2 *= fbGain; }   /* feedback topology, bus-comp style */

            float det = fmaxf (fabsf (sl), fabsf (sr2));
            if (rmsMs > 0)
            {
                ms = det * det + msCoef * (ms - det * det);
                det = sqrtf (2.0f * ms);
            }
            const float levelDb = fastGainToDb (det);

            float target = computeGr (levelDb, thr.next(), ratio, knee);
            if (style == Punch) target = fminf (40.0f, target * ratio);

            slowGr = target + slowCoef * (slowGr - target);
            if (autoRel && (i & 15) == 0)   /* program-dependent release, updated every 16 samples */
            {
                const float rel = relMs * (0.3f + 1.7f * clampf (slowGr / 8.0f, 0.0f, 1.0f));
                relCoef = expf (-1.0f / (rel * 0.001f * sr));
            }
            env = target > env ? target + attCoef * (env - target) : target + relCoef * (env - target);

            const float grGain = fastDbToGain (-env);
            fbGain = grGain;
            const float g = grGain * fastDbToGain (makeup.next());
            const float m = mix.next();
            float wl = xl * g, wr = xr * g;

            if (style == Slam)
            {
                /* FET-style grit that grows with the gain reduction */
                const float amt = 0.08f + 0.5f * clampf (env / 14.0f, 0.0f, 1.0f), d = 1.0f + 3.0f * amt;
                const float off = fastTanh (d * 0.08f) / d;
                wl = dc[0].process (wl + amt * (fastTanh (d * (wl + 0.08f)) / d - off - wl));
                wr = dc[1].process (wr + amt * (fastTanh (d * (wr + 0.08f)) / d - off - wr));
            }
            else if (style == Opto)
            {
                wl = dc[0].process (wl + 0.06f * wl * wl);
                wr = dc[1].process (wr + 0.06f * wr * wr);
            }

            out[2 * i] = fToS16 (xl + m * (wl - xl));
            out[2 * i + 1] = fToS16 (xr + m * (wr - xr));
        }
    }

    int display (int idx, char* b, int n)
    {
        const float v = params.get (idx);
        if (idx == pAtt) return fmtMs (b, n, attackMs());
        if (idx == pRel) return fmtMs (b, n, releaseMs());
        if (idx == pScHpf) { const float h = hpfHz(); return h <= 20.5f ? snprintf (b, (size_t) n, "Off") : fmtHz (b, n, h); }
        if (idx == pRatio) return snprintf (b, (size_t) n, v < 10 ? "%.1f:1" : "%.0f:1", v);
        if (idx == pThr || idx == pMakeup) return fmtDb (b, n, v);
        if (idx == pKnee) return snprintf (b, (size_t) n, "%.1f dB", v);
        if (idx == pMix) return snprintf (b, (size_t) n, "%.0f %%", v);
        return 0;
    }
};

extern "C" const mpc_engine_t* mpc_engine (void) { return EngineGlue<EraC>::get(); }
