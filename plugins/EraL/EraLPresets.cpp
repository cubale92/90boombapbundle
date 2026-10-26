#include "EraLProcessor.h"

namespace eral
{
namespace
{
era::FactoryPreset make (const char* cat, const char* name, int style, float gain, float ceiling, float release, float link = 100)
{
    return { cat, name, { { "style", (float) style }, { "gain", gain }, { "ceiling", ceiling },
                          { "release", release }, { "link", link }, { "unity", 0.0f } } };
}
} // namespace

std::vector<era::FactoryPreset> factoryPresets()
{
    std::vector<era::FactoryPreset> v;
    v.push_back ({ "Init", "Init (-1 dB Safety)", {} });

    // Master
    v.push_back (make ("Master", "Golden Era Master",      Warm,   4.0f, -0.3f, 120));
    v.push_back (make ("Master", "Loud '95 Single",        Loud95, 8.0f, -0.1f,  60));
    v.push_back (make ("Master", "R&B Radio Edit",         Clean,  5.0f, -0.5f, 200));
    v.push_back (make ("Master", "New Jack Club Mix",      Punchy, 7.0f, -0.3f,  80));
    v.push_back (make ("Master", "Mixtape Tape Slam",      Warm,   9.0f, -0.5f,  40));
    v.push_back (make ("Master", "Vinyl Cut Headroom",     Clean,  2.0f, -2.0f, 250));
    v.push_back (make ("Master", "Streaming Safe -1",      Clean,  3.0f, -1.0f, 150));
    v.push_back (make ("Master", "Boom Bap Knock Master",  Punchy, 6.0f, -0.3f,  50));

    // Bus
    v.push_back (make ("Bus", "Drum Bus Peak Catch",       Punchy, 3.0f, -3.0f,  30));
    v.push_back (make ("Bus", "Snare Crack Clip",          Loud95, 5.0f, -4.0f,  15));
    v.push_back (make ("Bus", "Vocal Peak Catch",          Clean,  2.0f, -6.0f,  80));
    v.push_back (make ("Bus", "808 / Sub Clip",            Loud95, 6.0f, -1.0f,  30));
    v.push_back (make ("Bus", "Sample Loop Flatten",       Warm,   4.0f, -3.0f, 100, 60));
    v.push_back (make ("Bus", "Wide Keys Hold",            Clean,  3.0f, -4.0f, 200, 40));
    return v;
}
} // namespace eral
