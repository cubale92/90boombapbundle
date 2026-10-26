#include "EraQProcessor.h"

namespace eraq
{
namespace
{
using Values = std::vector<std::pair<juce::String, float>>;

struct B { int type; float freq, gain = 0, q = 0.707f; int slope = 1; int place = Stereo; };

era::FactoryPreset make (const char* cat, const char* name, std::initializer_list<B> bands,
                         float color = 0, float output = 0)
{
    Values v;
    int i = 0;
    for (const auto& b : bands)
    {
        v.push_back ({ pid (i, "on"), 1.0f });
        v.push_back ({ pid (i, "type"), (float) b.type });
        v.push_back ({ pid (i, "freq"), b.freq });
        v.push_back ({ pid (i, "gain"), b.gain });
        v.push_back ({ pid (i, "q"), b.q });
        v.push_back ({ pid (i, "slope"), (float) b.slope });
        v.push_back ({ pid (i, "place"), (float) b.place });
        ++i;
    }
    v.push_back ({ "color", color });
    v.push_back ({ "output", output });
    return { cat, name, v };
}
} // namespace

std::vector<era::FactoryPreset> factoryPresets()
{
    return {
        make ("Init", "Init", {}),

        // --- Boom Bap Drums ---------------------------------------------------
        make ("Boom Bap Drums", "Dusty Kick Knock",
              { { LowCut, 28, 0, 0.707f, 2 }, { Bell, 62, 4.0f, 1.4f }, { Bell, 320, -3.5f, 1.6f },
                { Bell, 3200, 2.5f, 1.2f }, { HighCut, 9500, 0, 0.707f, 1 } }, 15),
        make ("Boom Bap Drums", "Premier Snare Crack",
              { { LowCut, 110, 0, 0.707f, 1 }, { Bell, 210, 3.0f, 1.5f }, { Bell, 650, -3.0f, 2.0f },
                { Bell, 5200, 3.5f, 0.9f }, { HighShelf, 11000, -2.0f } }, 20),
        make ("Boom Bap Drums", "SP Hat Grit",
              { { LowCut, 380, 0, 0.707f, 2 }, { Bell, 2600, 2.0f, 1.0f }, { Bell, 7500, -2.5f, 2.5f },
                { HighCut, 12500, 0, 0.707f, 1 } }, 25),
        make ("Boom Bap Drums", "Drum Bus Thump",
              { { LowCut, 25, 0, 0.707f, 2 }, { LowShelf, 85, 2.5f }, { Bell, 400, -2.0f, 0.9f },
                { Bell, 2200, 1.5f, 0.8f }, { HighShelf, 9000, -1.5f } }, 30),
        make ("Boom Bap Drums", "Loop Break Carve",
              { { LowCut, 45, 0, 0.707f, 1 }, { Bell, 120, 2.0f, 1.0f }, { Bell, 900, -2.5f, 1.2f },
                { Bell, 3800, 2.0f, 1.0f }, { HighCut, 10500, 0, 0.707f, 1 } }, 20),

        // --- Samples & Loops --------------------------------------------------
        make ("Samples & Loops", "Dusty Vinyl Loop",
              { { LowCut, 70, 0, 0.707f, 1 }, { Bell, 250, 1.5f, 0.8f }, { Bell, 1800, -1.5f, 1.0f },
                { HighCut, 7500, 0, 0.9f, 1 } }, 35),
        make ("Samples & Loops", "Jazz Sample Make Room",
              { { LowCut, 160, 0, 0.707f, 1 }, { Bell, 500, -2.0f, 0.9f }, { Bell, 2500, -2.5f, 1.0f, 1, Mid },
                { HighShelf, 8000, -2.0f } }, 10),
        make ("Samples & Loops", "Filtered Bassline Trick",
              { { LowCut, 30, 0, 0.707f, 1 }, { HighCut, 280, 0, 1.1f, 2 }, { Bell, 90, 2.0f, 1.0f } }, 20),
        make ("Samples & Loops", "Telephone Chop",
              { { LowCut, 450, 0, 1.0f, 2 }, { Bell, 1500, 4.0f, 1.0f }, { HighCut, 3400, 0, 1.0f, 2 } }, 40),

        // --- Bass -------------------------------------------------------------
        make ("Bass", "808 Sub Focus",
              { { LowCut, 30, 0, 0.707f, 2 }, { Bell, 50, 3.0f, 1.3f }, { Bell, 250, -3.0f, 1.0f },
                { Bell, 900, 2.0f, 1.5f }, { HighCut, 5000, 0, 0.707f, 1 } }, 15),
        make ("Bass", "Upright Jazz Bass",
              { { LowCut, 40, 0, 0.707f, 1 }, { Bell, 110, 2.0f, 1.0f }, { Bell, 350, -2.0f, 1.2f },
                { Bell, 1500, 2.5f, 1.4f }, { HighCut, 6500, 0, 0.707f, 1 } }, 10),
        make ("Bass", "New Jack Synth Bass",
              { { LowCut, 35, 0, 0.707f, 2 }, { LowShelf, 90, 2.0f }, { Bell, 600, -2.0f, 1.0f },
                { Bell, 2000, 3.0f, 1.2f } }, 10),

        // --- New Jack Swing ---------------------------------------------------
        make ("New Jack Swing", "Gated Snare Smack",
              { { LowCut, 120, 0, 0.707f, 1 }, { Bell, 240, 3.5f, 1.3f }, { Bell, 1100, -2.0f, 1.5f },
                { Bell, 6000, 4.0f, 0.8f }, { HighShelf, 12000, 2.0f } }),
        make ("New Jack Swing", "Teddy Riley Keys",
              { { LowCut, 150, 0, 0.707f, 1 }, { Bell, 400, -2.0f, 0.9f }, { Bell, 3000, 2.0f, 0.8f },
                { HighShelf, 9000, 2.5f } }),
        make ("New Jack Swing", "Swing Beat Bus",
              { { LowCut, 28, 0, 0.707f, 1 }, { LowShelf, 70, 2.0f }, { Bell, 350, -1.5f, 0.8f },
                { HighShelf, 10000, 2.5f } }, 10),
        make ("New Jack Swing", "Stab Brass Bite",
              { { LowCut, 200, 0, 0.707f, 1 }, { Bell, 800, -1.5f, 1.0f }, { Bell, 2800, 3.0f, 1.0f },
                { HighCut, 12000, 0, 0.707f, 1 } }),

        // --- R&B Vocals -------------------------------------------------------
        make ("R&B Vocals", "Silky Lead Vocal",
              { { LowCut, 85, 0, 0.707f, 1 }, { Bell, 280, -2.0f, 1.2f }, { Bell, 3200, 1.5f, 0.8f },
                { Bell, 7200, -1.5f, 3.0f }, { HighShelf, 12000, 3.0f } }, 10),
        make ("R&B Vocals", "Velvet Background Stack",
              { { LowCut, 160, 0, 0.707f, 2 }, { Bell, 400, -2.5f, 1.0f }, { Bell, 2500, -1.5f, 0.8f },
                { HighShelf, 10000, 3.5f }, { Bell, 5000, 1.5f, 0.7f, 1, Side } }),
        make ("R&B Vocals", "Rap Vocal Presence",
              { { LowCut, 95, 0, 0.707f, 2 }, { Bell, 230, -2.0f, 1.4f }, { Bell, 1000, 1.0f, 1.0f },
                { Bell, 4200, 2.5f, 1.0f }, { HighShelf, 10000, 1.5f } }, 15),
        make ("R&B Vocals", "Boxy Booth Fix",
              { { LowCut, 90, 0, 0.707f, 1 }, { Bell, 350, -4.0f, 2.2f }, { Bell, 800, -2.0f, 2.5f },
                { HighShelf, 9000, 2.0f } }),

        // --- Mix Bus / Master -------------------------------------------------
        make ("Mix Bus", "Golden Era Mix Bus",
              { { LowCut, 24, 0, 0.707f, 1 }, { LowShelf, 95, 1.5f }, { Bell, 420, -1.2f, 0.8f },
                { HighShelf, 12000, 1.2f } }, 25),
        make ("Mix Bus", "90s Tape Master Tilt",
              { { LowCut, 22, 0, 0.707f, 2 }, { LowShelf, 100, 1.5f }, { Bell, 3000, -0.8f, 0.6f },
                { HighShelf, 14000, -1.0f } }, 20),
        make ("Mix Bus", "Radio Ready Sheen",
              { { LowCut, 25, 0, 0.707f, 2 }, { Bell, 300, -1.0f, 0.7f }, { HighShelf, 11000, 2.0f },
                { Bell, 140, 1.5f, 0.8f, 1, Mid }, { LowCut, 120, 0, 0.707f, 1, Side } }, 10),
        make ("Mix Bus", "Mono Low End",
              { { LowCut, 150, 0, 0.707f, 2, Side }, { LowCut, 20, 0, 0.707f, 2 } }),
        make ("Mix Bus", "Wide Top Air",
              { { HighShelf, 8000, 2.5f, 0.707f, 1, Side }, { Bell, 400, -1.0f, 0.8f, 1, Side } }),
    };
}
} // namespace eraq
