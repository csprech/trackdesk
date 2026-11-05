#include "Panel.h"

namespace trackdesk::ui
{
namespace m = materials;
using P = juce::Point<float>;
using R = juce::Rectangle<float>;
using J = juce::Justification;

namespace
{
    // ---- the panel's colours (from the product photographs)
    const juce::Colour kBlack { 0xff1c1d20 }, kSlate { 0xff36414f }, kGold { 0xffc6a762 }, kBlue { 0xff2580e3 }, kWhiteBand { 0xffeceae4 };
    const juce::Colour kPrint { 0xfff1f1ec }, kInk { 0xff141414 }, kRule { 0xffcfcfca }, kEqLine { 0xffbf9f55 };
    const juce::Colour kCapRed { 0xffc01e2c }, kCapGreen { 0xff12804a }, kCapBlue { 0xff6fb3e6 }, kCapOrange { 0xffeb9416 }, kCapWhite { 0xffe9e9e6 };
    const juce::Colour kPointer { 0xff151515 };
    const juce::Colour kLedGreen { 0xff36d65a }, kLedRed { 0xffff3326 }, kLedAmber { 0xffffb020 }, kLedWhite { 0xfff3f6ff };

    constexpr float kStripPitch = 106.6f, kStrip0 = 158.0f;
    constexpr float kFaderTop = 1170.0f, kFaderBottom = 1427.0f; // +10 and the infinity mark
    constexpr float kKnob = 52.0f;                                // the knobs' diameter on the canvas

    float faderY (float pos) { return kFaderBottom - (kFaderBottom - kFaderTop) * pos; }

    void tickRing (juce::Graphics& g, P c, float r, juce::Colour col, int ticks = 11, float len = 4.5f, float sweep = 0.83f)
    {
        g.setColour (col);
        for (int i = 0; i < ticks; ++i)
        {
            const float a = juce::MathConstants<float>::pi * (-sweep + 2.0f * sweep * (float) i / (float) (ticks - 1));
            const P d (std::sin (a), -std::cos (a));
            const float w = (i == 0 || i == ticks - 1 || i == ticks / 2) ? 1.6f : 1.1f;
            g.drawLine (c.x + d.x * r, c.y + d.y * r, c.x + d.x * (r + len), c.y + d.y * (r + len), w);
        }
    }

    void text (juce::Graphics& g, const juce::String& t, float cx, float cy, float h, juce::Colour c, bool condensed = false, float w = 120.0f)
    {
        drawText (g, t, R (w, h * 1.5f).withCentre ({ cx, cy }), h, c, J::centred, condensed);
    }
    void textL (juce::Graphics& g, const juce::String& t, float x, float cy, float h, juce::Colour c, bool condensed = false, float w = 160.0f)
    {
        drawText (g, t, R (x, cy - h * 0.75f, w, h * 1.5f), h, c, J::centredLeft, condensed);
    }
    void boxed (juce::Graphics& g, const juce::String& t, float cx, float cy, float h, juce::Colour box, juce::Colour ink, float padX = 4.0f)
    {
        const float w = juce::GlyphArrangement::getStringWidth (panelFont (h), t) + padX * 2.0f;
        g.setColour (box);
        g.fillRect (R (w, h + 4.0f).withCentre ({ cx, cy }));
        text (g, t, cx, cy, h, ink, false, w + 10.0f);
    }

    /** A fader's printed scale: ticks either side of the slot and the dB figures on the right. */
    void faderScale (juce::Graphics& g, float slotX, juce::Colour col, bool figures = true)
    {
        static const float kDb[] = { 10, 5, 0, -5, -10, -15, -20, -30, -40, -100 };
        static const char* kText[] = { "10", "5", "0", "5", "10", "15", "20", "30", "40", "\xe2\x88\x9e" };
        g.setColour (col);
        for (int i = 0; i < 10; ++i)
        {
            const float y = faderY (law::faderPosForDb (kDb[i]));
            const bool major = i == 0 || i == 2 || i == 4 || i == 9;
            g.drawLine (slotX - (major ? 30.0f : 26.0f), y, slotX - 14.0f, y, 1.3f);
            g.drawLine (slotX + 10.0f, y, slotX + 17.0f, y, 1.3f);
            if (figures)
                text (g, juce::String::fromUTF8 (kText[i]), slotX + 28.0f, y, 9.5f, col, true, 22.0f);
        }
        // the shorter ticks between, on the left as printed
        for (float db : { 7.5f, 2.5f, -2.5f, -7.5f, -12.5f, -17.5f, -25.0f, -35.0f })
        {
            const float y = faderY (law::faderPosForDb (db));
            g.drawLine (slotX - 22.0f, y, slotX - 14.0f, y, 1.0f);
        }
        if (figures)
            text (g, "dB", slotX + 27.0f, faderY (1.0f) - 12.0f, 8.5f, col, true, 22.0f);
    }

    void screwAt (juce::Graphics& g, P c, float d = 15.0f)
    {
        m::roundShadow (g, c, d * 0.5f, d * 0.12f, 0.8f);
        const float s = m::scaleOf (g);
        const int px = juce::roundToInt (d * s);
        juce::Image img = m::screw (px, 0.4f + c.x * 0.013f);
        // the panel screws are black
        juce::Image dark (juce::Image::ARGB, img.getWidth(), img.getHeight(), true);
        {
            juce::Graphics gi (dark);
            gi.drawImageAt (img, 0, 0);
            gi.setColour (juce::Colour (0xff101012).withAlpha (0.78f));
            gi.reduceClipRegion (img, {});
            gi.fillAll();
        }
        m::drawAt (g, dark, R (d, d).withCentre (c));
    }

    void jack (juce::Graphics& g, P c, float r)
    {
        // a 1/4" phone jack: a chrome hex nut and the dark hole
        m::roundShadow (g, c, r, r * 0.25f, 0.8f);
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::pi / 3.0f * (float) i + 0.26f;
            const P p (c.x + std::cos (a) * r, c.y + std::sin (a) * r);
            if (i == 0)
                hex.startNewSubPath (p);
            else
                hex.lineTo (p);
        }
        hex.closeSubPath();
        juce::ColourGradient chrome (juce::Colour (0xfff2f2f2), c.translated (-r, -r), juce::Colour (0xff6a6a6a), c.translated (r, r), false);
        chrome.addColour (0.5, juce::Colour (0xffb5b5b5));
        g.setGradientFill (chrome);
        g.fillPath (hex);
        g.setColour (juce::Colour (0xff2a2a2a));
        g.strokePath (hex, juce::PathStrokeType (1.0f));
        juce::ColourGradient ring (juce::Colour (0xff8a8a8a), c.translated (0, -r * 0.7f), juce::Colour (0xffe0e0e0), c.translated (0, r * 0.7f), false);
        g.setGradientFill (ring);
        g.fillEllipse (R (r * 1.3f, r * 1.3f).withCentre (c));
        g.setColour (juce::Colour (0xff050505));
        g.fillEllipse (R (r * 0.75f, r * 0.75f).withCentre (c));
    }
} // namespace

float stripX (int s) { return kStrip0 + kStripPitch * (float) s; }

// ======================================================================== building

Panel::Panel (Engine& e) : engine (e)
{
    setSize (kWidth, kHeight);
    setWantsKeyboardFocus (true);
    setOpaque (true);
    refreshNow();
    build();
    startTimerHz (30);
}

Panel::~Panel() { stopTimer(); }

Knob& Panel::addKnob (P c, float d, juce::Colour cap, float dflt, std::function<float()> get, std::function<void (float)> set)
{
    auto k = std::make_unique<Knob> (cap, kPointer, dflt);
    k->get = std::move (get);
    k->set = std::move (set);
    const float box = d / 0.80f;
    k->setBounds (R (box, box).withCentre (c.translated (d * 0.05f, d * 0.05f)).toNearestInt());
    auto* raw = k.get();
    knobs.push_back (raw);
    addAndMakeVisible (*k);
    parts.push_back (std::move (k));
    return *raw;
}

PushKey& Panel::addKey (P c, P size, PushKey::Style style, const juce::String& glyph)
{
    auto k = std::make_unique<PushKey> (style, glyph);
    k->setBounds (R (size.x + 6.0f, size.y + 8.0f).withCentre (c.translated (1.5f, 2.5f)).toNearestInt());
    auto* raw = k.get();
    keys.push_back (raw);
    addAndMakeVisible (*k);
    parts.push_back (std::move (k));
    return *raw;
}

Led& Panel::addLed (P c, float d, juce::Colour base, std::function<juce::Colour()> colour)
{
    auto l = std::make_unique<Led> (base);
    l->colour = std::move (colour);
    l->setBounds (R (d * 3.0f, d * 3.0f).withCentre (c).toNearestInt());
    auto* raw = l.get();
    leds.push_back (raw);
    addAndMakeVisible (*l);
    parts.push_back (std::move (l));
    return *raw;
}

Fader& Panel::addFader (float x, juce::Colour cap, std::function<float()> get, std::function<void (float)> set)
{
    auto f = std::make_unique<Fader> (cap);
    f->get = std::move (get);
    f->set = std::move (set);
    f->setBounds (R (x - 30.0f, kFaderTop - f->capH * 0.5f, 60.0f, kFaderBottom - kFaderTop + f->capH).toNearestInt());
    auto* raw = f.get();
    faders.push_back (raw);
    addAndMakeVisible (*f);
    parts.push_back (std::move (f));
    return *raw;
}

void Panel::build()
{
    auto led = [] (bool on, juce::Colour c) { return on ? c : juce::Colours::transparentBlack; };
    // ---------------------------------------------------------------- the channel strips
    for (int s = 0; s < kStrips; ++s)
    {
        const float cx = stripX (s), kx = cx - 6.0f;
        auto knob = [this, s, kx] (float y, juce::Colour cap, StripKnob k, float dflt) {
            auto& w = addKnob ({ kx, y }, kKnob, cap, dflt, [this, s, k] {
                    const auto& c = snap.strips[(size_t) s];
                    switch (k)
                    {
                        case StripKnob::Gain: return c.gain;
                        case StripKnob::Comp: return c.comp;
                        case StripKnob::EqHigh: return c.eqHigh;
                        case StripKnob::EqMidFreq: return c.eqMidFreq;
                        case StripKnob::EqMid: return c.eqMid;
                        case StripKnob::EqLow: return c.eqLow;
                        case StripKnob::Aux1: return c.aux1;
                        case StripKnob::Aux2: return c.aux2;
                        case StripKnob::Pan: return c.pan;
                        case StripKnob::Fader: return c.fader;
                    }
                    return 0.0f; }, [this, s, k] (float v) { edit ([&] (Desk& d) { d.setKnob (s, k, v); }); });
            return &w;
        };
        knob (228.0f, kCapRed, StripKnob::Gain, 0.2f);
        knob (440.0f, kCapGreen, StripKnob::Comp, 0.0f);
        knob (533.0f, kCapBlue, StripKnob::EqHigh, 0.5f);
        knob (607.0f, kCapBlue, StripKnob::EqMidFreq, 0.4f);
        knob (683.0f, kCapBlue, StripKnob::EqMid, 0.5f);
        knob (760.0f, kCapBlue, StripKnob::EqLow, 0.5f);
        knob (850.0f, kCapOrange, StripKnob::Aux1, 0.0f);
        knob (925.0f, kCapOrange, StripKnob::Aux2, 0.0f);
        knob (1015.0f, kCapWhite, StripKnob::Pan, 0.5f);

        auto sw = [this, s] (P c, P size, StripSwitch w) {
            auto& k = addKey (c, size, PushKey::Style::Light);
            k.latched = [this, s, w] {
                const auto& st = snap.strips[(size_t) s];
                switch (w)
                {
                    case StripSwitch::LowCut: return st.lowCut;
                    case StripSwitch::Inst: return st.inst;
                    case StripSwitch::Mute: return st.mute;
                    case StripSwitch::Main: return st.main;
                    case StripSwitch::Sub: return st.sub;
                    case StripSwitch::Solo: return st.solo;
                    case StripSwitch::Rec: return false; // (REC is a momentary key with its own lamp)
                }
                return false;
            };
            k.onToggle = [this, s, w] { edit ([&] (Desk& d) { d.setSwitch (s, w, ! d.getSwitch (s, w)); }); };
            return &k;
        };
        sw ({ cx - 27.0f, 291.0f }, { 31.0f, 21.0f }, StripSwitch::LowCut);
        sw ({ cx + 32.0f, 291.0f }, { 29.0f, 21.0f }, StripSwitch::Inst);
        sw ({ cx + 36.0f, 1162.0f }, { 28.0f, 22.0f }, StripSwitch::Mute);
        sw ({ cx + 36.0f, 1305.0f }, { 28.0f, 22.0f }, StripSwitch::Main);
        sw ({ cx + 36.0f, 1365.0f }, { 28.0f, 22.0f }, StripSwitch::Sub);
        sw ({ cx + 36.0f, 1428.0f }, { 28.0f, 22.0f }, StripSwitch::Solo);
        // REC: a momentary key; its lamp blinks when armed and lights while recording
        auto& rec = addKey ({ cx + 2.0f, 1085.0f }, { 44.0f, 24.0f }, PushKey::Style::Light);
        rec.onDown = [this, s] { edit ([&] (Desk& d) { d.setSwitch (s, StripSwitch::Rec, ! d.recKey[(size_t) s]); }); };
        rec.lampColour = juce::Colour (0xffff2a20);
        rec.lamp = [this, s] {
            const auto l = snap.lights.rec[(size_t) s];
            return l == Desk::Lamp::On || (l == Desk::Lamp::Blink && snap.lights.blinkPhase) ? 1 : 0;
        };
        // the lights
        addLed ({ cx + 37.0f, 186.0f }, 7.0f, kLedGreen, [this, s] {
            return snap.lights.sigRed[(size_t) s] ? kLedRed : snap.lights.sigGreen[(size_t) s] ? kLedGreen : juce::Colours::transparentBlack;
        });
        addLed ({ cx + 40.0f, 410.0f }, 7.0f, kLedGreen, [this, s, led] { return led (snap.lights.comp[(size_t) s], kLedGreen); });
        addLed ({ cx + 34.0f, 1131.0f }, 7.0f, kLedAmber, [this, s, led] { return led (snap.lights.mute[(size_t) s], kLedAmber); });
        // MODE
        auto slide = std::make_unique<Slide3>();
        slide->get = [this, s] { return snap.strips[(size_t) s].mode == Mode::Live ? 0 : snap.strips[(size_t) s].mode == Mode::Pc ? 1 : 2; };
        slide->set = [this, s] (int p) { edit ([&] (Desk& d) { d.setMode (s, p == 0 ? Mode::Live : p == 1 ? Mode::Pc : Mode::Mtr); }); };
        slide->setBounds (R (38.0f, 13.0f).withCentre ({ cx - 2.0f, 359.0f }).toNearestInt());
        slides.push_back (slide.get());
        addAndMakeVisible (*slide);
        parts.push_back (std::move (slide));
        // the fader
        addFader (cx - 19.0f, kCapWhite, [this, s] { return snap.strips[(size_t) s].fader; },
                  [this, s] (float v) { edit ([&] (Desk& d) { d.setKnob (s, StripKnob::Fader, v); }); });
    }
    // POST (shared by the channels: AUX 1 after the faders)
    addLed ({ 111.0f, 832.0f }, 6.0f, kLedAmber, [this, led] { return led (snap.lights.post, kLedAmber); });

    // ---------------------------------------------------------------- the master section
    auto mknob = [this] (P c, juce::Colour cap, MasterKnob k, float dflt, float diam = kKnob) {
        return &addKnob (c, diam, cap, dflt, [this, k] {
                const auto& mm = snap.master;
                switch (k)
                {
                    case MasterKnob::EqHigh: return mm.eqHigh;
                    case MasterKnob::EqMidFreq: return mm.eqMidFreq;
                    case MasterKnob::EqMid: return mm.eqMid;
                    case MasterKnob::EqLow: return mm.eqLow;
                    case MasterKnob::Aux1Out: return mm.aux1Out;
                    case MasterKnob::Aux2Out: return mm.aux2Out;
                    case MasterKnob::PflAfl: return mm.pflAfl;
                    case MasterKnob::Phones1: return mm.phones1;
                    case MasterKnob::Phones2: return mm.phones2;
                    case MasterKnob::FxToAux1: return mm.fxToAux1;
                    case MasterKnob::FxFader: return mm.fxFader;
                    case MasterKnob::SubFader: return mm.subFader;
                    case MasterKnob::MainFader: return mm.mainFader;
                }
                return 0.0f; }, [this, k] (float v) { edit ([&] (Desk& d) { d.setMasterKnob (k, v); }); });
    };
    mknob ({ 1012.0f, 225.0f }, kCapBlue, MasterKnob::EqHigh, 0.5f);
    mknob ({ 1012.0f, 305.0f }, kCapBlue, MasterKnob::EqLow, 0.5f);
    mknob ({ 1110.0f, 225.0f }, kCapBlue, MasterKnob::EqMidFreq, 0.4f);
    mknob ({ 1110.0f, 305.0f }, kCapBlue, MasterKnob::EqMid, 0.5f);
    mknob ({ 1010.0f, 850.0f }, kCapWhite, MasterKnob::Aux1Out, 0.75f);
    mknob ({ 1010.0f, 925.0f }, kCapWhite, MasterKnob::Aux2Out, 0.75f);
    mknob ({ 1010.0f, 1015.0f }, kCapWhite, MasterKnob::PflAfl, 0.75f);
    mknob ({ 1115.0f, 1018.0f }, kCapWhite, MasterKnob::Phones1, 0.5f);
    mknob ({ 1222.0f, 1018.0f }, kCapWhite, MasterKnob::Phones2, 0.5f);
    mknob ({ 1012.0f, 1200.0f }, kCapOrange, MasterKnob::FxToAux1, 0.0f);

    auto msw = [this] (P c, MasterSwitch w, P size = { 28.0f, 22.0f }) {
        auto& k = addKey (c, size, PushKey::Style::Light);
        k.latched = [this, w] {
            const auto& mm = snap.master;
            switch (w)
            {
                case MasterSwitch::EqNarrow: return mm.eqNarrow;
                case MasterSwitch::EqOnAux: return mm.eqOnAux;
                case MasterSwitch::EqIn: return mm.eqIn;
                case MasterSwitch::Phantom: return mm.phantom;
                case MasterSwitch::Aux1Mute: return mm.aux1Mute;
                case MasterSwitch::Aux1Afl: return mm.aux1Afl;
                case MasterSwitch::Aux2Mute: return mm.aux2Mute;
                case MasterSwitch::Aux2Afl: return mm.aux2Afl;
                case MasterSwitch::SdReturn: return mm.sdReturn;
                case MasterSwitch::Phones1Aux: return mm.phones1Aux;
                case MasterSwitch::Phones2Aux: return mm.phones2Aux;
                case MasterSwitch::FxMute: return mm.fxMute;
                case MasterSwitch::FxAux1: return mm.fxAux1;
                case MasterSwitch::FxMain: return mm.fxMain;
                case MasterSwitch::FxSub: return mm.fxSub;
                case MasterSwitch::FxAfl: return mm.fxAfl;
                case MasterSwitch::SubMute: return mm.subMute;
                case MasterSwitch::SubMain: return mm.subMain;
                case MasterSwitch::MainMute: return mm.mainMute;
                case MasterSwitch::PostRec: return mm.postRec;
            }
            return false;
        };
        k.onToggle = [this, w] { edit ([&] (Desk& d) { d.setMasterSwitch (w, ! d.getMasterSwitch (w)); }); };
        return &k;
    };
    msw ({ 1165.0f, 215.0f }, MasterSwitch::EqNarrow);
    msw ({ 1055.0f, 366.0f }, MasterSwitch::EqOnAux);
    msw ({ 1165.0f, 366.0f }, MasterSwitch::EqIn);
    msw ({ 1336.0f, 180.0f }, MasterSwitch::Phantom);
    msw ({ 1080.0f, 858.0f }, MasterSwitch::Aux1Mute);
    msw ({ 1137.0f, 858.0f }, MasterSwitch::Aux1Afl);
    msw ({ 1080.0f, 935.0f }, MasterSwitch::Aux2Mute);
    msw ({ 1137.0f, 935.0f }, MasterSwitch::Aux2Afl);
    msw ({ 1244.0f, 935.0f }, MasterSwitch::SdReturn);
    msw ({ 1137.0f, 1083.0f }, MasterSwitch::Phones1Aux);
    msw ({ 1244.0f, 1083.0f }, MasterSwitch::Phones2Aux);
    msw ({ 1308.0f, 1083.0f }, MasterSwitch::PostRec);
    msw ({ 1148.0f, 1162.0f }, MasterSwitch::FxMute);
    msw ({ 1148.0f, 1245.0f }, MasterSwitch::FxAux1);
    msw ({ 1148.0f, 1305.0f }, MasterSwitch::FxMain);
    msw ({ 1148.0f, 1365.0f }, MasterSwitch::FxSub);
    msw ({ 1148.0f, 1428.0f }, MasterSwitch::FxAfl);
    msw ({ 1255.0f, 1162.0f }, MasterSwitch::SubMute);
    msw ({ 1255.0f, 1305.0f }, MasterSwitch::SubMain);
    msw ({ 1360.0f, 1162.0f }, MasterSwitch::MainMute);

    addLed ({ 1300.0f, 180.0f }, 7.0f, kLedRed, [this, led] { return led (snap.lights.phantom, kLedRed); });
    addLed ({ 1080.0f, 826.0f }, 7.0f, kLedAmber, [this, led] { return led (snap.lights.aux1Mute, kLedAmber); });
    addLed ({ 1080.0f, 903.0f }, 7.0f, kLedAmber, [this, led] { return led (snap.lights.aux2Mute, kLedAmber); });
    addLed ({ 1206.0f, 940.0f }, 7.0f, kLedRed, [this, led] { return led (snap.master.sdReturn, kLedRed); });
    addLed ({ 1148.0f, 1131.0f }, 7.0f, kLedAmber, [this, led] { return led (snap.lights.fxMute, kLedAmber); });
    addLed ({ 1255.0f, 1131.0f }, 7.0f, kLedAmber, [this, led] { return led (snap.lights.subMute, kLedAmber); });
    addLed ({ 1360.0f, 1131.0f }, 7.0f, kLedAmber, [this, led] { return led (snap.lights.mainMute, kLedAmber); });
    addLed ({ 993.0f, 512.0f }, 6.0f, kLedWhite, [this, led] { return led (snap.lights.usb, kLedWhite); });
    addLed ({ 993.0f, 560.0f }, 6.0f, kLedAmber, [this, led] { return led (snap.lights.pflAfl, kLedAmber); });
    addLed ({ 1207.0f, 858.0f }, 7.0f, kLedGreen, [this] {
        const auto l = snap.lights.pairing;
        return l == Desk::Lamp::On || (l == Desk::Lamp::Blink && snap.lights.blinkPhase) ? kLedGreen : juce::Colours::transparentBlack;
    });
    addLed ({ 1310.0f, 970.0f }, 7.0f, kLedWhite, [this] {
        const auto l = snap.lights.tempo;
        return l == Desk::Lamp::Red ? kLedRed : l == Desk::Lamp::Green ? kLedGreen : juce::Colours::transparentBlack;
    });

    // the master faders
    addFader (1083.0f, kCapOrange, [this] { return snap.master.fxFader; }, [this] (float v) { edit ([&] (Desk& d) { d.setMasterKnob (MasterKnob::FxFader, v); }); });
    addFader (1200.0f, kCapWhite, [this] { return snap.master.subFader; }, [this] (float v) { edit ([&] (Desk& d) { d.setMasterKnob (MasterKnob::SubFader, v); }); });
    addFader (1305.0f, kCapRed, [this] { return snap.master.mainFader; }, [this] (float v) { edit ([&] (Desk& d) { d.setMasterKnob (MasterKnob::MainFader, v); }); });

    // ASSIGN (the paired device: 9/10 - OFF - MAIN)
    {
        auto slide = std::make_unique<Slide3>();
        slide->get = [this] { return snap.master.assign == Assign::To910 ? 0 : snap.master.assign == Assign::Off ? 1 : 2; };
        slide->set = [this] (int p) { edit ([&] (Desk& d) { d.setAssign (p == 0 ? Assign::To910 : p == 1 ? Assign::Off : Assign::ToMain); }); };
        slide->setBounds (R (44.0f, 14.0f).withCentre ({ 1330.0f, 861.0f }).toNearestInt());
        slides.push_back (slide.get());
        addAndMakeVisible (*slide);
        parts.push_back (std::move (slide));
    }

    // ---------------------------------------------------------------- momentary keys
    auto momentary = [this] (P c, P size, PushKey::Style st, Key k, const juce::String& glyph = {}) {
        auto& key = addKey (c, size, st, glyph);
        key.onDown = [this, k] { pressKey (k); };
        key.onUp = [this, k] { releaseKey (k); };
        momentaries.push_back ({ &key, k });
        return &key;
    };
    momentary ({ 1015.0f, 643.0f }, { 46.0f, 22.0f }, PushKey::Style::Dark, Key::F1);
    momentary ({ 1085.0f, 643.0f }, { 46.0f, 22.0f }, PushKey::Style::Dark, Key::F2);
    momentary ({ 1152.0f, 643.0f }, { 46.0f, 22.0f }, PushKey::Style::Dark, Key::F3);
    momentary ({ 1222.0f, 643.0f }, { 46.0f, 22.0f }, PushKey::Style::Dark, Key::F4);
    momentary ({ 1152.0f, 708.0f }, { 46.0f, 20.0f }, PushKey::Style::Dark, Key::Menu);
    momentary ({ 1255.0f, 708.0f }, { 46.0f, 20.0f }, PushKey::Style::Dark, Key::Rew);
    momentary ({ 1322.0f, 708.0f }, { 46.0f, 20.0f }, PushKey::Style::Dark, Key::Ff);
    auto* stop = momentary ({ 1167.0f, 760.0f }, { 50.0f, 32.0f }, PushKey::Style::Transport, Key::Stop, "stop");
    stop->lampColour = juce::Colour (0xffffffff);
    stop->lamp = [this] { return snap.lights.stop == Desk::Lamp::On ? 1 : 0; };
    auto* play = momentary ({ 1255.0f, 760.0f }, { 46.0f, 32.0f }, PushKey::Style::Transport, Key::Play, "play");
    play->lampColour = juce::Colour (0xff42e06a);
    play->lamp = [this] {
        const auto l = snap.lights.play;
        return l == Desk::Lamp::On || (l == Desk::Lamp::Blink && snap.lights.blinkPhase) ? 1 : 0;
    };
    auto* recKey = momentary ({ 1322.0f, 760.0f }, { 46.0f, 32.0f }, PushKey::Style::Transport, Key::Record, "rec");
    recKey->lampColour = juce::Colour (0xffff3326);
    recKey->lamp = [this] {
        const auto l = snap.lights.record;
        return l == Desk::Lamp::On || (l == Desk::Lamp::Blink && snap.lights.blinkPhase) ? 1 : 0;
    };
    momentary ({ 1010.0f, 1265.0f }, { 28.0f, 22.0f }, PushKey::Style::Light, Key::FxSelect);
    momentary ({ 1360.0f, 970.0f }, { 28.0f, 22.0f }, PushKey::Style::Light, Key::Click);
    momentary ({ 1244.0f, 858.0f }, { 28.0f, 22.0f }, PushKey::Style::Light, Key::Pairing);

    // ---------------------------------------------------------------- the display, the meter, the dial
    auto lcdView = std::make_unique<LcdView>();
    lcdView->setBounds (R (206.0f, 103.0f).withCentre ({ 1132.0f, 537.0f }).toNearestInt());
    lcd = lcdView.get();
    addAndMakeVisible (*lcdView);
    parts.push_back (std::move (lcdView));
    auto meterView = std::make_unique<MeterView>();
    meterView->setBounds (R (1274.0f, 463.0f, 78.0f, 180.0f).toNearestInt());
    meter = meterView.get();
    addAndMakeVisible (*meterView);
    parts.push_back (std::move (meterView));
    auto dial = std::make_unique<JogDial>();
    dial->setBounds (R (70.0f, 70.0f).withCentre ({ 1046.0f, 745.0f }).toNearestInt());
    dial->turn = [this] (int d) {
        engine.withDesk ([d] (Desk& desk) { desk.turnJog (d); });
        refreshNow();
    };
    dial->push = [this] { pressKey (Key::Jog); };
    dial->unpush = [this] { releaseKey (Key::Jog); };
    jog = dial.get();
    addAndMakeVisible (*dial);
    parts.push_back (std::move (dial));
    lcd->render = [] (Lcd&) {}; // (the timer draws it from the snapshot)
}

// ======================================================================== the console's state

void Panel::refreshNow()
{
    Lcd frame;
    engine.withDesk ([&] (Desk& d) {
        snap.strips = d.strips;
        snap.master = d.master;
        snap.rec = d.recKey;
        snap.lights = d.lights();
        snap.meters = d.takeMeters();
        snap.contrast = d.displayContrast();
        snap.dim = ! d.settings.dimmerHigh;
        d.render (frame);
    });
    if (lcd != nullptr)
    {
        lcd->render = [frame] (Lcd& l) { l = frame; };
        lcd->contrast = snap.contrast;
        lcd->dim = snap.dim;
        lcd->refresh();
    }
    if (meter != nullptr)
        meter->setLevels (snap.meters.mainL, snap.meters.mainR);
    for (auto* k : knobs)
        k->refresh();
    for (auto* k : keys)
        k->refresh();
    for (auto* l : leds)
        l->refresh();
    for (auto* f : faders)
        f->refresh();
    for (auto* s : slides)
        s->refresh();
}

void Panel::timerCallback() { refreshNow(); }

std::pair<juce::StringArray, juce::StringArray> Panel::selfTestControls()
{
    juce::StringArray ok, bad;
    // knobs and faders: each sets the console and reads back
    int i = 0;
    for (auto* k : knobs)
    {
        const float before = k->get();
        k->set (0.37f);
        refreshNow();
        if (std::abs (k->get() - 0.37f) > 1.0e-4f)
            bad.add ("knob " + juce::String (i) + " didn't reach the console");
        k->set (before);
        ++i;
    }
    ok.add ("ok    " + juce::String ((int) knobs.size()) + " knobs");
    i = 0;
    for (auto* f : faders)
    {
        const float before = f->get();
        f->set (0.61f);
        refreshNow();
        if (std::abs (f->get() - 0.61f) > 1.0e-4f)
            bad.add ("fader " + juce::String (i) + " didn't reach the console");
        f->set (before);
        ++i;
    }
    ok.add ("ok    " + juce::String ((int) faders.size()) + " faders");
    // the latching switches: on and back off
    int switches = 0;
    for (auto* k : keys)
        if (k->onToggle && k->latched)
        {
            const bool before = k->latched();
            k->onToggle();
            refreshNow();
            if (k->latched() == before)
                bad.add ("switch " + juce::String (switches) + " didn't change");
            k->onToggle();
            refreshNow();
            if (k->latched() != before)
                bad.add ("switch " + juce::String (switches) + " didn't change back");
            ++switches;
        }
    engine.withDesk ([] (Desk& d) {
        if (d.popupOpen())
        {
            d.press (Key::F1);
            d.release (Key::F1);
        }
    });
    ok.add ("ok    " + juce::String (switches) + " switches");
    // the REC keys
    for (int s = 0; s < kStrips; ++s)
    {
        edit ([s] (Desk& d) { d.setSwitch (s, StripSwitch::Rec, true); });
        refreshNow();
        if (! snap.rec[(size_t) s] || snap.lights.rec[(size_t) s] != Desk::Lamp::Blink)
            bad.add ("REC " + stripName (s) + " doesn't arm");
        edit ([s] (Desk& d) { d.setSwitch (s, StripSwitch::Rec, false); });
    }
    ok.add ("ok    8 REC keys");
    // the slide switches: each position
    i = 0;
    for (auto* sl : slides)
    {
        const int before = sl->get();
        for (int p = 0; p < 3; ++p)
        {
            sl->set (p);
            refreshNow();
            if (sl->get() != p)
                bad.add ("slide switch " + juce::String (i) + " didn't reach position " + juce::String (p));
        }
        sl->set (before);
        ++i;
    }
    ok.add ("ok    " + juce::String ((int) slides.size()) + " slide switches");
    // the momentary keys reach the console: each one is down while pressed
    for (auto& [w, k] : momentaries)
    {
        bool downDuring = false, downAfter = true;
        w->onDown();
        engine.withDesk ([&] (Desk& d) { downDuring = d.isDown (k); });
        w->onUp();
        engine.withDesk ([&] (Desk& d) { downAfter = d.isDown (k); });
        if (! downDuring || downAfter)
            bad.add ("a momentary key doesn't press and release (key " + juce::String ((int) k) + ")");
        // put things back: out of any screen it opened, the metronome off, the transport stopped
        engine.withDesk ([] (Desk& d) {
            if (d.popupOpen())
            {
                d.press (Key::F1);
                d.release (Key::F1);
            }
            if (d.clickRunning())
            {
                d.press (Key::Click);
                d.release (Key::Click);
            }
            d.press (Key::Stop);
            d.release (Key::Stop);
            d.takePairingRequest();
        });
    }
    ok.add ("ok    " + juce::String ((int) momentaries.size()) + " keys (F1-F4, MENU, transport, SELECT, CLICK, PAIRING)");
    // the display follows MENU, and the dial moves the counter on the Home Screen
    engine.withDesk ([] (Desk& d) {
        while (d.screen() != Desk::ScreenId::Home)
        {
            d.press (Key::Menu);
            d.release (Key::Menu);
        }
        d.recorder.locate (0);
    });
    jog->turn (3);
    int64_t pos = 0;
    juce::String shown;
    engine.withDesk ([&] (Desk& d) {
        pos = d.recorder.getPosition();
        Lcd l;
        d.render (l);
        shown = l.allText();
    });
    if (pos != (int64_t) (3.0 * engine.withDesk ([] (Desk& d) { return d.getSampleRate(); })))
        bad.add ("the MULTI JOG didn't move the counter");
    else
        ok.add ("ok    MULTI JOG moves the counter");
    if (! shown.contains ("00") || ! shown.contains ("METR"))
        bad.add ("the Home Screen isn't showing: " + shown);
    else
        ok.add ("ok    the Home Screen shows (" + shown.substring (0, 40) + "...)");
    refreshNow();
    return { ok, bad };
}

void Panel::pressKey (Key k)
{
    engine.withDesk ([k] (Desk& d) { d.press (k); });
    refreshNow();
}

void Panel::releaseKey (Key k)
{
    engine.withDesk ([k] (Desk& d) { d.release (k); });
    refreshNow();
}

bool Panel::keyPressed (const juce::KeyPress& key)
{
    // the computer keyboard: \ is the footswitch; the arrows and return turn and push the dial
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey || code == juce::KeyPress::downKey)
    {
        const int d = (code == juce::KeyPress::leftKey || code == juce::KeyPress::upKey) ? -1 : 1;
        engine.withDesk ([d] (Desk& desk) { desk.turnJog (d); });
        refreshNow();
        return true;
    }
    if (code == juce::KeyPress::returnKey)
    {
        pressKey (Key::Jog);
        releaseKey (Key::Jog);
        return true;
    }
    if (code == juce::KeyPress::spaceKey)
    {
        pressKey (Key::Play);
        releaseKey (Key::Play);
        return true;
    }
    if (key.getTextCharacter() == '\\')
        return true; // (handled in keyStateChanged: held and released)
    return false;
}

bool Panel::keyStateChanged (bool)
{
    const bool down = juce::KeyPress::isKeyCurrentlyDown ('\\');
    if (down != footHeld)
    {
        footHeld = down;
        if (down)
            pressKey (Key::Foot1);
        else
            releaseKey (Key::Foot1);
        return true;
    }
    return false;
}

// ======================================================================== painting

void Panel::paint (juce::Graphics& g)
{
    const float s = m::scaleOf (g);
    if (background.isNull() || std::abs (s - backgroundScale) > 0.01f)
    {
        backgroundScale = s;
        background = juce::Image (juce::Image::ARGB, juce::roundToInt (kWidth * s), juce::roundToInt (kHeight * s), true);
        juce::Graphics bg (background);
        bg.addTransform (juce::AffineTransform::scale (s));
        paintBackground (bg);
    }
    g.drawImage (background, getLocalBounds().toFloat());
}

void Panel::paintBackground (juce::Graphics& g)
{
    const float s = m::scaleOf (g);
    // ---------------------------------------------------------------- the desk it sits on, the chassis, the cheeks
    g.fillAll (juce::Colour (0xff0d0d0e));
    const R chassis (14.0f, 32.0f, 1460.0f, 1532.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (chassis.translated (6.0f, 10.0f).expanded (4.0f), 18.0f);
    g.setColour (juce::Colour (0xff131315));
    g.fillRoundedRectangle (chassis, 14.0f);
    for (int side = 0; side < 2; ++side)
    {
        // the walnut cheeks, end grain rounded at the front and back
        const R cheek = side == 0 ? R (14.0f, 32.0f, 62.0f, 1532.0f) : R (1412.0f, 32.0f, 62.0f, 1532.0f);
        const int w = juce::roundToInt (cheek.getWidth() * s), h = juce::roundToInt (cheek.getHeight() * s);
        auto wood = m::cached ("cheek" + juce::String (side) + "_" + juce::String (w) + "x" + juce::String (h), [&] { return m::woodTile (w, h, 41u + (uint32_t) side); });
        juce::Path p;
        p.addRoundedRectangle (cheek, 12.0f);
        g.saveState();
        g.reduceClipRegion (p);
        m::drawAt (g, wood, cheek);
        // rounded over: lit edge on the left, shade on the right; a darker oiled end at each end
        juce::ColourGradient round (juce::Colours::white.withAlpha (0.16f), cheek.getTopLeft(), juce::Colours::black.withAlpha (0.35f), cheek.getTopRight(), false);
        round.addColour (0.35, juce::Colours::transparentWhite);
        round.addColour (0.7, juce::Colours::transparentBlack);
        g.setGradientFill (round);
        g.fillRect (cheek);
        g.restoreState();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (cheek, 12.0f, 1.2f);
    }
    // the steel top panel between the cheeks, its front and back lips
    const R panel (78.0f, 36.0f, 1332.0f, 1520.0f);
    {
        const int tile = 120;
        auto paint = m::cached ("paintTile_black_" + juce::String (juce::roundToInt (tile * s)), [&] { return m::paintTile (kBlack, juce::roundToInt (tile * s), 0.7f, 5u); });
        g.setFillType (juce::FillType (paint, juce::AffineTransform::scale (1.0f / s)));
        g.fillRect (panel);
        g.setColour (kBlack);
        g.setOpacity (0.55f);
        g.fillRect (panel);
        g.setOpacity (1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawLine (panel.getX(), panel.getY() + 0.5f, panel.getRight(), panel.getY() + 0.5f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (panel.getX(), panel.getBottom() - 0.5f, panel.getRight(), panel.getBottom() - 0.5f, 1.5f);
    }

    // ---------------------------------------------------------------- coloured areas
    auto area = [&] (R r, juce::Colour c, float corner = 3.0f) {
        g.setColour (c);
        g.fillRoundedRectangle (r, corner);
        g.setColour (c.brighter (0.12f).withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    };
    area (R (103.0f, 1063.0f, 853.0f, 452.0f), kSlate, 4.0f); // the channel faders
    area (R (965.0f, 140.0f, 222.0f, 253.0f), kGold);         // MASTER SECTION EQ
    area (R (1192.0f, 140.0f, 78.0f, 253.0f), kGold);         // PHONES
    area (R (1275.0f, 140.0f, 107.0f, 98.0f), kGold);         // PHANTOM
    area (R (1275.0f, 242.0f, 107.0f, 151.0f), kGold);        // the card slot
    area (R (1176.0f, 811.0f, 206.0f, 84.0f), kBlue, 6.0f);   // the paired device
    area (R (1276.0f, 902.0f, 106.0f, 113.0f), kGold);        // CLICK
    area (R (1276.0f, 1020.0f, 106.0f, 492.0f), kGold);       // POST REC / MAIN
    area (R (965.0f, 1110.0f, 204.0f, 402.0f), kGold);        // FX
    area (R (1172.0f, 1110.0f, 101.0f, 402.0f), kGold);       // SUB
    // the white MODE bands and the scribble strips
    for (int st = 0; st < kStrips; ++st)
    {
        const float cx = stripX (st);
        g.setColour (kWhiteBand);
        g.fillRect (R (cx - 51.0f, 337.0f, 102.0f, 58.0f));
        g.setColour (juce::Colour (0xfff7f7f3));
        g.fillRect (R (cx - 48.0f, 138.0f, 96.0f, 23.0f));
    }

    // ---------------------------------------------------------------- rules between the strips and sections
    g.setColour (kRule);
    for (int st = 1; st < kStrips; ++st)
    {
        const float x = stripX (st) - kStripPitch * 0.5f;
        g.drawLine (x, 136.0f, x, 330.0f, 1.3f);
        g.drawLine (x, 400.0f, x, 1060.0f, 1.3f);
    }
    g.setColour (kRule.withAlpha (0.9f));
    for (int st = 1; st < kStrips; ++st)
    {
        const float x = stripX (st) - kStripPitch * 0.5f;
        g.drawLine (x, 1066.0f, x, 1512.0f, 1.3f);
    }
    g.setColour (kRule.withAlpha (0.55f));
    g.drawLine (104.0f, 331.0f, 955.0f, 331.0f, 1.2f);
    g.drawLine (104.0f, 970.0f, 955.0f, 970.0f, 1.2f);
    // the EQ section's gold outline (the original's thin line)
    g.setColour (kEqLine);
    g.drawRoundedRectangle (R (104.0f, 487.0f, 851.0f, 318.0f), 5.0f, 1.6f);
    // the AUX group bracket per strip
    for (int st = 0; st < kStrips; ++st)
    {
        const float cx = stripX (st);
        g.setColour (kPrint);
        g.drawLine (cx - 48.0f, 822.0f, cx - 48.0f, 932.0f, 1.0f);
        g.drawLine (cx - 48.0f, 822.0f, cx - 40.0f, 822.0f, 1.0f);
        // MID bracket
        g.drawLine (cx - 46.0f, 590.0f, cx - 46.0f, 690.0f, 1.0f);
        g.drawLine (cx - 46.0f, 590.0f, cx - 40.0f, 590.0f, 1.0f);
        g.drawLine (cx - 46.0f, 690.0f, cx - 40.0f, 690.0f, 1.0f);
    }

    // ---------------------------------------------------------------- channel strip printing
    for (int st = 0; st < kStrips; ++st)
    {
        const float cx = stripX (st), kx = cx - 6.0f;
        const bool stereo = isStereo (st);
        // GAIN: two scales, LINE outside and MIC inside
        textL (g, "GAIN", cx - 50.0f, 181.0f, 11.0f, kPrint);
        text (g, "LINE", kx, 188.0f, 7.5f, kPrint, true);
        text (g, "MIC", kx, 197.0f, 7.5f, kPrint, true);
        tickRing (g, { kx, 228.0f }, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
        g.setColour (kPrint);
        g.drawEllipse (R (kKnob + 22.0f, kKnob + 22.0f).withCentre ({ kx, 228.0f }), 0.8f);
        text (g, stereo ? "-20" : "-10", kx - 36.0f, 258.0f, 8.5f, kPrint, true, 30.0f);
        text (g, "0", kx - 20.0f, 258.0f, 8.5f, kPrint, true, 20.0f);
        text (g, "dB", kx, 259.0f, 8.0f, kPrint, true, 20.0f);
        text (g, stereo ? "+50+30" : "+50+40", kx + 34.0f, 258.0f, 8.5f, kPrint, true, 44.0f);
        text (g, "SIG", cx + 37.0f, 199.0f, 7.5f, kPrint, true, 30.0f);
        text (g, "LOW CUT", cx - 27.0f, 313.0f, 9.0f, kPrint, true, 50.0f);
        text (g, "100Hz", cx - 34.0f, 324.0f, 7.5f, kPrint, true, 40.0f);
        text (g, "INST", cx + 32.0f, 313.0f, 9.0f, kPrint, true, 40.0f);
        // MODE (black on the white band)
        text (g, "MODE", cx - 2.0f, 343.0f, 9.5f, kInk, false, 60.0f);
        text (g, "LIVE", cx - 37.0f, 359.0f, 9.0f, kInk, true, 30.0f);
        text (g, "MTR", cx + 35.0f, 359.0f, 9.0f, kInk, true, 30.0f);
        text (g, "DIRECT", cx - 30.0f, 375.0f, 8.0f, kInk, true, 40.0f);
        text (g, "PC", cx - 2.0f, 375.0f, 8.0f, kInk, true, 20.0f);
        g.setColour (kInk);
        g.drawLine (cx - 40.0f, 381.0f, cx - 40.0f, 387.0f, 0.9f);
        g.drawLine (cx - 40.0f, 387.0f, cx - 22.0f, 387.0f, 0.9f);
        g.drawLine (cx + 6.0f, 387.0f, cx + 16.0f, 387.0f, 0.9f);
        g.drawLine (cx + 16.0f, 381.0f, cx + 16.0f, 387.0f, 0.9f);
        text (g, "USB", cx - 8.0f, 387.0f, 7.5f, kInk, true, 30.0f);
        if (st == 7)
            text (g, "*", cx - 46.0f, 343.0f, 10.0f, kInk); // (the paired device comes in here)
        // COMP
        textL (g, "COMP", cx - 50.0f, 409.0f, 10.0f, kPrint);
        tickRing (g, { kx, 440.0f }, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
        text (g, "OFF", kx - 22.0f, 472.0f, 8.0f, kPrint, true, 30.0f);
        text (g, "MAX", kx + 22.0f, 472.0f, 8.0f, kPrint, true, 30.0f);
        // EQ
        textL (g, "EQ", cx - 50.0f, 498.0f, 10.0f, kPrint);
        const float eqY[] = { 533.0f, 607.0f, 683.0f, 760.0f };
        for (int i = 0; i < 4; ++i)
        {
            tickRing (g, { kx, eqY[i] }, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
            if (i == 1)
            {
                text (g, "600", kx, eqY[i] - 30.0f, 8.0f, kPrint, true, 30.0f);
                text (g, "100", kx - 24.0f, eqY[i] + 30.0f, 8.0f, kPrint, true, 30.0f);
                text (g, "Hz", kx, eqY[i] + 31.0f, 7.5f, kPrint, true, 20.0f);
                text (g, "8k", kx + 22.0f, eqY[i] + 30.0f, 8.0f, kPrint, true, 20.0f);
            }
            else
            {
                text (g, "0", kx, eqY[i] - 30.0f, 8.0f, kPrint, true, 20.0f);
                text (g, "-15", kx - 24.0f, eqY[i] + 30.0f, 8.0f, kPrint, true, 30.0f);
                text (g, "dB", kx, eqY[i] + 31.0f, 7.5f, kPrint, true, 20.0f);
                text (g, "+15", kx + 22.0f, eqY[i] + 30.0f, 8.0f, kPrint, true, 30.0f);
            }
        }
        text (g, "10kHz", cx + 33.0f, 502.0f, 8.0f, kPrint, true, 40.0f);
        text (g, "80Hz", cx + 34.0f, 731.0f, 8.0f, kPrint, true, 40.0f);
        textL (g, "MID", cx - 42.0f, 651.0f, 9.0f, kPrint);
        // AUX
        textL (g, "AUX", cx - 44.0f, 817.0f, 9.5f, kPrint);
        text (g, "AUX 1", cx + 33.0f, 820.0f, 8.0f, kPrint, true, 40.0f);
        text (g, "AUX 2/", cx + 33.0f, 896.0f, 8.0f, kPrint, true, 40.0f);
        text (g, "FX", cx + 38.0f, 905.0f, 8.0f, kPrint, true, 30.0f);
        for (float y : { 850.0f, 925.0f })
        {
            tickRing (g, { kx, y }, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
            text (g, juce::String::fromUTF8 ("\xe2\x88\x9e"), kx - 22.0f, y + 30.0f, 10.0f, kPrint, false, 20.0f);
            text (g, "dB", kx, y + 31.0f, 7.5f, kPrint, true, 20.0f);
            text (g, "+15", kx + 22.0f, y + 30.0f, 8.0f, kPrint, true, 30.0f);
        }
        // PAN / BAL
        textL (g, "PAN", cx - 50.0f, 983.0f, 9.5f, kPrint);
        text (g, "C", kx, 984.0f, 8.5f, kPrint, false, 20.0f);
        if (stereo)
            text (g, "BAL", cx + 34.0f, 983.0f, 9.0f, kPrint, false, 40.0f);
        tickRing (g, { kx, 1015.0f }, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
        text (g, "L", kx - 20.0f, 1047.0f, 8.5f, kPrint, false, 20.0f);
        text (g, "R", kx + 20.0f, 1047.0f, 8.5f, kPrint, false, 20.0f);
        // the fader area (white on slate)
        text (g, "REC", cx + 2.0f, 1111.0f, 9.0f, kPrint, false, 40.0f);
        text (g, "MUTE", cx + 36.0f, 1187.0f, 9.0f, kPrint, false, 50.0f);
        text (g, "MAIN", cx + 36.0f, 1329.0f, 9.0f, kPrint, false, 50.0f);
        text (g, "SUB", cx + 36.0f, 1389.0f, 9.0f, kPrint, false, 50.0f);
        text (g, "SOLO", cx + 36.0f, 1452.0f, 9.0f, kPrint, false, 50.0f);
        faderScale (g, cx - 19.0f, kPrint);
        text (g, stripName (st), cx, 1491.0f, 19.0f, kPrint, false, 80.0f);
    }
    text (g, "POST", 112.0f, 846.0f, 7.0f, kPrint, true, 40.0f);
    text (g, "CH1-10", 113.0f, 855.0f, 7.0f, kPrint, true, 40.0f);
    for (P sc : { P (211.0f, 228.0f), P (637.0f, 228.0f), P (120.0f, 577.0f), P (548.0f, 577.0f), P (858.0f, 577.0f), P (120.0f, 896.0f),
                  P (548.0f, 896.0f), P (858.0f, 896.0f), P (620.0f, 1469.0f), P (936.0f, 1469.0f) })
        screwAt (g, sc);

    // ---------------------------------------------------------------- MASTER SECTION EQ, PHONES, PHANTOM, the card slot
    text (g, "MASTER SECTION EQ", 1076.0f, 154.0f, 10.5f, kInk, false, 200.0f);
    g.setColour (kInk);
    g.drawLine (970.0f, 154.0f, 1005.0f, 154.0f, 1.0f);
    g.drawLine (1148.0f, 154.0f, 1182.0f, 154.0f, 1.0f);
    textL (g, "HIGH", 970.0f, 193.0f, 9.0f, kInk);
    text (g, "10kHz", 1040.0f, 193.0f, 8.0f, kInk, true, 40.0f);
    textL (g, "LOW", 970.0f, 273.0f, 9.0f, kInk);
    text (g, "60Hz", 1040.0f, 273.0f, 8.0f, kInk, true, 40.0f);
    textL (g, "MID", 1080.0f, 193.0f, 9.0f, kInk);
    for (P c : { P (1012.0f, 225.0f), P (1012.0f, 305.0f), P (1110.0f, 225.0f), P (1110.0f, 305.0f) })
    {
        tickRing (g, c, kKnob * 0.5f + 4.0f, kInk, 11, 4.0f);
        const bool freq = c.x > 1100.0f && c.y < 260.0f;
        text (g, freq ? "800" : "0", c.x, c.y - 30.0f, 8.0f, kInk, true, 30.0f);
        text (g, freq ? "100" : "-15", c.x - 24.0f, c.y + 31.0f, 8.0f, kInk, true, 30.0f);
        text (g, freq ? "Hz" : "dB", c.x, c.y + 32.0f, 7.5f, kInk, true, 20.0f);
        text (g, freq ? "8k" : "+15", c.x + 22.0f, c.y + 31.0f, 8.0f, kInk, true, 30.0f);
    }
    g.setColour (kInk);
    g.drawLine (1080.0f, 212.0f, 1080.0f, 318.0f, 1.0f);
    // NARROW / WIDE icons, AUX 1/2 / MAIN MIX, EQ IN / BYPASS
    auto upIcon = [&] (float x, float y, bool in) {
        g.setColour (kInk);
        g.drawLine (x, y + 4.0f, x + 3.0f, y + 4.0f, 1.0f);
        g.drawLine (x + 3.0f, y + 4.0f, x + 3.0f, in ? y + 1.0f : y - 2.0f, 1.0f);
        g.drawLine (x + 3.0f, in ? y + 1.0f : y - 2.0f, x + 8.0f, in ? y + 1.0f : y - 2.0f, 1.0f);
        g.drawLine (x + 8.0f, in ? y + 1.0f : y - 2.0f, x + 8.0f, y + 4.0f, 1.0f);
        g.drawLine (x + 8.0f, y + 4.0f, x + 11.0f, y + 4.0f, 1.0f);
    };
    upIcon (1140.0f, 240.0f, false);
    text (g, "WIDE", 1172.0f, 242.0f, 7.5f, kInk, true, 30.0f);
    upIcon (1140.0f, 254.0f, true);
    text (g, "NARROW", 1176.0f, 256.0f, 7.5f, kInk, true, 34.0f);
    upIcon (972.0f, 357.0f, false);
    textL (g, "AUX 1/2", 986.0f, 359.0f, 8.0f, kInk, true);
    upIcon (972.0f, 371.0f, true);
    textL (g, "MAIN MIX", 986.0f, 373.0f, 8.0f, kInk, true);
    upIcon (1092.0f, 357.0f, false);
    textL (g, "EQ IN", 1106.0f, 359.0f, 8.0f, kInk, true);
    upIcon (1092.0f, 371.0f, true);
    textL (g, "BYPASS", 1106.0f, 373.0f, 8.0f, kInk, true);
    screwAt (g, { 1060.0f, 205.0f }, 13.0f);
    text (g, "PHONES", 1231.0f, 155.0f, 9.5f, kInk, false, 70.0f);
    jack (g, { 1231.0f, 215.0f }, 25.0f);
    jack (g, { 1231.0f, 306.0f }, 25.0f);
    boxed (g, "1", 1231.0f, 262.0f, 9.0f, kInk, kGold);
    boxed (g, "2", 1231.0f, 353.0f, 9.0f, kInk, kGold);
    text (g, "PHANTOM", 1336.0f, 205.0f, 8.5f, kInk, false, 70.0f);
    text (g, "+48V", 1336.0f, 216.0f, 8.5f, kInk, false, 50.0f);
    {
        // the card slot (recorded songs live in the card folder)
        const R slot (1290.0f, 252.0f, 80.0f, 132.0f);
        g.setColour (juce::Colour (0xff0e0e10));
        g.fillRoundedRectangle (slot, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (slot.reduced (1.0f), 6.0f, 1.0f);
        g.setColour (juce::Colour (0xff1c1c1f));
        g.fillRoundedRectangle (slot.reduced (10.0f, 14.0f), 3.0f);
        text (g, "PUSH", 1303.0f, 266.0f, 6.5f, kPrint.withAlpha (0.7f), true, 30.0f);
        text (g, "EJECT", 1303.0f, 274.0f, 6.5f, kPrint.withAlpha (0.7f), true, 30.0f);
        text (g, "CARD", 1338.0f, 318.0f, 12.0f, kPrint.withAlpha (0.55f), false, 60.0f);
    }

    // ---------------------------------------------------------------- the display block
    {
        const R bezel (965.0f, 403.0f, 417.0f, 262.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bezel.translated (3.0f, 5.0f), 8.0f);
        juce::ColourGradient body (juce::Colour (0xff2a2b2f), bezel.getTopLeft(), juce::Colour (0xff141416), bezel.getBottomLeft(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (bezel, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (bezel.reduced (1.0f), 8.0f, 1.0f);
        // the brand above the display
        text (g, "TRACKDESK", 1132.0f, 462.0f, 17.0f, kPrint, false, 200.0f);
        // the window
        const R win (1023.0f, 478.0f, 218.0f, 118.0f);
        g.setColour (juce::Colour (0xff050506));
        g.fillRoundedRectangle (win, 4.0f);
        text (g, "USB", 993.0f, 526.0f, 8.0f, kPrint, false, 30.0f);
        text (g, "PFL/", 993.0f, 573.0f, 8.0f, kPrint, false, 30.0f);
        text (g, "AFL", 993.0f, 583.0f, 8.0f, kPrint, false, 30.0f);
        // the arrows from the screen's labels to F1-F4
        g.setColour (kPrint.withAlpha (0.85f));
        const float kx[] = { 1015.0f, 1085.0f, 1152.0f, 1222.0f };
        const float tops[] = { 1047.0f, 1097.0f, 1149.0f, 1199.0f };
        for (int i = 0; i < 4; ++i)
        {
            g.drawLine (tops[i], 604.0f, kx[i], 624.0f, 1.2f);
            juce::Path arrow;
            arrow.addArrow ({ kx[i], 624.0f, tops[i], 604.0f }, 1.2f, 5.0f, 5.0f);
            g.fillPath (arrow);
        }
        // the meter's printing
        text (g, "MAIN", 1313.0f, 455.0f, 8.5f, kPrint, false, 40.0f);
        g.drawLine (1276.0f, 455.0f, 1294.0f, 455.0f, 1.0f);
        g.drawLine (1332.0f, 455.0f, 1350.0f, 455.0f, 1.0f);
        const char* marks[] = { "OL", "15", "10", "6", "3", "0", "-2", "-4", "-7", "-10", "-20", "-30" };
        for (int i = 0; i < 12; ++i)
            text (g, marks[i], 1313.0f, 463.0f + 15.0f * (float) i + 7.5f, 7.5f, kPrint, true, 30.0f);
        boxed (g, "L", 1287.0f, 651.0f, 7.5f, kPrint.withAlpha (0.85f), kInk, 3.0f);
        text (g, "(dB)", 1313.0f, 651.0f, 7.5f, kPrint, true, 30.0f);
        boxed (g, "R", 1339.0f, 651.0f, 7.5f, kPrint.withAlpha (0.85f), kInk, 3.0f);
    }

    // ---------------------------------------------------------------- the transport block
    {
        const R tb (965.0f, 668.0f, 417.0f, 128.0f);
        juce::ColourGradient body (juce::Colour (0xff232427), tb.getTopLeft(), juce::Colour (0xff17181a), tb.getBottomLeft(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (tb, 6.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (tb.reduced (1.0f), 6.0f, 1.0f);
        text (g, "MULTI JOG", 1052.0f, 695.0f, 8.5f, kPrint, false, 70.0f);
        text (g, "PUSH", 996.0f, 770.0f, 7.5f, kPrint, false, 40.0f);
        text (g, "ENTER", 996.0f, 780.0f, 7.5f, kPrint, false, 40.0f);
        // the turning arrows round the dial
        g.setColour (kPrint);
        juce::Path arc;
        arc.addCentredArc (1046.0f, 745.0f, 42.0f, 42.0f, 0.0f, -1.05f, 1.05f, true);
        g.strokePath (arc, juce::PathStrokeType (1.2f));
        juce::Path a1, a2;
        a1.addArrow ({ 1012.0f, 721.0f, 1009.0f, 726.0f }, 1.2f, 6.0f, 5.0f);
        a2.addArrow ({ 1080.0f, 721.0f, 1083.0f, 726.0f }, 1.2f, 6.0f, 5.0f);
        g.fillPath (a1);
        g.fillPath (a2);
        text (g, "MENU", 1152.0f, 688.0f, 8.5f, kPrint, false, 40.0f);
        // the STOP key's side guards
        g.setColour (juce::Colour (0xff0b0b0c));
        g.fillRoundedRectangle (R (1131.0f, 742.0f, 8.0f, 38.0f), 2.0f);
        g.fillRoundedRectangle (R (1196.0f, 742.0f, 8.0f, 38.0f), 2.0f);
        // REW / FF glyphs above the keys
        auto tri = [&] (float x, float y, float dir) {
            juce::Path p;
            for (int i = 0; i < 2; ++i)
            {
                const float x0 = x + dir * (float) i * 7.0f - (dir > 0 ? 7.0f : -7.0f);
                p.addTriangle (x0, y - 4.0f, x0, y + 4.0f, x0 + dir * 7.0f, y);
            }
            g.fillPath (p);
        };
        g.setColour (kPrint);
        tri (1255.0f, 688.0f, -1.0f);
        tri (1322.0f, 688.0f, 1.0f);
    }

    // ---------------------------------------------------------------- monitoring
    {
        for (P c : { P (1010.0f, 850.0f), P (1010.0f, 925.0f) })
        {
            tickRing (g, c, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
            text (g, juce::String::fromUTF8 ("\xe2\x88\x9e"), c.x - 22.0f, c.y + 30.0f, 10.0f, kPrint, false, 20.0f);
            text (g, "dB", c.x, c.y + 31.0f, 7.5f, kPrint, true, 20.0f);
            text (g, "+10", c.x + 22.0f, c.y + 30.0f, 8.0f, kPrint, true, 30.0f);
            text (g, "0", c.x + 26.0f, c.y - 22.0f, 7.5f, kPrint, true, 20.0f);
        }
        text (g, "MUTE", 1080.0f, 881.0f, 8.5f, kPrint, false, 40.0f);
        text (g, "MUTE", 1080.0f, 958.0f, 8.5f, kPrint, false, 40.0f);
        boxed (g, "AUX 1", 1137.0f, 826.0f, 8.0f, kPrint, kInk);
        boxed (g, "AUX 2", 1137.0f, 903.0f, 8.0f, kPrint, kInk);
        text (g, "AFL", 1137.0f, 881.0f, 8.5f, kPrint, false, 40.0f);
        text (g, "AFL", 1137.0f, 958.0f, 8.5f, kPrint, false, 40.0f);
        text (g, "SD MAIN MIX", 1215.0f, 914.0f, 7.5f, kPrint, false, 60.0f);
        text (g, "RETURN", 1205.0f, 924.0f, 7.5f, kPrint, false, 50.0f);
        screwAt (g, { 1258.0f, 968.0f }, 13.0f);
        for (P c : { P (1010.0f, 1015.0f), P (1115.0f, 1018.0f), P (1222.0f, 1018.0f) })
        {
            tickRing (g, c, kKnob * 0.5f + 4.0f, kPrint, 11, 4.0f);
            text (g, "MIN", c.x - 22.0f, c.y + 30.0f, 7.5f, kPrint, true, 30.0f);
            text (g, "MAX", c.x + 22.0f, c.y + 30.0f, 7.5f, kPrint, true, 30.0f);
        }
        text (g, "PFL/AFL", 1010.0f, 1057.0f, 9.0f, kPrint, false, 60.0f);
        text (g, "MASTER", 1010.0f, 1069.0f, 9.0f, kPrint, false, 60.0f);
        text (g, "PHONES", 1170.0f, 975.0f, 9.0f, kPrint, false, 60.0f);
        g.setColour (kPrint);
        g.drawLine (1100.0f, 975.0f, 1140.0f, 975.0f, 1.0f);
        g.drawLine (1200.0f, 975.0f, 1240.0f, 975.0f, 1.0f);
        g.drawLine (1100.0f, 975.0f, 1100.0f, 981.0f, 1.0f);
        g.drawLine (1240.0f, 975.0f, 1240.0f, 981.0f, 1.0f);
        boxed (g, "1", 1115.0f, 1056.0f, 8.0f, kPrint, kInk);
        boxed (g, "2", 1222.0f, 1056.0f, 8.0f, kPrint, kInk);
        for (float x : { 1062.0f, 1170.0f })
        {
            textL (g, "AUX 1/2", x + 14.0f, 1077.0f, 8.0f, kPrint, true);
            textL (g, "MAIN", x + 14.0f, 1090.0f, 8.0f, kPrint, true);
            g.setColour (kPrint);
            g.drawLine (x, 1080.0f, x + 3.0f, 1080.0f, 1.0f);
            g.drawLine (x + 3.0f, 1080.0f, x + 3.0f, 1074.0f, 1.0f);
            g.drawLine (x + 3.0f, 1074.0f, x + 9.0f, 1074.0f, 1.0f);
            g.drawLine (x + 9.0f, 1074.0f, x + 9.0f, 1080.0f, 1.0f);
            g.drawLine (x + 9.0f, 1080.0f, x + 12.0f, 1080.0f, 1.0f);
        }
    }

    // ---------------------------------------------------------------- the paired device (AUDIO LINK), CLICK, POST REC
    {
        text (g, "AUDIO LINK", 1232.0f, 825.0f, 11.0f, kPrint, false, 120.0f);
        g.setColour (kPrint.withAlpha (0.8f));
        g.drawRoundedRectangle (R (1183.0f, 840.0f, 94.0f, 50.0f), 4.0f, 1.0f);
        text (g, "PAIRING", 1228.0f, 881.0f, 8.5f, kPrint, false, 60.0f);
        text (g, "ASSIGN", 1330.0f, 843.0f, 8.5f, kPrint, false, 60.0f);
        text (g, "9/10", 1309.0f, 881.0f, 7.5f, kPrint, true, 30.0f);
        text (g, "OFF", 1331.0f, 881.0f, 7.5f, kPrint, true, 30.0f);
        text (g, "MAIN", 1354.0f, 881.0f, 7.5f, kPrint, true, 30.0f);
        boxed (g, "CLICK", 1330.0f, 917.0f, 9.0f, kInk, kGold);
        boxed (g, "TAP", 1360.0f, 944.0f, 7.5f, kInk, kGold, 3.0f);
        g.setColour (kInk);
        g.drawLine (1318.0f, 970.0f, 1342.0f, 970.0f, 1.2f);
        text (g, "TEMPO", 1310.0f, 993.0f, 8.0f, kInk, false, 50.0f);
        text (g, "START/", 1360.0f, 993.0f, 8.0f, kInk, false, 50.0f);
        text (g, "STOP", 1360.0f, 1004.0f, 8.0f, kInk, false, 50.0f);
        text (g, "POST", 1308.0f, 1108.0f, 8.5f, kInk, false, 50.0f);
        text (g, "REC", 1308.0f, 1119.0f, 8.5f, kInk, false, 50.0f);
    }

    // ---------------------------------------------------------------- FX, SUB, MAIN
    {
        boxed (g, "DIGITAL EFFECT", 1010.0f, 1136.0f, 7.5f, kInk, kPrint, 3.0f);
        boxed (g, "PROCESSOR", 1010.0f, 1147.0f, 7.5f, kInk, kPrint, 3.0f);
        text (g, "TO AUX 1", 1046.0f, 1165.0f, 7.5f, kInk, false, 50.0f);
        tickRing (g, { 1012.0f, 1200.0f }, kKnob * 0.5f + 4.0f, kInk, 11, 4.0f);
        text (g, juce::String::fromUTF8 ("\xe2\x88\x9e"), 990.0f, 1230.0f, 10.0f, kInk, false, 20.0f);
        text (g, "dB", 1012.0f, 1231.0f, 7.5f, kInk, true, 20.0f);
        text (g, "+15", 1034.0f, 1230.0f, 8.0f, kInk, true, 30.0f);
        text (g, "SELECT", 1010.0f, 1288.0f, 8.5f, kInk, false, 50.0f);
        for (int i = 0; i < kFxTypes; ++i)
        {
            const float y = 1319.0f + 11.4f * (float) i;
            boxed (g, juce::String (i + 1), 978.0f, y, 7.0f, kInk, kGold, 2.0f);
            textL (g, fxName (i), 990.0f, y, 7.5f, kInk, true);
        }
        text (g, "MUTE", 1148.0f, 1187.0f, 8.5f, kInk, false, 40.0f);
        text (g, "AUX 1", 1148.0f, 1270.0f, 8.5f, kInk, false, 40.0f);
        text (g, "MAIN", 1148.0f, 1329.0f, 8.5f, kInk, false, 40.0f);
        text (g, "SUB", 1148.0f, 1389.0f, 8.5f, kInk, false, 40.0f);
        text (g, "AFL", 1148.0f, 1452.0f, 8.5f, kInk, false, 40.0f);
        faderScale (g, 1083.0f, kInk);
        text (g, "FX", 1095.0f, 1491.0f, 19.0f, kInk, false, 60.0f);
        text (g, "MUTE", 1255.0f, 1187.0f, 8.5f, kInk, false, 40.0f);
        text (g, "MAIN", 1255.0f, 1329.0f, 8.5f, kInk, false, 40.0f);
        faderScale (g, 1200.0f, kInk);
        text (g, "SUB", 1222.0f, 1491.0f, 19.0f, kInk, false, 60.0f);
        text (g, "MUTE", 1360.0f, 1187.0f, 8.5f, kInk, false, 40.0f);
        faderScale (g, 1305.0f, kInk);
        text (g, "MAIN", 1330.0f, 1491.0f, 19.0f, kInk, false, 80.0f);
        screwAt (g, { 1362.0f, 1449.0f });
    }

    // ---------------------------------------------------------------- the badge
    {
        // our own mark: TD-12, heavy and rounded, with the product line under it
        g.setColour (kPrint);
        g.setFont (panelFont (54.0f));
        g.drawText ("TD-12", R (1090.0f, 58.0f, 290.0f, 60.0f), J::centredRight);
        text (g, "MIXER / RECORDER / INTERFACE", 1270.0f, 122.0f, 8.0f, kPrint, false, 220.0f);
    }
}
} // namespace trackdesk::ui
