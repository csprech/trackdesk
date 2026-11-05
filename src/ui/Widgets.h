#pragma once

// The panel's parts: knobs, faders, push switches, LEDs, slide switches, the display, the level meter and the
// MULTI JOG dial. Each reads and writes the console through small functions, and repaints when what it shows
// changes (refresh() is called by the panel's timer).
//
// As on the other devices, shadows that don't move (knobs, keys and their wells, the dial) are drawn by the panel's
// background from knob faces and key footprints, so nothing clips them; the widgets draw only what moves or lights.
// A fader's cap moves, so it draws its own soft shadow, with room for it in its bounds.

#include "../core/Lcd.h"
#include "Materials.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace trackdesk::ui
{
/** The panel's printing: a bold grotesque, condensed for the tight spots. */
juce::Font panelFont (float height, bool condensed = false);
void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float height, juce::Colour c,
               juce::Justification j = juce::Justification::centred, bool condensed = false);

class Knob : public juce::Component
{
    friend struct GpuiAdapter;

public:
    Knob (juce::Colour cap, juce::Colour pointer, float defaultValue);
    std::function<float()> get;
    std::function<void (float)> set;
    void refresh();
    juce::Colour cap, pointer, tick;
    float defaultValue;
    juce::String tooltipName;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    /** The knob's circle within its bounds (the bounds leave room for the shadow). */
    juce::Rectangle<float> face() const;

private:
    float shown = -1.0f, dragStart = 0.0f;
};

class Fader : public juce::Component
{
    friend struct GpuiAdapter;

public:
    explicit Fader (juce::Colour cap);
    std::function<float()> get;
    std::function<void (float)> set;
    void refresh();
    juce::Colour cap;
    static constexpr float capW = 46.0f, capH = 78.0f;
    /** Room round the travel for the cap's shadow (it falls down and to the right). */
    static constexpr float padTop = 8.0f, padBottom = 16.0f, halfWidth = 44.0f;

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    /** The cap's centre y for a value (0 bottom .. 1 top). */
    float yFor (float v) const;

private:
    float shown = -1.0f, dragStart = 0.0f;
};

/** A push key. Light grey latching switches (MUTE, MAIN...), dark keys (MENU, F1-F4, REW, FF), and the transport
    keys with their lamp. Latching keys show pushed in while they're on. */
class PushKey : public juce::Component
{
    friend struct GpuiAdapter;

public:
    enum class Style
    {
        Light,
        Dark,
        Transport
    };
    PushKey (Style s, juce::String glyph = {});
    std::function<bool()> latched;      // (switches) on = pushed in
    std::function<int()> lamp;          // (transport keys) 0 off, 1 on, 2 blinking-on phase
    std::function<void()> onDown, onUp; // momentary press and release
    std::function<void()> onToggle;     // (switches) a click toggles
    void refresh();
    Style style;
    juce::String glyph;
    juce::Colour lampColour { 0xffff3b30 };

    /** An extruded key: the face on top of its walls, which show below it (seen from above at a slight tilt). */
    static constexpr float kMargin = 3.0f;
    static float extrusionFor (Style s) { return s == Style::Transport ? 6.0f : s == Style::Dark ? 4.0f : 5.0f; }
    /** The bounds for a key whose face (at rest) is `face`. */
    static juce::Rectangle<int> boundsFor (juce::Rectangle<float> face, Style s);
    float extrusion() const { return extrusionFor (style); }
    juce::Rectangle<float> faceArea() const;
    float faceCorner() const { return 3.5f; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool held = false, shownLatched = false;
    int shownLamp = -1;
};

class Led : public juce::Component
{
    friend struct GpuiAdapter;

public:
    explicit Led (juce::Colour b = juce::Colour (0xff36d65a)) : base (b) { setInterceptsMouseClicks (false, false); }
    /** The colour it shows (transparent when off). */
    std::function<juce::Colour()> colour;
    juce::Colour base;
    void refresh();
    void paint (juce::Graphics&) override;

private:
    juce::Colour shown { 0x01000000 };
};

/** A slide switch with 3 positions (MODE: LIVE / PC / MTR; ASSIGN: 9/10 / OFF / MAIN). */
class Slide3 : public juce::Component
{
    friend struct GpuiAdapter;

public:
    std::function<int()> get;
    std::function<void (int)> set;
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    int shown = -1;
};

/** The 128 x 64 graphic display behind its window, backlit. */
class LcdView : public juce::Component
{
    friend struct GpuiAdapter;

public:
    LcdView() { setInterceptsMouseClicks (false, false); }
    std::function<void (Lcd&)> render;
    float contrast = 0.6f;
    bool dim = false;
    void refresh();
    void paint (juce::Graphics&) override;

private:
    Lcd lcd;
    std::array<uint8_t, (size_t) (Lcd::W * Lcd::H)> last {};
};

/** The MAIN level meter: two columns of 12 LEDs (OL, 15, 10, 6, 3, 0, -2, -4, -7, -10, -20, -30). */
class MeterView : public juce::Component
{
    friend struct GpuiAdapter;

public:
    MeterView() { setInterceptsMouseClicks (false, false); }
    void setLevels (float left, float right); // linear peaks
    void paint (juce::Graphics&) override;
    /** The panel's dB scale (meter 0 dB = -16 dBFS, as the original's "0" sits well below clipping). */
    static int litSegments (float level);

private:
    std::array<int, 2> lit { -1, -1 };
    std::array<float, 2> held {};
};

/** The MULTI JOG dial: turned by dragging around it (or the scroll wheel), pushed by a click. */
class JogDial : public juce::Component
{
    friend struct GpuiAdapter;

public:
    std::function<void (int)> turn;
    std::function<void()> push, unpush;
    /** The knob within the dial's recess (the recess and the knob's shadow are the panel's). */
    juce::Rectangle<float> knobArea() const;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    float angle = 0.0f, lastAngle = 0.0f, accum = 0.0f, wheelAccum = 0.0f;
    bool dragged = false, pressed = false;
};
} // namespace trackdesk::ui
