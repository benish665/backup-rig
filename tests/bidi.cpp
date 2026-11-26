#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
static juce::Image R (const juce::Font& f, const char* s, bool layout)
{
    juce::Image im (juce::Image::ARGB, 120, 40, true); juce::Graphics g (im); g.setColour (juce::Colours::white); g.setFont (f);
    if (! layout) g.drawText (juce::String::fromUTF8 (s), 0, 0, 120, 40, juce::Justification::centredLeft);
    else { juce::AttributedString a; a.setReadingDirection (juce::AttributedString::rightToLeft); a.append (juce::String::fromUTF8 (s), f, juce::Colours::white); juce::TextLayout t; t.createLayout (a, 120); t.draw (g, { 0, 0, 120, 40 }); }
    return im;
}
static int firstCol (const juce::Image& im) { for (int x = 0; x < im.getWidth(); ++x) for (int y = 0; y < im.getHeight(); ++y) if (im.getPixelAt (x, y).getAlpha() > 100) return x; return -1; }
static long diffLeft (const juce::Image& a, const juce::Image& b)
{ int fa = firstCol (a), fb = firstCol (b); long d = 0; for (int x = 0; x < 10; ++x) for (int y = 0; y < 40; ++y) d += std::abs (a.getPixelAt (fa + x, y).getAlpha() - b.getPixelAt (fb + x, y).getAlpha()); return d; }
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto tf = juce::Typeface::createSystemTypefaceFor (BinaryData::Heebo_ttf, BinaryData::Heebo_ttfSize);
    juce::Font f (juce::FontOptions (tf).withHeight (30));
    for (bool lay : { false, true })
    {
        auto A = R (f, "\xd7\x90", lay), B = R (f, "\xd7\xa9", lay), AB = R (f, "\xd7\x90\xd7\xa9", lay);
        { auto W = R (f, "\xd7\x90 \xd7\xa9", lay); std::cout << "  two words: left word is " << (diffLeft (W, A) < diffLeft (W, B) ? "ALEF (wrong)" : "SHIN (correct)") << std::endl;
          auto Bu = R (f, "\xe2\x80\xa2 \xd7\x90", lay); std::cout << "  bullet+alef: leftmost is " << (diffLeft (Bu, A) < 2000 ? "ALEF (bullet on right, RTL para)" : "BULLET (LTR para)") << std::endl; }
        std::cout << (lay ? "TextLayout RTL: " : "drawText: ") << "left glyph of 'alef-shin' matches " << (diffLeft (AB, A) < diffLeft (AB, B) ? "ALEF (wrong, LTR order)" : "SHIN (correct RTL)") << std::endl;
    }
}
