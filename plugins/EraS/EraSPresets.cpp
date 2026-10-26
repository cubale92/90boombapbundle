#include "EraSProcessor.h"

namespace eras
{
namespace
{
struct S
{
    int mode; float drive, bias = 0, tone = 0, low = 10, high = 22000; float bits = 12, rate = 26040, dust = 0, mix = 100, out = 0;
};

era::FactoryPreset make (const char* cat, const char* name, S s)
{
    return { cat, name, {
        { "mode", (float) s.mode }, { "drive", s.drive }, { "bias", s.bias }, { "tone", s.tone },
        { "lowcut", s.low }, { "highcut", s.high }, { "bits", s.bits }, { "rate", s.rate },
        { "dust", s.dust }, { "mix", s.mix }, { "output", s.out }, { "autogain", 1.0f } } };
}
} // namespace

std::vector<era::FactoryPreset> factoryPresets()
{
    std::vector<era::FactoryPreset> v;
    v.push_back ({ "Init", "Init", {} });

    // Sampler: the classic 12-bit drum machine / sampler colours
    v.push_back (make ("Sampler", "SP 26k Grit",              { Sampler26, 3, 0, 0, 10, 22000, 12 }));
    v.push_back (make ("Sampler", "SP 26k Hot Converter",     { Sampler26, 10, 10, 1, 10, 22000, 12 }));
    v.push_back (make ("Sampler", "SP Filtered Loop",         { Sampler26, 5, 0, -2, 40, 9000, 12 }));
    v.push_back (make ("Sampler", "Sixty 40k Swing Warmth",   { Sampler40, 3, 0, -1, 10, 22000, 12 }));
    v.push_back (make ("Sampler", "Sixty Drum Bus",           { Sampler40, 7, 5, 1, 25, 22000, 12 }));
    v.push_back (make ("Sampler", "Vari 32k Smooth",          { VariRate, 2, 0, -1, 10, 22000, 12, 32000 }));
    v.push_back (make ("Sampler", "Vari 22k Lo-Fi Keys",      { VariRate, 4, 0, -2, 60, 22000, 12, 22050 }));
    v.push_back (make ("Sampler", "Pitched-Down 15k",         { VariRate, 5, 0, -3, 30, 22000, 12, 15000, 0, 100 }));
    v.push_back (make ("Sampler", "8-Bit Basement",           { VariRate, 6, 0, 0, 40, 22000, 8, 12000, 0, 55 }));
    v.push_back (make ("Sampler", "Crushed Hat Sparkle",      { Sampler26, 2, 0, 3, 200, 22000, 10, 26040, 0, 60 }));

    // Tape & Vinyl
    v.push_back (make ("Tape & Vinyl", "Cassette Demo '93",   { Tape, 9, 10, -2, 40, 12000, 12, 26040, 12 }));
    v.push_back (make ("Tape & Vinyl", "Reel Warm Drums",     { Tape, 7, 0, 0, 20, 22000 }));
    v.push_back (make ("Tape & Vinyl", "Dusty Vinyl Loop",    { Tape, 4, 5, -2, 60, 10000, 12, 26040, 45 }));
    v.push_back (make ("Tape & Vinyl", "Crate Crackle Only",  { Tape, 0, 0, 0, 10, 22000, 12, 26040, 60 }));
    v.push_back (make ("Tape & Vinyl", "Mixtape Dub",         { Tape, 12, 15, -3, 50, 9000, 12, 26040, 20, 100, -1 }));
    v.push_back (make ("Tape & Vinyl", "Bus Tape Glue",       { Tape, 5, 0, 0, 10, 22000, 12, 26040, 0, 70 }));

    // Tube
    v.push_back (make ("Tube", "R&B Vocal Glow",              { Tube, 5, 15, 1, 60, 22000, 12, 26040, 0, 55 }));
    v.push_back (make ("Tube", "Bass Tube Growl",             { Tube, 13, 20, 0, 25, 9000, 12, 26040, 0, 70 }));
    v.push_back (make ("Tube", "Rhodes Amp Bark",             { Tube, 10, 25, -1, 40, 14000, 12, 26040, 0, 85 }));
    v.push_back (make ("Tube", "Warm Mix Bus",                { Tube, 3, 10, 0, 10, 22000, 12, 26040, 0, 50 }));

    // Console
    v.push_back (make ("Console", "Console Kick Push",        { Console, 9, 0, 0, 25, 22000 }));
    v.push_back (make ("Console", "Snare Board Drive",        { Console, 14, 10, 2, 90, 22000, 12, 26040, 0, 80 }));
    v.push_back (make ("Console", "New Jack Drum Machine Bus",{ Console, 6, 0, 2, 30, 22000 }));
    v.push_back (make ("Console", "Telephone Radio Hook",     { Console, 18, 0, 0, 450, 3400, 12, 26040, 0, 100, -2 }));
    return v;
}
} // namespace eras
