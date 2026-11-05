#include "Lcd.h"
#include "LcdFont.h"

namespace trackdesk
{
namespace
{
    // a 3 x 5 face for dense lines: figures, capitals and a few signs
    struct Small
    {
        char c;
        const char* rows[5];
    };
    // clang-format off
    constexpr Small kSmall[] = {
        { ' ', { "...", "...", "...", "...", "..." } }, { '0', { "###", "#.#", "#.#", "#.#", "###" } },
        { '1', { ".#.", "##.", ".#.", ".#.", "###" } }, { '2', { "###", "..#", "###", "#..", "###" } },
        { '3', { "###", "..#", ".##", "..#", "###" } }, { '4', { "#.#", "#.#", "###", "..#", "..#" } },
        { '5', { "###", "#..", "###", "..#", "###" } }, { '6', { "###", "#..", "###", "#.#", "###" } },
        { '7', { "###", "..#", ".#.", ".#.", ".#." } }, { '8', { "###", "#.#", "###", "#.#", "###" } },
        { '9', { "###", "#.#", "###", "..#", "###" } }, { 'A', { ".#.", "#.#", "###", "#.#", "#.#" } },
        { 'B', { "##.", "#.#", "##.", "#.#", "##." } }, { 'C', { ".##", "#..", "#..", "#..", ".##" } },
        { 'D', { "##.", "#.#", "#.#", "#.#", "##." } }, { 'E', { "###", "#..", "##.", "#..", "###" } },
        { 'F', { "###", "#..", "##.", "#..", "#.." } }, { 'G', { ".##", "#..", "#.#", "#.#", ".##" } },
        { 'H', { "#.#", "#.#", "###", "#.#", "#.#" } }, { 'I', { "###", ".#.", ".#.", ".#.", "###" } },
        { 'J', { "..#", "..#", "..#", "#.#", ".#." } }, { 'K', { "#.#", "#.#", "##.", "#.#", "#.#" } },
        { 'L', { "#..", "#..", "#..", "#..", "###" } }, { 'M', { "#.#", "###", "###", "#.#", "#.#" } },
        { 'N', { "##.", "#.#", "#.#", "#.#", "#.#" } }, { 'O', { ".#.", "#.#", "#.#", "#.#", ".#." } },
        { 'P', { "##.", "#.#", "##.", "#..", "#.." } }, { 'Q', { ".#.", "#.#", "#.#", "##.", ".##" } },
        { 'R', { "##.", "#.#", "##.", "#.#", "#.#" } }, { 'S', { ".##", "#..", ".#.", "..#", "##." } },
        { 'T', { "###", ".#.", ".#.", ".#.", ".#." } }, { 'U', { "#.#", "#.#", "#.#", "#.#", "###" } },
        { 'V', { "#.#", "#.#", "#.#", "#.#", ".#." } }, { 'W', { "#.#", "#.#", "###", "###", "#.#" } },
        { 'X', { "#.#", "#.#", ".#.", "#.#", "#.#" } }, { 'Y', { "#.#", "#.#", ".#.", ".#.", ".#." } },
        { 'Z', { "###", "..#", ".#.", "#..", "###" } }, { ':', { "...", ".#.", "...", ".#.", "..." } },
        { '/', { "..#", "..#", ".#.", "#..", "#.." } }, { '.', { "...", "...", "...", "...", ".#." } },
        { '-', { "...", "...", "###", "...", "..." } }, { '+', { "...", ".#.", "###", ".#.", "..." } },
        { '_', { "...", "...", "...", "...", "###" } }, { '(', { ".#.", "#..", "#..", "#..", ".#." } },
        { ')', { ".#.", "..#", "..#", "..#", ".#." } }, { '*', { "...", "#.#", ".#.", "#.#", "..." } },
        { '=', { "...", "###", "...", "###", "..." } }, { '%', { "#.#", "..#", ".#.", "#..", "#.#" } },
        { '#', { "#.#", "###", "#.#", "###", "#.#" } }, { '?', { "##.", "..#", ".#.", "...", ".#." } },
    };
    // clang-format on
    const Small& smallGlyph (char c)
    {
        c = (char) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) c);
        for (auto& g : kSmall)
            if (g.c == c)
                return g;
        return kSmall[sizeof (kSmall) / sizeof (kSmall[0]) - 1];
    }
    int advance (Lcd::Font f) { return f == Lcd::Font::Small ? 4 : f == Lcd::Font::Big ? 12 : 6; }
} // namespace

void Lcd::clear()
{
    px.fill (0);
    written.clear();
}

void Lcd::set (int x, int y, bool on)
{
    if (x >= 0 && y >= 0 && x < W && y < H)
        px[(size_t) (y * W + x)] = on ? 1 : 0;
}

void Lcd::fill (int x, int y, int w, int h, bool on)
{
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            set (i, j, on);
}

void Lcd::invert (int x, int y, int w, int h)
{
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i)
            set (i, j, ! get (i, j));
}

void Lcd::frame (int x, int y, int w, int h, bool on)
{
    hline (x, y, w, on);
    hline (x, y + h - 1, w, on);
    vline (x, y, h, on);
    vline (x + w - 1, y, h, on);
}

void Lcd::dottedH (int x, int y, int w)
{
    for (int i = x; i < x + w; i += 2)
        set (i, y);
}

void Lcd::glyph (int x, int y, char c, Font f, bool on)
{
    if (f == Font::Small)
    {
        const auto& g = smallGlyph (c);
        for (int r = 0; r < 5; ++r)
            for (int k = 0; k < 3; ++k)
                if (g.rows[r][k] == '#')
                    set (x + k, y + r, on);
        return;
    }
    const auto& g = lcdfont::glyph (c);
    const int s = f == Font::Big ? 2 : 1;
    for (int r = 0; r < 7; ++r)
        for (int k = 0; k < 5; ++k)
            if (lcdfont::lit (g, k, r))
                fill (x + k * s, y + r * s, s, s, on);
}

int Lcd::text (int x, int y, const juce::String& s, Font f, bool on)
{
    if (s.isNotEmpty())
        written.add (s);
    for (auto ch : s)
    {
        glyph (x, y, (char) (ch < 128 ? ch : '?'), f, on);
        x += advance (f);
    }
    return x;
}

int Lcd::textWidth (const juce::String& s, Font f) { return s.length() * advance (f) - (s.isEmpty() ? 0 : advance (f) - (f == Font::Small ? 3 : f == Font::Big ? 10 : 5)); }

int Lcd::textRight (int right, int y, const juce::String& s, Font f, bool on) { return text (right - textWidth (s, f), y, s, f, on); }

int Lcd::textCentred (int cx, int y, const juce::String& s, Font f, bool on) { return text (cx - textWidth (s, f) / 2, y, s, f, on); }

int Lcd::label (int x, int y, const juce::String& s, Font f)
{
    const int w = textWidth (s, f) + 2, h = lineHeight (f) + 2;
    fill (x, y, w, h);
    text (x + 1, y + 1, s, f, false);
    return x + w;
}

void Lcd::icon (int x, int y, std::initializer_list<const char*> rows, bool on)
{
    int r = 0;
    for (auto* row : rows)
    {
        for (int k = 0; row[k] != 0; ++k)
            if (row[k] == '#')
                set (x + k, y + r, on);
        ++r;
    }
}

bool Lcd::shows (const juce::String& s) const
{
    for (auto& t : written)
        if (t.contains (s))
            return true;
    return false;
}
} // namespace trackdesk
