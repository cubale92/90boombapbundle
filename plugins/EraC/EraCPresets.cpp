#include "EraCProcessor.h"

namespace erac
{
namespace
{
struct C
{
    int style; float thr, ratio, att, rel; bool autoRel; float knee, makeup; float mix = 100; float hpf = 20;
};

era::FactoryPreset make (const char* cat, const char* name, C c)
{
    return { cat, name, {
        { "style", (float) c.style }, { "threshold", c.thr }, { "ratio", c.ratio }, { "attack", c.att },
        { "release", c.rel }, { "autorel", c.autoRel ? 1.0f : 0.0f }, { "knee", c.knee }, { "makeup", c.makeup },
        { "automakeup", 0.0f }, { "mix", c.mix }, { "schpf", c.hpf } } };
}
} // namespace

std::vector<era::FactoryPreset> factoryPresets()
{
    std::vector<era::FactoryPreset> v;
    v.push_back ({ "Init", "Init", {} });

    // Drums
    v.push_back (make ("Drums", "NY Parallel Smash",      { Slam,  -34, 12.0f, 0.5f,  70, false, 2,  12.0f, 38 }));
    v.push_back (make ("Drums", "Snare Snap Back",        { Punch, -20,  4.0f, 18.0f, 70, false, 4,   3.5f }));
    v.push_back (make ("Drums", "Break Glue",             { Punch, -16,  2.5f, 10.0f, 150, true, 6,   2.5f, 100, 60 }));
    v.push_back (make ("Drums", "Kick Knock",             { Clean, -18,  5.0f, 25.0f, 90, false, 3,   3.5f, 100, 20 }));
    v.push_back (make ("Drums", "New Jack Snare Crack",   { Slam,  -24,  8.0f, 0.2f,  45, false, 0,   7.0f, 70 }));
    v.push_back (make ("Drums", "Dusty Loop Pump",        { Punch, -22,  4.0f, 30.0f, 140, false, 6,  4.0f, 100, 20 }));
    v.push_back (make ("Drums", "Room Mic Crush",         { Slam,  -40, 20.0f, 0.1f, 120, false, 0,  16.0f, 30, 120 }));
    v.push_back (make ("Drums", "Hats Tamer",             { Clean, -24,  3.0f, 1.0f,  40, false, 6,   1.5f, 100, 400 }));

    // Bass
    v.push_back (make ("Bass", "Steady Bass Line",        { Opto,  -22,  4.0f, 15.0f, 300, true, 12,  4.0f }));
    v.push_back (make ("Bass", "Sub Leveler",             { Clean, -20,  3.0f, 30.0f, 220, false, 8,  3.0f }));
    v.push_back (make ("Bass", "Finger Bass Grip",        { Slam,  -20,  4.0f, 8.0f,  90, false, 4,   4.0f, 85 }));

    // Vocals
    v.push_back (make ("Vocals", "Boom Bap Rap Vocal",    { Slam,  -22,  6.0f, 3.0f,  70, false, 3,   6.0f, 100, 80 }));
    v.push_back (make ("Vocals", "R&B Silk Leveler",      { Opto,  -24,  3.0f, 12.0f, 350, true, 12,  5.0f, 100, 80 }));
    v.push_back (make ("Vocals", "Ad-lib Squash",         { Slam,  -30, 20.0f, 0.3f,  50, false, 0,  12.0f, 100, 120 }));
    v.push_back (make ("Vocals", "Harmony Stack Glue",    { Opto,  -20,  2.0f, 20.0f, 500, true, 12,  2.5f, 100, 120 }));
    v.push_back (make ("Vocals", "Hook Upfront",          { Punch, -18,  4.0f, 6.0f,  90, true, 6,   4.0f, 100, 100 }));

    // Samples & Keys
    v.push_back (make ("Samples & Keys", "Crate Loop Squeeze",{ Punch, -20,  3.0f, 20.0f, 160, true, 6,   3.0f }));
    v.push_back (make ("Samples & Keys", "Keys Smoother",     { Opto,  -22,  2.5f, 15.0f, 400, true, 12,  3.0f }));
    v.push_back (make ("Samples & Keys", "Horn Section Tame", { Clean, -18,  3.0f, 5.0f,  120, false, 6,  2.0f, 100, 150 }));

    // Bus & Master
    v.push_back (make ("Bus & Master", "Mix Bus Glue 2:1",    { Punch, -16,  2.0f, 30.0f, 200, true, 6,   2.0f, 100, 60 }));
    v.push_back (make ("Bus & Master", "Drum Bus 4:1 Slam",   { Punch, -20,  4.0f, 3.0f,  100, true, 4,   4.5f, 100, 40 }));
    v.push_back (make ("Bus & Master", "Gentle Master Hold",  { Clean, -12,  1.5f, 30.0f, 400, true, 12,  1.5f, 100, 90 }));
    v.push_back (make ("Bus & Master", "Radio Mix '94",       { Opto,  -18,  3.0f, 10.0f, 250, true, 12,  4.0f, 100, 70 }));
    return v;
}
} // namespace erac
