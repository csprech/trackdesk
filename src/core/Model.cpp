#include "Model.h"
#include <cmath>

namespace trackdesk
{
juce::String stripName (int strip)
{
    if (strip < 6)
        return juce::String (strip + 1);
    return strip == 6 ? "7/8" : "9/10";
}

juce::String trackName (int track) { return juce::String (track + 1); }

namespace law
{
    float gainDb (float k, int strip)
    {
        k = juce::jlimit (0.0f, 1.0f, k);
        return strip < 6 ? -10.0f + 50.0f * k : -20.0f + 50.0f * k;
    }

    // the printed fader scale: position (0 bottom .. 1 top) against dB
    namespace
    {
        // (measured off the panel drawing: +10 at the top, 0 at 0.728 of the travel, the infinity mark at the bottom)
        const float kFaderPos[] = { 0.0f, 0.051f, 0.097f, 0.237f, 0.350f, 0.482f, 0.658f, 0.728f, 0.813f, 1.0f };
        const float kFaderDb[] = { -100.0f, -40.0f, -30.0f, -20.0f, -15.0f, -10.0f, -5.0f, 0.0f, 5.0f, 10.0f };
    } // namespace

    float faderDb (float pos)
    {
        pos = juce::jlimit (0.0f, 1.0f, pos);
        for (int i = 1; i < (int) std::size (kFaderPos); ++i)
            if (pos <= kFaderPos[i])
            {
                const float t = (pos - kFaderPos[i - 1]) / (kFaderPos[i] - kFaderPos[i - 1]);
                return kFaderDb[i - 1] + (kFaderDb[i] - kFaderDb[i - 1]) * t;
            }
        return 10.0f;
    }

    float faderGain (float pos)
    {
        if (pos <= 0.001f)
            return 0.0f; // -infinity at the bottom
        return juce::Decibels::decibelsToGain (faderDb (pos), -100.0f);
    }

    float faderPosForDb (float db)
    {
        for (int i = 1; i < (int) std::size (kFaderPos); ++i)
            if (db <= kFaderDb[i])
                return kFaderPos[i - 1] + (kFaderPos[i] - kFaderPos[i - 1]) * (db - kFaderDb[i - 1]) / (kFaderDb[i] - kFaderDb[i - 1]);
        return 1.0f;
    }

    float sendGain (float k)
    {
        k = juce::jlimit (0.0f, 1.0f, k);
        if (k <= 0.001f)
            return 0.0f;
        // unity at 3/4, +15 dB at the top, falling to -50 dB then infinity
        const float db = k >= 0.75f ? (k - 0.75f) / 0.25f * 15.0f : -50.0f * std::pow ((0.75f - k) / 0.75f, 1.6f);
        return juce::Decibels::decibelsToGain (db, -100.0f);
    }

    float midHz (float k) { return 100.0f * std::pow (80.0f, juce::jlimit (0.0f, 1.0f, k)); }
    float midKnobForHz (float hz) { return std::log (hz / 100.0f) / std::log (80.0f); }

    float levelGain (float k)
    {
        k = juce::jlimit (0.0f, 1.0f, k);
        if (k <= 0.001f)
            return 0.0f;
        const float db = k >= 0.75f ? (k - 0.75f) / 0.25f * 10.0f : -60.0f * std::pow ((0.75f - k) / 0.75f, 1.5f);
        return juce::Decibels::decibelsToGain (db, -100.0f);
    }

    void pan (float k, float& l, float& r)
    {
        const float a = juce::jlimit (0.0f, 1.0f, k) * juce::MathConstants<float>::halfPi;
        l = std::cos (a);
        r = std::sin (a); // centre: 0.707 each (-3 dB), as the manual says (p.21)
    }

    void balance (float k, float& l, float& r)
    {
        k = juce::jlimit (0.0f, 1.0f, k);
        l = k <= 0.5f ? 1.0f : (1.0f - k) * 2.0f;
        r = k >= 0.5f ? 1.0f : k * 2.0f;
    }
} // namespace law

// ---------------------------------------------------------------- names

const char* fxName (int t)
{
    static const char* n[kFxTypes] = { "HALL 1",  "HALL 2",  "ROOM 1",    "ROOM 2", "PLATE",   "STUDIO",       "LIVE",          "DELAY 1",
                                       "DELAY 2", "PING PONG", "CHORUS", "FLANGER", "DELAY+HALL 1", "DELAY+HALL 2", "CHORUS+HALL 1", "CHORUS+HALL 2" };
    return n[juce::jlimit (0, kFxTypes - 1, t)];
}

const char* fxParamName (int t)
{
    if (t <= 6)
        return "REVERB TIME";
    if (t <= 9 || t == 12 || t == 13)
        return "DELAY TIME";
    return "RATE";
}

juce::String fxParamText (int t, float v)
{
    if (t <= 6)
        return juce::String (0.5 + 5.5 * v, 1) + "s";
    if (t <= 9 || t == 12 || t == 13)
        return juce::String (juce::roundToInt (20.0f + 980.0f * v)) + "ms";
    return juce::String (0.1 + 4.9 * v, 1) + "Hz";
}

const char* clickSoundName (ClickSound s)
{
    static const char* n[] = { "CLICK", "STICK", "BELL", "KICK", "SNARE", "PEDAL HIHAT", "OPEN HIHAT", "CLOSE HIHAT" };
    return n[(int) s];
}

const char* footFnName (FootFn f)
{
    static const char* n[] = { "OFF", "PLAY/PAUSE", "PUNCH I/O", "FX MUTE", "REC/STOP", "MARKER", "MARK SKIP", "SONG INCREMENT", "TAP", "VAMP", "VAMP I/O SET" };
    return n[(int) f];
}

// ---------------------------------------------------------------- JSON

namespace
{
    juce::DynamicObject* obj() { return new juce::DynamicObject(); }
    template <typename T>
    T clampEnum (const juce::var& v, T dflt, int maxValue)
    {
        return v.isVoid() ? dflt : (T) juce::jlimit (0, maxValue, (int) v);
    }
    float f01 (const juce::var& v, float dflt) { return v.isVoid() ? dflt : juce::jlimit (0.0f, 1.0f, (float) (double) v); }
    bool flag (const juce::var& v, bool dflt) { return v.isVoid() ? dflt : (bool) v; }
} // namespace

juce::var metronomeToJson (const Metronome& m)
{
    auto* o = obj();
    o->setProperty ("tempo", m.tempo);
    o->setProperty ("sigNum", m.sigNum);
    o->setProperty ("sigDen", m.sigDen);
    o->setProperty ("sound", (int) m.sound);
    o->setProperty ("accent", (int) m.accent);
    o->setProperty ("clickOut", (int) m.clickOut);
    o->setProperty ("clickVolume", m.clickVolume);
    juce::Array<juce::var> on, vol;
    for (int i = 0; i < 6; ++i)
    {
        on.add (m.routeOn[(size_t) i]);
        vol.add (m.routeVolume[(size_t) i]);
    }
    o->setProperty ("routeOn", on);
    o->setProperty ("routeVolume", vol);
    o->setProperty ("countIn", (int) m.countIn);
    o->setProperty ("barDisplay", m.barDisplay);
    return juce::var (o);
}

void metronomeFromJson (const juce::var& v, Metronome& m)
{
    if (! v.isObject())
        return;
    m.tempo = juce::jlimit (20.0, 250.0, (double) v.getProperty ("tempo", 120.0));
    m.sigNum = juce::jlimit (1, 12, (int) v.getProperty ("sigNum", 4));
    m.sigDen = juce::jlimit (1, 8, (int) v.getProperty ("sigDen", 4));
    m.sound = clampEnum (v["sound"], ClickSound::Click, 7);
    m.accent = clampEnum (v["accent"], Accent::Mid, 3);
    m.clickOut = clampEnum (v["clickOut"], ClickOut::Off, 3);
    m.clickVolume = juce::jlimit (0, 50, (int) v.getProperty ("clickVolume", 30));
    if (auto* on = v["routeOn"].getArray())
        for (int i = 0; i < std::min (6, on->size()); ++i)
            m.routeOn[(size_t) i] = (bool) (*on)[i];
    if (auto* vol = v["routeVolume"].getArray())
        for (int i = 0; i < std::min (6, vol->size()); ++i)
            m.routeVolume[(size_t) i] = juce::jlimit (0, 50, (int) (*vol)[i]);
    m.countIn = clampEnum (v["countIn"], CountIn::Beat4, 6);
    m.barDisplay = flag (v["barDisplay"], false);
}

juce::var settingsToJson (const Settings& s, const FxSettings& fx, const std::array<Strip, kStrips>& strips, const Master& m)
{
    auto* o = obj();
    o->setProperty ("format", "trackdesk-settings");
    auto* so = obj();
    so->setProperty ("soloMode", (int) s.soloMode);
    so->setProperty ("aux1Post", s.aux1PostFader);
    so->setProperty ("phonesMainPost", s.phonesMainPost);
    so->setProperty ("midiTimecode", s.midiTimecode);
    so->setProperty ("midiClock", s.midiClock);
    so->setProperty ("foot1", (int) s.footFn[0]);
    so->setProperty ("foot2", (int) s.footFn[1]);
    so->setProperty ("foot1NC", s.footNormallyClosed[0]);
    so->setProperty ("foot2NC", s.footNormallyClosed[1]);
    so->setProperty ("f4", (int) s.f4);
    so->setProperty ("contrast", s.contrast);
    so->setProperty ("dimmerHigh", s.dimmerHigh);
    so->setProperty ("usbStereoMix", s.usbStereoMix);
    so->setProperty ("nameByDate", s.nameByDate);
    so->setProperty ("nameWord", s.nameWord);
    so->setProperty ("outputDelayOn", s.outputDelayOn);
    so->setProperty ("outputDelayOnMain", s.outputDelayOnMain);
    so->setProperty ("outputDelayMs", s.outputDelayMs);
    so->setProperty ("padMain", s.padMainDb);
    so->setProperty ("padSub", s.padSubDb);
    so->setProperty ("preRoll", s.preRollSec);
    so->setProperty ("vampAuto", s.vampAuto);
    o->setProperty ("settings", juce::var (so));

    auto* fo = obj();
    fo->setProperty ("type", fx.type);
    fo->setProperty ("on", fx.on);
    fo->setProperty ("libSlot", fx.libSlot);
    juce::Array<juce::var> params, lib;
    for (auto p : fx.param)
        params.add (p);
    for (auto& slot : fx.library)
    {
        auto* l = obj();
        l->setProperty ("used", slot.used);
        l->setProperty ("type", slot.type);
        l->setProperty ("param", slot.param);
        lib.add (juce::var (l));
    }
    fo->setProperty ("params", params);
    fo->setProperty ("library", lib);
    o->setProperty ("fx", juce::var (fo));

    juce::Array<juce::var> st;
    for (auto& c : strips)
    {
        auto* j = obj();
        j->setProperty ("gain", c.gain);
        j->setProperty ("comp", c.comp);
        j->setProperty ("eqHigh", c.eqHigh);
        j->setProperty ("eqMidFreq", c.eqMidFreq);
        j->setProperty ("eqMid", c.eqMid);
        j->setProperty ("eqLow", c.eqLow);
        j->setProperty ("aux1", c.aux1);
        j->setProperty ("aux2", c.aux2);
        j->setProperty ("pan", c.pan);
        j->setProperty ("fader", c.fader);
        j->setProperty ("lowCut", c.lowCut);
        j->setProperty ("inst", c.inst);
        j->setProperty ("mute", c.mute);
        j->setProperty ("main", c.main);
        j->setProperty ("sub", c.sub);
        j->setProperty ("solo", c.solo);
        j->setProperty ("mode", (int) c.mode);
        j->setProperty ("phase", c.phaseInvert);
        j->setProperty ("sendPoint", (int) c.sendPoint);
        st.add (juce::var (j));
    }
    o->setProperty ("strips", st);

    auto* mo = obj();
    mo->setProperty ("eqHigh", m.eqHigh);
    mo->setProperty ("eqMidFreq", m.eqMidFreq);
    mo->setProperty ("eqMid", m.eqMid);
    mo->setProperty ("eqLow", m.eqLow);
    mo->setProperty ("eqNarrow", m.eqNarrow);
    mo->setProperty ("eqOnAux", m.eqOnAux);
    mo->setProperty ("eqIn", m.eqIn);
    mo->setProperty ("aux1Out", m.aux1Out);
    mo->setProperty ("aux2Out", m.aux2Out);
    mo->setProperty ("pflAfl", m.pflAfl);
    mo->setProperty ("phones1", m.phones1);
    mo->setProperty ("phones2", m.phones2);
    mo->setProperty ("aux1Mute", m.aux1Mute);
    mo->setProperty ("aux1Afl", m.aux1Afl);
    mo->setProperty ("aux2Mute", m.aux2Mute);
    mo->setProperty ("aux2Afl", m.aux2Afl);
    mo->setProperty ("sdReturn", m.sdReturn);
    mo->setProperty ("phones1Aux", m.phones1Aux);
    mo->setProperty ("phones2Aux", m.phones2Aux);
    mo->setProperty ("assign", (int) m.assign);
    mo->setProperty ("fxToAux1", m.fxToAux1);
    mo->setProperty ("fxFader", m.fxFader);
    mo->setProperty ("subFader", m.subFader);
    mo->setProperty ("mainFader", m.mainFader);
    mo->setProperty ("fxMute", m.fxMute);
    mo->setProperty ("fxAux1", m.fxAux1);
    mo->setProperty ("fxMain", m.fxMain);
    mo->setProperty ("fxSub", m.fxSub);
    mo->setProperty ("fxAfl", m.fxAfl);
    mo->setProperty ("subMute", m.subMute);
    mo->setProperty ("subMain", m.subMain);
    mo->setProperty ("mainMute", m.mainMute);
    mo->setProperty ("postRec", m.postRec);
    o->setProperty ("master", juce::var (mo));
    return juce::var (o);
}

bool settingsFromJson (const juce::var& v, Settings& s, FxSettings& fx, std::array<Strip, kStrips>& strips, Master& m)
{
    if (v.getProperty ("format", {}).toString() != "trackdesk-settings")
        return false;
    const auto so = v["settings"];
    s.soloMode = clampEnum (so["soloMode"], SoloMode::Pfl, 1);
    s.aux1PostFader = flag (so["aux1Post"], false);
    s.phonesMainPost = flag (so["phonesMainPost"], true);
    s.midiTimecode = flag (so["midiTimecode"], false);
    s.midiClock = flag (so["midiClock"], false);
    s.footFn[0] = clampEnum (so["foot1"], FootFn::PlayPause, 10);
    s.footFn[1] = clampEnum (so["foot2"], FootFn::Off, 10);
    s.footNormallyClosed[0] = flag (so["foot1NC"], false);
    s.footNormallyClosed[1] = flag (so["foot2NC"], false);
    s.f4 = clampEnum (so["f4"], F4Fn::Mark, 1);
    s.contrast = juce::jlimit (10, 40, (int) so.getProperty ("contrast", 28));
    s.dimmerHigh = flag (so["dimmerHigh"], true);
    s.usbStereoMix = flag (so["usbStereoMix"], false);
    s.nameByDate = flag (so["nameByDate"], true);
    s.nameWord = so.getProperty ("nameWord", "SONG").toString().substring (0, 8);
    s.outputDelayOn = flag (so["outputDelayOn"], false);
    s.outputDelayOnMain = flag (so["outputDelayOnMain"], false);
    s.outputDelayMs = juce::jlimit (0, 2000, (int) so.getProperty ("outputDelayMs", 0));
    s.padMainDb = juce::jlimit (-40, 0, (int) so.getProperty ("padMain", 0));
    s.padSubDb = juce::jlimit (-40, 0, (int) so.getProperty ("padSub", 0));
    s.preRollSec = juce::jlimit (0, 10, (int) so.getProperty ("preRoll", 2));
    s.vampAuto = flag (so["vampAuto"], true);

    const auto fo = v["fx"];
    fx.type = juce::jlimit (0, kFxTypes - 1, (int) fo.getProperty ("type", 0));
    fx.on = flag (fo["on"], true);
    fx.libSlot = juce::jlimit (-1, kFxLibrary - 1, (int) fo.getProperty ("libSlot", -1));
    if (auto* p = fo["params"].getArray())
        for (int i = 0; i < std::min (kFxTypes, p->size()); ++i)
            fx.param[(size_t) i] = f01 ((*p)[i], 0.5f);
    if (auto* lib = fo["library"].getArray())
        for (int i = 0; i < std::min (kFxLibrary, lib->size()); ++i)
        {
            const auto& l = lib->getReference (i);
            fx.library[(size_t) i] = { flag (l["used"], false), juce::jlimit (0, kFxTypes - 1, (int) l.getProperty ("type", 0)), f01 (l["param"], 0.5f) };
        }

    if (auto* st = v["strips"].getArray())
        for (int i = 0; i < std::min (kStrips, st->size()); ++i)
        {
            const auto& j = st->getReference (i);
            auto& c = strips[(size_t) i];
            c.gain = f01 (j["gain"], c.gain);
            c.comp = f01 (j["comp"], c.comp);
            c.eqHigh = f01 (j["eqHigh"], 0.5f);
            c.eqMidFreq = f01 (j["eqMidFreq"], 0.4f);
            c.eqMid = f01 (j["eqMid"], 0.5f);
            c.eqLow = f01 (j["eqLow"], 0.5f);
            c.aux1 = f01 (j["aux1"], 0.0f);
            c.aux2 = f01 (j["aux2"], 0.0f);
            c.pan = f01 (j["pan"], 0.5f);
            c.fader = f01 (j["fader"], 0.0f);
            c.lowCut = flag (j["lowCut"], false);
            c.inst = flag (j["inst"], false);
            c.mute = flag (j["mute"], false);
            c.main = flag (j["main"], true);
            c.sub = flag (j["sub"], false);
            c.solo = flag (j["solo"], false);
            c.mode = clampEnum (j["mode"], Mode::Live, 2);
            c.phaseInvert = flag (j["phase"], false);
            c.sendPoint = clampEnum (j["sendPoint"], SendPoint::PreComp, 2);
        }

    const auto mo = v["master"];
    m.eqHigh = f01 (mo["eqHigh"], 0.5f);
    m.eqMidFreq = f01 (mo["eqMidFreq"], 0.4f);
    m.eqMid = f01 (mo["eqMid"], 0.5f);
    m.eqLow = f01 (mo["eqLow"], 0.5f);
    m.eqNarrow = flag (mo["eqNarrow"], false);
    m.eqOnAux = flag (mo["eqOnAux"], false);
    m.eqIn = flag (mo["eqIn"], false);
    m.aux1Out = f01 (mo["aux1Out"], 0.75f);
    m.aux2Out = f01 (mo["aux2Out"], 0.75f);
    m.pflAfl = f01 (mo["pflAfl"], 0.75f);
    m.phones1 = f01 (mo["phones1"], 0.5f);
    m.phones2 = f01 (mo["phones2"], 0.5f);
    m.aux1Mute = flag (mo["aux1Mute"], false);
    m.aux1Afl = flag (mo["aux1Afl"], false);
    m.aux2Mute = flag (mo["aux2Mute"], false);
    m.aux2Afl = flag (mo["aux2Afl"], false);
    m.sdReturn = flag (mo["sdReturn"], false);
    m.phones1Aux = flag (mo["phones1Aux"], false);
    m.phones2Aux = flag (mo["phones2Aux"], false);
    m.assign = clampEnum (mo["assign"], Assign::Off, 2);
    m.fxToAux1 = f01 (mo["fxToAux1"], 0.0f);
    m.fxFader = f01 (mo["fxFader"], 0.75f);
    m.subFader = f01 (mo["subFader"], 0.0f);
    m.mainFader = f01 (mo["mainFader"], 0.0f);
    m.fxMute = flag (mo["fxMute"], false);
    m.fxAux1 = flag (mo["fxAux1"], false);
    m.fxMain = flag (mo["fxMain"], true);
    m.fxSub = flag (mo["fxSub"], false);
    m.fxAfl = flag (mo["fxAfl"], false);
    m.subMute = flag (mo["subMute"], false);
    m.subMain = flag (mo["subMain"], false);
    m.mainMute = flag (mo["mainMute"], false);
    m.postRec = flag (mo["postRec"], false);
    m.phantom = false; // (phantom power always starts off)
    return true;
}
} // namespace trackdesk
