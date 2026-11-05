#pragma once

// The console's 128 x 64 monochrome graphic display, drawn by the console and shown by the panel.
// Everything written as text is also kept as a list of strings, so tests can read the screen.

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

namespace trackdesk
{
class Lcd
{
public:
    static constexpr int W = 128, H = 64;
    enum class Font : uint8_t
    {
        Normal, // 5 x 7, 6 px apart
        Small,  // 3 x 5, 4 px apart (capitals and figures)
        Big     // 5 x 7 doubled, 12 px apart
    };

    void clear();
    bool get (int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H && px[(size_t) (y * W + x)] != 0; }
    void set (int x, int y, bool on = true);
    void fill (int x, int y, int w, int h, bool on = true);
    void invert (int x, int y, int w, int h);
    void frame (int x, int y, int w, int h, bool on = true);
    void hline (int x, int y, int w, bool on = true) { fill (x, y, w, 1, on); }
    void vline (int x, int y, int h, bool on = true) { fill (x, y, 1, h, on); }
    void dottedH (int x, int y, int w);
    /** Draws text with its top-left at x, y; returns the x after it. */
    int text (int x, int y, const juce::String& s, Font f = Font::Normal, bool on = true);
    int textRight (int right, int y, const juce::String& s, Font f = Font::Normal, bool on = true);
    int textCentred (int centreX, int y, const juce::String& s, Font f = Font::Normal, bool on = true);
    /** White text on a black box (the display's labels): returns the x after it. */
    int label (int x, int y, const juce::String& s, Font f = Font::Normal);
    static int textWidth (const juce::String& s, Font f = Font::Normal);
    static int lineHeight (Font f) { return f == Font::Small ? 5 : f == Font::Big ? 14 : 7; }
    /** A small picture from rows of '#' and '.' */
    void icon (int x, int y, std::initializer_list<const char*> rows, bool on = true);

    /** Everything written since clear(), in order. */
    const juce::StringArray& texts() const { return written; }
    juce::String allText() const { return written.joinIntoString ("|"); }
    bool shows (const juce::String& s) const;

private:
    std::array<uint8_t, (size_t) (W * H)> px {};
    juce::StringArray written;
    void glyph (int x, int y, char c, Font f, bool on);
};
} // namespace trackdesk
