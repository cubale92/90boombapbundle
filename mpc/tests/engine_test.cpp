/* Offline audio test for one MPC engine (built per port with its generated params.h):
 *   engine_test <out-dir> [desktop-test-runner]
 * Renders a 92 BPM boom bap test loop (int16, 44.1 kHz, 128-frame blocks, exactly what MPC feeds an insert) through
 * every preset of the port's presets.json, checks for NaN / runaway output, times the DSP, and, given the desktop
 * test runner (tests/TestMain.cpp --render), renders the same input through the desktop plug-in and reports how close
 * the MPC engine comes: level difference and the residual after aligning for latency. Exit status 1 on a failure. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
extern "C" {
#include "engine.h"
}
#include "params.h"

static const int kRate = 44100, kBlock = 128;

static std::vector<float> makeLoop (double seconds)
{
    const int n = (int) (seconds * kRate);
    std::vector<float> b ((size_t) n * 2);
    unsigned rs = 1234;
    auto rnd = [&rs]() { rs = rs * 1664525u + 1013904223u; return (float) (rs >> 8) / 16777216.0f; };
    const double beat = 60.0 / 92.0, tw = 2 * M_PI;
    float lpS = 0, hpH = 0, prevH = 0;
    for (int i = 0; i < n; i++)
    {
        const double t = (double) i / kRate, inBeat = fmod (t, beat);
        const int beatIdx = (int) (t / beat) % 4;
        float x = 0;
        const double kickT = fmod (t, beat * 2.0);
        if (kickT < 0.35) { const double f = 50 + 70 * exp (-kickT * 30); x += (float) (0.8 * exp (-kickT * 9) * sin (tw * f * kickT)); }
        if ((beatIdx == 1 || beatIdx == 3) && inBeat < 0.25)
        {
            lpS += 0.35f * ((rnd() * 2 - 1) - lpS);
            x += (float) (0.55 * exp (-inBeat * 18) * (lpS + 0.4 * sin (tw * 190 * inBeat)));
        }
        const double eighth = beat / 2.0, hT = fmod (t + (fmod (t, beat) > eighth ? -0.03 : 0.0), eighth);
        if (hT < 0.06) { const float nz = rnd() * 2 - 1; hpH = nz - prevH; prevH = nz; x += (float) (0.18 * exp (-hT * 70) * hpH); }
        const float bass = (float) (0.28 * sin (tw * 55.0 * t));
        const float chord = (float) (0.08 * (sin (tw * 261.6 * t) + sin (tw * 329.6 * t) + sin (tw * 392.0 * t + 0.3)));
        /* -6 dB so the int16 input has headroom; quantised to int16 so both renders see the same samples */
        for (int c = 0; c < 2; c++)
        {
            const float v = 0.5f * (x + bass + chord * (c ? 0.9f : 1.1f));
            b[(size_t) (2 * i + c)] = (float) lrintf (v * 32768.0f) / 32768.0f;
        }
    }
    return b;
}

static double rms (const float* x, size_t n) { double s = 0; for (size_t i = 0; i < n; i++) s += (double) x[i] * x[i]; return sqrt (s / (double) (n ? n : 1)); }
static double db (double v) { return v > 1e-12 ? 20 * log10 (v) : -240; }

int main (int argc, char** argv)
{
    if (argc < 2) { fprintf (stderr, "usage: engine_test <out-dir> [desktop-runner]\n"); return 2; }
    const std::string dir = argv[1];
    const char* desktop = argc > 2 ? argv[2] : 0;
    const mpc_engine_t* api = mpc_engine();
    if (! api || ! api->process) { printf ("FAIL: no process()\n"); return 1; }

    std::vector<float> in = makeLoop (6.0);
    const int frames = (int) in.size() / 2;
    const std::string inPath = dir + "/in.f32";
    { FILE* f = fopen (inPath.c_str(), "wb"); fwrite (in.data(), sizeof (float), in.size(), f); fclose (f); }
    std::vector<int16_t> in16 (in.size());
    for (size_t i = 0; i < in.size(); i++) in16[i] = (int16_t) lrintf (in[i] * 32768.0f);
    const double inRms = db (rms (in.data(), in.size()));

    int failures = 0;
    double worstUs = 0;
    printf ("%-24s %8s %8s %8s %9s %s\n", "preset", "out dBFS", "peak", "vs desk", "residual", "");
    for (int p = 0; p < NPRESETS; p++)
    {
        void* inst = api->create (0);
        for (int k = 0; k < PRESETS[p].n; k++)
            api->set_param (inst, PARAMS[PRESETS[p].values[k].param].key, PRESETS[p].values[k].value);

        std::vector<float> out (in.size());
        int16_t ob[kBlock * 2];
        double us = 0;
        for (int pos = 0; pos + kBlock <= frames; pos += kBlock)
        {
            timespec t0, t1;
            clock_gettime (CLOCK_MONOTONIC, &t0);
            api->process (inst, &in16[(size_t) pos * 2], ob, kBlock);
            clock_gettime (CLOCK_MONOTONIC, &t1);
            us += (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
            for (int i = 0; i < kBlock * 2; i++) out[(size_t) pos * 2 + i] = ob[i] / 32768.0f;
        }
        const int blocks = frames / kBlock;
        us /= blocks;
        if (us > worstUs) worstUs = us;

        /* state round trip: save, load into a fresh instance, compare the parameters */
        char st[8192], st2[8192];
        if (api->get_param (inst, "state", st, sizeof st) > 0)
        {
            void* b = api->create (0);
            api->set_param (b, "state", st);
            api->get_param (b, "state", st2, sizeof st2);
            if (strcmp (st, st2)) { printf ("FAIL %s: state round trip differs\n", PRESETS[p].name); failures++; }
            api->destroy (b);
        }
        api->destroy (inst);

        bool finite = true;
        float peak = 0;
        for (float v : out) { if (! (v == v)) finite = false; peak = fmaxf (peak, fabsf (v)); }
        const size_t skip = (size_t) kRate * 2;   /* compare after the first second (envelopes settled) */
        const double outRms = db (rms (out.data() + skip, out.size() - skip));
        if (! finite) { printf ("FAIL %s: NaN in output\n", PRESETS[p].name); failures++; }
        if (outRms < inRms - 40) { printf ("FAIL %s: output silent\n", PRESETS[p].name); failures++; }

        char cmp[64] = "", res[64] = "";
        if (desktop)
        {
            const std::string outPath = dir + "/desk.f32";
            char cmd[2048];
            snprintf (cmd, sizeof cmd, "\"%s\" --render %d \"%s\" \"%s\" > /dev/null", desktop, p, inPath.c_str(), outPath.c_str());
            if (system (cmd) != 0) { printf ("FAIL: desktop render\n"); return 1; }
            std::vector<float> d (in.size());
            FILE* f = fopen (outPath.c_str(), "rb");
            const size_t got = fread (d.data(), sizeof (float), d.size(), f);
            fclose (f);
            if (got != d.size()) { printf ("FAIL: short desktop render\n"); return 1; }

            /* best alignment within +-200 samples (the latencies differ: 2x vs 4x oversampling in Era-S) */
            const size_t n = d.size() / 2 - skip / 2 - 400;
            double best = 1e30; int bestLag = 0;
            for (int lag = -200; lag <= 200; lag++)
            {
                double e = 0;
                for (size_t i = skip / 2; i < skip / 2 + n; i += 3)
                    for (int c = 0; c < 2; c++)
                    {
                        const double diff = out[(i + (size_t) (lag + 200) - 200) * 2 + c] - d[i * 2 + c];
                        e += diff * diff;
                    }
                if (e < best) { best = e; bestLag = lag; }
            }
            std::vector<float> diff;
            diff.reserve (n * 2);
            for (size_t i = skip / 2; i < skip / 2 + n; i++)
                for (int c = 0; c < 2; c++) diff.push_back (out[(i + bestLag) * 2 + c] - d[i * 2 + c]);
            const double deskRms = db (rms (d.data() + skip, d.size() - skip));
            const double resid = db (rms (diff.data(), diff.size())) - deskRms;
            snprintf (cmp, sizeof cmp, "%+7.2f", outRms - deskRms);
            snprintf (res, sizeof res, "%6.1f dB (lag %d)", resid, bestLag);
        }
        printf ("%-24s %8.2f %8.2f %8s %s\n", PRESETS[p].name, outRms, db (peak), cmp, res);
    }
    printf ("worst DSP time per 128-frame block on this machine: %.1f us (MPC budget for the whole project: 2900 us)\n", worstUs);
    printf (failures ? "FAILED (%d)\n" : "ALL OK\n", failures);
    return failures ? 1 : 0;
}
