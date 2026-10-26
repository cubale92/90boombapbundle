#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace era
{
// Golden Era palette: late-night Brooklyn navy, New Jack neon, gold-chain accents.
namespace Colours
{
    inline const juce::Colour background  { 0xff100f17 };
    inline const juce::Colour panel       { 0xff191823 };
    inline const juce::Colour panelLight  { 0xff22212f };
    inline const juce::Colour display     { 0xff0b0a10 };
    inline const juce::Colour grid        { 0xff2a2838 };
    inline const juce::Colour gridStrong  { 0xff3a374c };
    inline const juce::Colour outline     { 0xff2e2c3d };
    inline const juce::Colour text        { 0xffe9e6f2 };
    inline const juce::Colour textDim     { 0xff8c88a3 };
    inline const juce::Colour gold        { 0xfff2b84b };
    inline const juce::Colour teal        { 0xff2fd3c5 };
    inline const juce::Colour magenta     { 0xffe8457e };
    inline const juce::Colour purple      { 0xff9b6cff };
    inline const juce::Colour orange      { 0xffff8a3d };
    inline const juce::Colour meterGreen  { 0xff3fd68a };
    inline const juce::Colour meterYellow { 0xfff2d14b };
    inline const juce::Colour meterRed    { 0xffff4d5e };

    // Band colours for the EQ nodes
    inline juce::Colour band (int i)
    {
        static const juce::uint32 c[] = { 0xffff5d73, 0xffff9f43, 0xfff7d046, 0xff6ee26b,
                                          0xff2fd3c5, 0xff4aa3ff, 0xff9b6cff, 0xffe86fd3 };
        return juce::Colour (c[(size_t) juce::jlimit (0, 7, i)]);
    }
}

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace era
