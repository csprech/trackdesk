#pragma once

// The console's state: the eight channel strips (1-6 mono, 7/8 and 9/10 stereo), the master section, the menu
// settings that survive power-off, and a song's data (its tracks are in Recorder.h).

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>
#include <vector>

namespace trackdesk
{
constexpr int kStrips = 8;  // channel strips
constexpr int kInputs = 10; // input jacks: 1-6, 7, 8, 9, 10
constexpr int kTracks = 12; // recorder tracks: 1-10, and 11/12 the MAIN MIX
constexpr int kMixTrack = 10; // track 11 (index 10) and 12
constexpr int kFxTypes = 16;
constexpr int kFxLibrary = 10;
constexpr int kMaxMarks = 99;
constexpr int kMaxVamps = 10;

inline bool isStereo (int strip) { return strip >= 6; }
/** The first input jack (and recorder track) of a strip: 0-5, then 6 (7/8) and 8 (9/10). */
inline int firstInputOf (int strip) { return strip < 6 ? strip : 6 + (strip - 6) * 2; }
inline int stripOfInput (int input) { return input < 6 ? input : 6 + (input - 6) / 2; }
juce::String stripName (int strip); // "1".."6", "7/8", "9/10"
juce::String trackName (int track); // "1".."12"

enum class Mode : uint8_t { Live = 0, Pc, Mtr };
enum class SendPoint : uint8_t { PreComp = 0, PostComp, PostEq };
enum class SoloMode : uint8_t { Pfl = 0, InPlace };
enum class ClickOut : uint8_t { Off = 0, CountInRec, Rec, RecPlay };
enum class CountIn : uint8_t { Sig1 = 0, Sig2, Sig4, Beat2p4, Beat2, Beat3, Beat4 };
enum class ClickSound : uint8_t { Click = 0, Stick, Bell, Kick, Snare, PedalHat, OpenHat, ClosedHat };
enum class Accent : uint8_t { Off = 0, Low, Mid, High };
enum class Repeat : uint8_t { Continue = 0, PlayStop, Loop };
enum class FootFn : uint8_t { Off = 0, PlayPause, PunchIO, FxMute, RecStop, Marker, MarkSkip, SongIncrement, Tap, Vamp, VampIOSet };
enum class F4Fn : uint8_t { Mark = 0, Click };
enum class Assign : uint8_t { To910 = 0, Off, ToMain };

namespace law
{
    /** The fader position of 0 dB (on the printed scale). */
    constexpr float kFaderUnity = 0.728f;
} // namespace law

/** One channel strip. Knobs and faders are their panel positions, 0..1. */
struct Strip
{
    float gain = 0.2f;      // (unity on a line input)
    float comp = 0.0f;      // OFF .. MAX
    float eqHigh = 0.5f, eqMidFreq = 0.4f, eqMid = 0.5f, eqLow = 0.5f; // (mid frequency 0.4 = 600 Hz)
    float aux1 = 0.0f, aux2 = 0.0f; // infinity .. +15 dB
    float pan = 0.5f;       // L .. R (balance on the stereo strips)
    float fader = 0.0f;     // -infinity .. +10 dB
    bool lowCut = false, inst = false, mute = false, main = true, sub = false, solo = false;
    Mode mode = Mode::Live;
    // the MIXER menu (kept like the other settings)
    bool phaseInvert = false;
    SendPoint sendPoint = SendPoint::PreComp;
    bool operator== (const Strip&) const = default;
};

/** The master section's controls. */
struct Master
{
    float eqHigh = 0.5f, eqMidFreq = 0.4f, eqMid = 0.5f, eqLow = 0.5f;
    bool eqNarrow = false, eqOnAux = false, eqIn = false; // (EQ IN/BYPASS: bypassed from the factory)
    bool phantom = false;
    float aux1Out = 0.75f, aux2Out = 0.75f, pflAfl = 0.75f, phones1 = 0.5f, phones2 = 0.5f;
    bool aux1Mute = false, aux1Afl = false, aux2Mute = false, aux2Afl = false;
    bool sdReturn = false, phones1Aux = false, phones2Aux = false;
    Assign assign = Assign::Off;
    float fxToAux1 = 0.0f, fxFader = law::kFaderUnity, subFader = 0.0f, mainFader = 0.0f;
    bool fxMute = false, fxAux1 = false, fxMain = true, fxSub = false, fxAfl = false;
    bool subMute = false, subMain = false, mainMute = false, postRec = false;
    bool operator== (const Master&) const = default;
};

/** The built-in effect: its type, a parameter per type (0..1), on/off, and the library. */
struct FxSettings
{
    int type = 0; // 0 = HALL 1
    std::array<float, kFxTypes> param {};
    bool on = true;
    struct Slot
    {
        bool used = false;
        int type = 0;
        float param = 0.5f;
        bool operator== (const Slot&) const = default;
    };
    std::array<Slot, kFxLibrary> library {};
    int libSlot = -1; // the library number the effect is tied to (-1: none)
    FxSettings() { param.fill (0.5f); }
    bool operator== (const FxSettings&) const = default;
};

/** Metronome settings (saved in each song, p.50). */
struct Metronome
{
    double tempo = 120.0;
    int sigNum = 4, sigDen = 4;
    ClickSound sound = ClickSound::Click;
    Accent accent = Accent::Mid;
    ClickOut clickOut = ClickOut::Off;
    int clickVolume = 30; // the CLICK jack, 0..50
    // the click into the phones and buses: on/off and volume each (PHONES1, PHONES2, SUB, MAIN, AUX1, AUX2)
    std::array<bool, 6> routeOn {};
    std::array<int, 6> routeVolume { 30, 30, 30, 30, 30, 30 };
    CountIn countIn = CountIn::Beat4;
    bool barDisplay = false;
    bool operator== (const Metronome&) const = default;
};

/** Menu settings that survive power-off (p.28). */
struct Settings
{
    SoloMode soloMode = SoloMode::Pfl;
    bool aux1PostFader = false;
    bool phonesMainPost = true;
    bool midiTimecode = false, midiClock = false;
    std::array<FootFn, 2> footFn { FootFn::PlayPause, FootFn::Off };
    std::array<bool, 2> footNormallyClosed { false, false };
    F4Fn f4 = F4Fn::Mark;
    int contrast = 28;                 // DISPLAY: 10..40
    bool dimmerHigh = true;            // DISPLAY: DIMMER HIGH / LOW
    bool nameByDate = true;            // SONG NAME: DATE or WORD
    juce::String nameWord = "SONG";
    bool outputDelayOn = false, outputDelayOnMain = false;
    int outputDelayMs = 0;             // 0..2000
    int padMainDb = 0, padSubDb = 0;   // 0..-40
    int preRollSec = 2;                // 0 = OFF, 1..10
    bool vampAuto = true;
    bool usbStereoMix = false;         // USB AUDIO: MULTI INPUT / STEREO MIX
    bool operator== (const Settings&) const = default;
};

/** A song's data other than its audio: format, marks, punch and vamp points, metronome, effect. */
struct SongInfo
{
    juce::String name;
    double sampleRate = 48000.0;
    int bits = 24;
    bool protect = false;
    std::vector<int64_t> marks;                     // sample positions
    int64_t punchIn = -1, punchOut = -1;            // -1 = not set
    bool autoPunch = false;
    std::vector<std::pair<int64_t, int64_t>> vamps; // in/out pairs
    bool vampOn = false;
    Metronome metronome;
    int64_t lastRecStart = 0, lastRecEnd = 0;
};

// ---- the panel's law: knob positions to what they do (the panel's printed scales)
namespace law
{
    /** GAIN in dB: the line scales (p.76): -10..+40 on 1-6, -20..+30 on 7-10. */
    float gainDb (float knob, int strip);
    /** The channel fader and the MAIN/SUB/FX faders: +10 at the top, 0 at 3/4, -inf at the bottom. */
    float faderGain (float pos);
    float faderDb (float pos);
    float faderPosForDb (float db);
    /** AUX sends and TO AUX 1: infinity .. +15 dB, unity at 3/4. */
    float sendGain (float knob);
    /** EQ gain knobs: -15..+15 dB. */
    inline float eqDb (float knob) { return (knob - 0.5f) * 30.0f; }
    /** The channel MID frequency (100 Hz .. 8 kHz, log). */
    float midHz (float knob);
    float midKnobForHz (float hz);
    /** Output level knobs (AUX 1/2 out, PFL/AFL, PHONES): -inf .. +10 dB, unity at 3/4. */
    float levelGain (float knob);
    /** -3 dB centre pan: L and R gains. */
    void pan (float knob, float& l, float& r);
    /** Balance (stereo strips): unity at centre, the other side faded. */
    void balance (float knob, float& l, float& r);
} // namespace law

juce::var settingsToJson (const Settings&, const FxSettings&, const std::array<Strip, kStrips>&, const Master&);
bool settingsFromJson (const juce::var&, Settings&, FxSettings&, std::array<Strip, kStrips>&, Master&);
juce::var metronomeToJson (const Metronome&);
void metronomeFromJson (const juce::var&, Metronome&);

const char* fxName (int type);         // "HALL 1" ...
const char* fxParamName (int type);    // "TIME", "RATE" ...
juce::String fxParamText (int type, float v);
const char* clickSoundName (ClickSound);
const char* footFnName (FootFn);
} // namespace trackdesk
