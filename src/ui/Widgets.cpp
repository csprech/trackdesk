#include "Widgets.h"
#include "../core/Model.h"

namespace trackdesk::ui
{
namespace m = materials;

namespace
{
    constexpr float kKnobSweep = juce::MathConstants<float>::pi * 0.83f; // +-150 degrees

    bool haveFont (const juce::String& name)
    {
        static const auto names = juce::Font::findAllTypefaceNames();
        return names.contains (name);
    }
} // namespace

juce::Font panelFont (float h, bool condensed)
{
    if (haveFont ("Helvetica Neue"))
        return juce::Font (juce::FontOptions ("Helvetica Neue", condensed ? "Condensed Bold" : "Bold", h));
    return juce::Font (juce::FontOptions (h, juce::Font::bold));
}

void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float h, juce::Colour c, juce::Justification j, bool condensed)
{
    g.setColour (c);
    g.setFont (panelFont (h, condensed));
    g.drawFittedText (text, r.toNearestInt(), j, 1, 0.7f);
}

// ======================================================================== Knob

Knob::Knob (juce::Colour c, juce::Colour p, float d) : cap (c), pointer (p), tick (c), defaultValue (d)
{
    setRepaintsOnMouseActivity (false);
}

juce::Rectangle<float> Knob::face() const
{
    const float d = std::min ((float) getWidth(), (float) getHeight()) * 0.80f;
    return juce::Rectangle<float> (d, d).withCentre (getLocalBounds().toFloat().getCentre().translated (-d * 0.05f, -d * 0.05f));
}

void Knob::refresh()
{
    const float v = get ? get() : 0.0f;
    if (std::abs (v - shown) > 0.0005f)
    {
        shown = v;
        repaint();
    }
}

void Knob::paint (juce::Graphics& g)
{
    const float v = get ? get() : 0.0f;
    shown = v;
    const auto f = face(); // (its shadow is on the panel)
    const float scale = m::scaleOf (g);
    const int px = juce::roundToInt (f.getWidth() * scale);
    // the angle in steps of a degree: enough to look smooth, few enough to cache
    const int step = juce::roundToInt ((v * 2.0f - 1.0f) * kKnobSweep / juce::MathConstants<float>::pi * 180.0f);
    const float angle = (float) step / 180.0f * juce::MathConstants<float>::pi;
    const auto key = "capknob" + juce::String (px) + "_" + juce::String (step) + "_" + cap.toString() + pointer.toString() + tick.toString();
    m::drawAt (g, m::cached (key, [&] { return m::capKnob (px, angle, cap, pointer, tick); }), f);
}

void Knob::mouseDown (const juce::MouseEvent&) { dragStart = get ? get() : 0.0f; }

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    // up/right turns it clockwise: 250 px for the whole sweep (slower with shift or alt)
    const float range = e.mods.isShiftDown() || e.mods.isAltDown() ? 1200.0f : 250.0f;
    const float d = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / range;
    if (set)
        set (juce::jlimit (0.0f, 1.0f, dragStart + d));
    refresh();
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (set)
        set (defaultValue);
    refresh();
}

void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (set && get)
        set (juce::jlimit (0.0f, 1.0f, get() + (w.deltaY + w.deltaX) * (w.isReversed ? -0.25f : 0.25f)));
    refresh();
}

// ======================================================================== Fader

Fader::Fader (juce::Colour c) : cap (c) {}

void Fader::refresh()
{
    const float v = get ? get() : 0.0f;
    if (std::abs (v - shown) > 0.0005f)
    {
        shown = v;
        repaint();
    }
}

float Fader::yFor (float v) const
{
    const float top = padTop + capH * 0.5f, bottom = (float) getHeight() - padBottom - capH * 0.5f;
    return bottom - (bottom - top) * juce::jlimit (0.0f, 1.0f, v);
}

bool Fader::hitTest (int x, int)
{
    // the bounds leave room for the shadow; only the slot's column takes the mouse
    return std::abs ((float) x - getWidth() * 0.5f) <= capW * 0.5f + 4.0f;
}

void Fader::paint (juce::Graphics& g)
{
    const float v = get ? get() : 0.0f;
    shown = v;
    const float cx = getWidth() * 0.5f;
    // the slot: a dark slit with a lit lower lip
    const float slotTop = yFor (1.0f) - 6.0f, slotBottom = yFor (0.0f) + 6.0f;
    juce::Rectangle<float> slot (cx - 3.5f, slotTop, 7.0f, slotBottom - slotTop);
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (slot, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (slot.getRight() + 0.5f, slot.getY() + 3, slot.getRight() + 0.5f, slot.getBottom() - 3, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (slot.getX() - 0.5f, slot.getY() + 3, slot.getX() - 0.5f, slot.getBottom() - 3, 1.0f);
    // the cap stands on its lever above the panel: a soft shadow cast down-right, a tight one where it's closest
    const float cy = yFor (v);
    juce::Rectangle<float> capR (cx - capW * 0.5f, cy - capH * 0.5f, capW, capH);
    juce::Path foot;
    foot.addRoundedRectangle (capR.translated (1.5f, 1.0f), 6.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 12, { 3, 6 }).drawForPath (g, foot);
    juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 4, { 1, 2 }).drawForPath (g, foot);
    const float scale = m::scaleOf (g);
    const int w = juce::roundToInt (capW * scale), h = juce::roundToInt (capH * scale);
    m::drawAt (g, m::cached ("fadercap" + juce::String (w) + "x" + juce::String (h) + cap.toString(), [&] { return m::faderCap (w, h, cap); }), capR);
}

void Fader::mouseDown (const juce::MouseEvent& e)
{
    // a click on the slot away from the cap moves the cap there; on the cap it's grabbed
    const float v = get ? get() : 0.0f;
    if (std::abs ((float) e.y - yFor (v)) > capH * 0.5f && set)
    {
        const float top = yFor (1.0f), bottom = yFor (0.0f);
        set (juce::jlimit (0.0f, 1.0f, (bottom - (float) e.y) / (bottom - top)));
    }
    dragStart = get ? get() : 0.0f;
    refresh();
}

void Fader::mouseDrag (const juce::MouseEvent& e)
{
    const float travel = yFor (0.0f) - yFor (1.0f);
    const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.2f : 1.0f;
    if (set)
        set (juce::jlimit (0.0f, 1.0f, dragStart - (float) e.getDistanceFromDragStartY() / travel * fine));
    refresh();
}

void Fader::mouseDoubleClick (const juce::MouseEvent&)
{
    if (set)
        set (law::kFaderUnity); // 0 dB
    refresh();
}

void Fader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    if (set && get)
        set (juce::jlimit (0.0f, 1.0f, get() + w.deltaY * (w.isReversed ? -0.15f : 0.15f)));
    refresh();
}

// ======================================================================== PushKey

PushKey::PushKey (Style s, juce::String gl) : style (s), glyph (std::move (gl)) {}

void PushKey::refresh()
{
    const bool l = latched ? latched() : false;
    const int lp = lamp ? lamp() : 0;
    if (l != shownLatched || lp != shownLamp)
    {
        shownLatched = l;
        shownLamp = lp;
        repaint();
    }
}

juce::Rectangle<int> PushKey::boundsFor (juce::Rectangle<float> face, Style s)
{
    auto b = face.expanded (kMargin);
    b.setBottom (b.getBottom() + extrusionFor (s));
    return b.getSmallestIntegerContainer();
}

juce::Rectangle<float> PushKey::faceArea() const
{
    auto r = getLocalBounds().toFloat().reduced (kMargin);
    r.removeFromBottom (extrusion());
    return r;
}

void PushKey::paint (juce::Graphics& g)
{
    shownLatched = latched ? latched() : false;
    shownLamp = lamp ? lamp() : 0;
    // an extruded cap seen from above at a slight tilt: its face on top, its walls showing below it down to the panel
    // (the well round it and its shadow are the panel's). Pushed, or latched in, the face sinks down the walls.
    const auto rest = faceArea();
    const float depth = extrusion(), corner = faceCorner();
    const bool in = held || shownLatched;
    const auto face = rest.translated (0.0f, in ? depth * 0.7f : 0.0f);
    const bool on = shownLamp > 0;
    const juce::Colour body = style == Style::Dark ? juce::Colour (0xff2b2c30) : style == Style::Transport ? juce::Colour (0xffe8e8e6) : juce::Colour (0xffd9d9d6);

    // a lit key throws a little of its light on the panel round its base
    if (on)
    {
        juce::ColourGradient bloom (lampColour.withAlpha (0.30f), rest.getCentreX(), rest.getBottom(), lampColour.withAlpha (0.0f), rest.getCentreX(),
                                    rest.getBottom() + depth + kMargin, true);
        g.setGradientFill (bloom);
        g.fillRoundedRectangle (rest.expanded (kMargin * 0.8f).withBottom (rest.getBottom() + depth + kMargin * 0.8f), corner + 3.0f);
    }

    // the walls: the face swept down to where the key meets the panel
    const auto walls = juce::Rectangle<float> (rest.getX(), face.getY(), rest.getWidth(), rest.getBottom() + depth - face.getY());
    juce::Colour sideTop = style == Style::Dark ? juce::Colour (0xff3f4045) : body.darker (0.28f);
    juce::Colour sideBottom = style == Style::Dark ? juce::Colour (0xff141416) : body.darker (0.85f);
    if (on)
    {
        sideTop = sideTop.interpolatedWith (lampColour, 0.35f);
        sideBottom = sideBottom.interpolatedWith (lampColour, 0.25f);
    }
    g.setGradientFill (juce::ColourGradient (sideTop, 0.0f, face.getBottom() - corner, sideBottom, 0.0f, walls.getBottom(), false));
    g.fillRoundedRectangle (walls, corner);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (walls.reduced (0.25f), corner, 0.6f);

    // the face
    const float scale = m::scaleOf (g);
    const int w = juce::roundToInt (face.getWidth() * scale), h = juce::roundToInt (face.getHeight() * scale);
    const juce::Colour top = on ? body.interpolatedWith (lampColour, 0.25f) : body;
    m::drawAt (g, m::cached ("key" + juce::String (w) + "x" + juce::String (h) + top.toString(), [&] { return m::keyFace (w, h, corner * scale, 1.6f * scale, top); }), face);
    if (on)
    {
        // the lamp behind the translucent cap
        juce::ColourGradient glow (lampColour.withAlpha (0.55f), face.getCentre(), lampColour.withAlpha (0.0f), face.getCentre().translated (face.getWidth() * 0.6f, 0), true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (face, corner);
    }
    if (glyph.isNotEmpty())
    {
        const juce::Colour ink = style == Style::Dark ? juce::Colour (0xffe8e8e8) : juce::Colour (0xff1a1a1a);
        g.setColour (ink);
        const auto c = face.getCentre();
        const float s = std::min (face.getWidth(), face.getHeight()) * 0.30f;
        juce::Path p;
        if (glyph == "stop")
            p.addRectangle (juce::Rectangle<float> (s * 1.3f, s * 1.3f).withCentre (c));
        else if (glyph == "play")
        {
            p.addTriangle (c.x - s * 1.6f, c.y - s, c.x - s * 1.6f, c.y + s, c.x - s * 0.2f, c.y);
            p.addRectangle (c.x + s * 0.25f, c.y - s, s * 0.45f, s * 2.0f);
            p.addRectangle (c.x + s * 1.0f, c.y - s, s * 0.45f, s * 2.0f);
        }
        else if (glyph == "rec")
            p.addEllipse (juce::Rectangle<float> (s * 1.4f, s * 1.4f).withCentre (c));
        g.fillPath (p);
    }
}

void PushKey::mouseDown (const juce::MouseEvent&)
{
    held = true;
    repaint();
    if (onToggle)
        onToggle();
    if (onDown)
        onDown();
}

void PushKey::mouseUp (const juce::MouseEvent&)
{
    held = false;
    repaint();
    if (onUp)
        onUp();
}

// ======================================================================== Led

void Led::refresh()
{
    const auto c = colour ? colour() : juce::Colours::transparentBlack;
    if (c != shown)
    {
        shown = c;
        repaint();
    }
}

void Led::paint (juce::Graphics& g)
{
    shown = colour ? colour() : juce::Colours::transparentBlack;
    const bool on = shown.getAlpha() > 0;
    const auto c = on ? shown : base;
    const float d = std::min ((float) getWidth(), (float) getHeight()) / 3.0f; // (the bounds leave room for the glow)
    auto r = juce::Rectangle<float> (d, d).withCentre (getLocalBounds().toFloat().getCentre());
    if (on)
    {
        juce::ColourGradient halo (c.withAlpha (0.45f), r.getCentre(), c.withAlpha (0.0f), r.getCentre().translated (d * 1.3f, 0), true);
        g.setGradientFill (halo);
        g.fillEllipse (r.expanded (d * 0.9f));
    }
    const float scale = m::scaleOf (g);
    const int px = juce::roundToInt (d * scale);
    m::drawAt (g, m::cached ("led" + juce::String (px) + c.toString() + (on ? "1" : "0"), [&] { return m::led (px, c, on); }), r);
}

// ======================================================================== Slide3

void Slide3::refresh()
{
    const int v = get ? get() : 0;
    if (v != shown)
    {
        shown = v;
        repaint();
    }
}

void Slide3::paint (juce::Graphics& g)
{
    shown = get ? get() : 0;
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    // the slot in a black plate, the white tab in one of three places
    g.setColour (juce::Colour (0xff0b0b0c));
    g.fillRoundedRectangle (b, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawRoundedRectangle (b, 2.0f, 1.0f);
    const float tabW = b.getWidth() / 3.0f;
    auto tab = juce::Rectangle<float> (b.getX() + tabW * (float) shown, b.getY(), tabW, b.getHeight()).reduced (1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (tab.translated (1.5f, 1.5f), 1.5f);
    juce::ColourGradient grad (juce::Colour (0xfff2f2f0), tab.getTopLeft(), juce::Colour (0xffb8b8b4), tab.getBottomLeft(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (tab, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = -1; i <= 1; ++i)
        g.drawVerticalLine (juce::roundToInt (tab.getCentreX() + (float) i * 2.5f), tab.getY() + 2.0f, tab.getBottom() - 2.0f);
}

void Slide3::mouseDown (const juce::MouseEvent& e)
{
    const int pos = juce::jlimit (0, 2, (int) ((float) e.x / ((float) getWidth() / 3.0f)));
    if (set)
        set (pos);
    refresh();
}

void Slide3::mouseDrag (const juce::MouseEvent& e) { mouseDown (e); }

// ======================================================================== LcdView

void LcdView::refresh()
{
    if (! render)
        return;
    lcd.clear();
    render (lcd);
    std::array<uint8_t, (size_t) (Lcd::W * Lcd::H)> now {};
    for (int y = 0; y < Lcd::H; ++y)
        for (int x = 0; x < Lcd::W; ++x)
            now[(size_t) (y * Lcd::W + x)] = lcd.get (x, y) ? 1 : 0;
    if (now != last)
    {
        last = now;
        repaint();
    }
}

void LcdView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // the backlight: pale blue, brighter in the middle; DIMMER LOW darkens it
    const float dimK = dim ? 0.72f : 1.0f;
    const juce::Colour back = juce::Colour::fromFloatRGBA (0.58f * dimK, 0.80f * dimK, 0.93f * dimK, 1.0f);
    juce::ColourGradient grad (back.brighter (0.12f), b.getCentre(), back.darker (0.18f), b.getTopLeft(), true);
    g.setGradientFill (grad);
    g.fillRect (b);
    // the dots: navy, darker with more contrast; the unlit ones faintly visible
    const float cw = b.getWidth() / (float) Lcd::W, ch = b.getHeight() / (float) Lcd::H;
    const float k = juce::jlimit (0.0f, 1.0f, contrast);
    const juce::Colour ink = juce::Colour::fromFloatRGBA (0.05f, 0.09f, 0.20f, 0.55f + 0.45f * k);
    g.setColour (back.darker (0.06f + 0.05f * k));
    for (int y = 0; y < Lcd::H; ++y)
        for (int x = 0; x < Lcd::W; ++x)
            if (last[(size_t) (y * Lcd::W + x)] == 0)
                g.fillRect (b.getX() + x * cw + cw * 0.12f, b.getY() + y * ch + ch * 0.12f, cw * 0.76f, ch * 0.76f);
    g.setColour (ink);
    for (int y = 0; y < Lcd::H; ++y)
        for (int x = 0; x < Lcd::W; ++x)
            if (last[(size_t) (y * Lcd::W + x)] != 0)
                g.fillRect (b.getX() + x * cw + cw * 0.06f, b.getY() + y * ch + ch * 0.06f, cw * 0.88f, ch * 0.88f);
    // the window: a soft inner shadow at the top-left and a faint sheen
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.28f), b.getTopLeft(), juce::Colours::transparentBlack, b.getTopLeft().translated (0, b.getHeight() * 0.18f), false);
    g.setGradientFill (shade);
    g.fillRect (b);
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.10f), b.getTopRight(), juce::Colours::transparentWhite, b.getCentre(), false);
    g.setGradientFill (sheen);
    g.fillRect (b);
}

// ======================================================================== MeterView

int MeterView::litSegments (float level)
{
    // dBFS thresholds for -30, -20, -10, -7, -4, -2, 0, 3, 6, 10, 15, OL
    static const float kDb[] = { -46.0f, -36.0f, -26.0f, -23.0f, -20.0f, -18.0f, -16.0f, -13.0f, -10.0f, -6.0f, -1.0f, -0.1f };
    const float db = juce::Decibels::gainToDecibels (level, -100.0f);
    int n = 0;
    for (float t : kDb)
        if (db >= t)
            ++n;
    return n;
}

void MeterView::setLevels (float left, float right)
{
    // a quick attack and a falling release, like the LEDs on the original
    const float lv[2] = { left, right };
    bool changed = false;
    for (int c = 0; c < 2; ++c)
    {
        held[(size_t) c] = std::max (lv[c], held[(size_t) c] * 0.80f);
        const int n = litSegments (held[(size_t) c]);
        if (n != lit[(size_t) c])
        {
            lit[(size_t) c] = n;
            changed = true;
        }
    }
    if (changed)
        repaint();
}

void MeterView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float rowH = b.getHeight() / 12.0f;
    const float colW = b.getWidth() * 0.28f;
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < 12; ++s)
        {
            const float x = c == 0 ? b.getX() : b.getRight() - colW;
            juce::Rectangle<float> seg (x, b.getY() + (11 - s) * rowH + rowH * 0.18f, colW, rowH * 0.64f);
            const juce::Colour col = s == 11 ? juce::Colour (0xffff3a2e) : s >= 8 ? juce::Colour (0xffffb21f) : juce::Colour (0xff7ee03a);
            const bool on = s < lit[(size_t) c];
            if (on)
            {
                g.setColour (col.withAlpha (0.35f));
                g.fillRect (seg.expanded (2.0f, 2.0f));
            }
            g.setColour (on ? col : col.withMultipliedBrightness (0.28f).withMultipliedSaturation (0.7f));
            g.fillRect (seg);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.25f : 0.06f));
            g.fillRect (seg.withHeight (seg.getHeight() * 0.35f));
        }
}

// ======================================================================== JogDial

juce::Rectangle<float> JogDial::knobArea() const
{
    const float d = std::min ((float) getWidth(), (float) getHeight());
    return juce::Rectangle<float> (d, d).withCentre (getLocalBounds().toFloat().getCentre()).reduced (d * 0.08f);
}

void JogDial::paint (juce::Graphics& g)
{
    // the knob (a white cap on a ribbed skirt) that turns with the angle; it dips a little when pushed
    const auto f = knobArea().translated (pressed ? 1.0f : 0.0f, pressed ? 1.5f : 0.0f);
    const float scale = m::scaleOf (g);
    const int px = juce::roundToInt (f.getWidth() * scale);
    const int step = ((int) std::lround (angle / juce::MathConstants<float>::twoPi * 72.0f) % 72 + 72) % 72;
    const float a = (float) step / 72.0f * juce::MathConstants<float>::twoPi;
    m::drawAt (g, m::cached ("jog" + juce::String (px) + "_" + juce::String (step), [&] {
                   return m::capKnob (px, a, juce::Colour (0xfff4f4f2), juce::Colour (0x00000000), juce::Colour (0xff111111));
               }),
               f);
}

void JogDial::mouseDown (const juce::MouseEvent& e)
{
    dragged = false;
    pressed = true;
    accum = 0.0f;
    lastAngle = std::atan2 ((float) e.x - getWidth() * 0.5f, getHeight() * 0.5f - (float) e.y);
    repaint();
}

void JogDial::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4 && ! dragged)
        return;
    if (! dragged)
    {
        dragged = true;
        pressed = false;
        repaint();
    }
    // turning: around the centre when far enough from it, otherwise up/down
    const float dx = (float) e.x - getWidth() * 0.5f, dy = getHeight() * 0.5f - (float) e.y;
    float delta;
    if (std::hypot (dx, dy) > getWidth() * 0.2f)
    {
        const float a = std::atan2 (dx, dy);
        delta = a - lastAngle;
        if (delta > juce::MathConstants<float>::pi)
            delta -= juce::MathConstants<float>::twoPi;
        if (delta < -juce::MathConstants<float>::pi)
            delta += juce::MathConstants<float>::twoPi;
        lastAngle = a;
    }
    else
    {
        delta = 0.0f; // (too close to the centre to tell the direction)
        lastAngle = std::atan2 (dx, dy);
    }
    angle += delta;
    accum += delta;
    const float detent = juce::MathConstants<float>::twoPi / 24.0f; // 24 clicks a turn
    while (accum >= detent)
    {
        accum -= detent;
        if (turn)
            turn (1);
    }
    while (accum <= -detent)
    {
        accum += detent;
        if (turn)
            turn (-1);
    }
    repaint();
}

void JogDial::mouseUp (const juce::MouseEvent&)
{
    if (! dragged && push)
        push();
    if (! dragged && unpush)
        unpush();
    pressed = false;
    repaint();
}

void JogDial::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    // a mouse wheel's notch is one click of the dial (towards you = clockwise); a trackpad accumulates
    const float v = (w.isReversed ? -w.deltaY : w.deltaY) - w.deltaX;
    if (std::abs (v) < 1.0e-4f)
        return;
    if (w.isSmooth)
        wheelAccum -= v * 14.0f;
    else
        wheelAccum += v < 0.0f ? 1.0f : -1.0f;
    while (wheelAccum >= 1.0f)
    {
        wheelAccum -= 1.0f;
        angle += juce::MathConstants<float>::twoPi / 24.0f;
        if (turn)
            turn (1);
    }
    while (wheelAccum <= -1.0f)
    {
        wheelAccum += 1.0f;
        angle -= juce::MathConstants<float>::twoPi / 24.0f;
        if (turn)
            turn (-1);
    }
    repaint();
}
} // namespace trackdesk::ui
