#pragma once

// The console: every knob, fader, switch and key of the panel, the display and its menus, the lights, the
// recorder's transport, the metronome, MIDI clock and time code, the footswitch and the paired device.
// It's headless (the panel draws it, the tests play it). The message thread calls the control functions and the
// audio thread calls process(), both under the owner's lock; anything slow (files) goes through Storage.

#include "Lcd.h"
#include "Mixer.h"
#include "Recorder.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>

namespace trackdesk
{
enum class Key : uint8_t
{
    F1, F2, F3, F4, Menu, Jog, Rew, Ff, Stop, Play, Record, FxSelect, Click, Pairing, Foot1, Foot2
};
enum class StripKnob : uint8_t { Gain, Comp, EqHigh, EqMidFreq, EqMid, EqLow, Aux1, Aux2, Pan, Fader };
enum class StripSwitch : uint8_t { LowCut, Inst, Mute, Main, Sub, Solo, Rec };
enum class MasterKnob : uint8_t
{
    EqHigh, EqMidFreq, EqMid, EqLow, Aux1Out, Aux2Out, PflAfl, Phones1, Phones2, FxToAux1, FxFader, SubFader, MainFader
};
enum class MasterSwitch : uint8_t
{
    EqNarrow, EqOnAux, EqIn, Phantom, Aux1Mute, Aux1Afl, Aux2Mute, Aux2Afl, SdReturn, Phones1Aux, Phones2Aux,
    FxMute, FxAux1, FxMain, FxSub, FxAfl, SubMute, SubMain, MainMute, PostRec
};

/** A summary of a song in the song folder. */
struct SongSummary
{
    juce::String name;
    double sampleRate = 48000.0;
    int bits = 24;
    bool protect = false;
    int64_t bytes = 0;
    juce::Time written;
};

/** An entry in the MUSIC folder (for IMPORT and SD PLAY). */
struct MusicEntry
{
    juce::String name; // shown
    juce::String path; // relative to the MUSIC folder
    bool folder = false;
    double sampleRate = 0.0;
    int bits = 0, channels = 0;
    int64_t length = 0;
};

/** The song folder ("SD card"), provided by the engine (files) or the tests (memory). Called on the message thread. */
struct Storage
{
    virtual ~Storage() = default;
    virtual std::vector<SongSummary> songs() = 0;
    virtual juce::String newSongName (const Settings&) = 0;
    /** Creates an empty song (it's not loaded). */
    virtual bool createSong (const SongInfo&) = 0;
    /** Loads a song's info and tracks into the recorder. */
    virtual bool loadSong (const juce::String& name, Recorder& recorder) = 0;
    /** Saves the song's info and track lists (the takes are already on disk). */
    virtual bool saveSong (const Recorder& recorder) = 0;
    virtual bool deleteSong (const juce::String& name) = 0;
    virtual bool renameSong (const juce::String& from, const juce::String& to) = 0;
    virtual bool setProtected (const juce::String& name, bool protect) = 0;
    virtual bool clearMarks (const juce::String& name) = 0;
    /** A sink for a pass on the current song. */
    virtual std::shared_ptr<TakeSink> newTake (const SongInfo& song) = 0;
    /** The MUSIC folder: entries in a sub-folder ("" for the top). */
    virtual std::vector<MusicEntry> music (const juce::String& folder) = 0;
    /** Opens a WAV in the MUSIC folder: one TakeAudio per channel (empty on failure). */
    virtual std::vector<std::shared_ptr<TakeAudio>> openMusic (const juce::String& path) = 0;
    /** Copies a WAV channel into the song (for IMPORT): a take of the song. */
    virtual std::shared_ptr<TakeAudio> importAudio (const SongInfo& song, const juce::String& path, int channel) = 0;
    /** STEREO MIX EXPORT: tracks 11/12 to one stereo WAV in the MUSIC folder. */
    virtual bool exportMixExists (const SongInfo& song) = 0;
    virtual bool exportMix (const Recorder& recorder) = 0;
    /** MEDIA FORMAT: removes everything and makes the folders again. */
    virtual bool format() = 0;
    virtual int64_t freeBytes() = 0;
    virtual int64_t totalBytes() = 0;
    /** STORAGE: shows the folder on the computer. */
    virtual void reveal() = 0;
};

class Desk
{
public:
    Desk();
    ~Desk();

    void setStorage (Storage* s) { storage = s; }
    /** At start-up (and after MEDIA FORMAT): loads the last song, or the newest, or makes one. */
    void startUp (const juce::String& lastSong);
    void prepare (double sampleRate, int maxBlock);
    double getSampleRate() const { return sampleRate; }

    // ---------------------------------------------------------------- the panel
    std::array<Strip, kStrips> strips;
    Master master;
    Settings settings;
    FxSettings fx;
    Recorder recorder;
    std::array<bool, kStrips> recKey {}; // REC armed (per strip)

    void setKnob (int strip, StripKnob k, float v);
    float getKnob (int strip, StripKnob k) const;
    void setSwitch (int strip, StripSwitch w, bool on);
    bool getSwitch (int strip, StripSwitch w) const;
    void setMode (int strip, Mode m);
    void setMasterKnob (MasterKnob k, float v);
    float getMasterKnob (MasterKnob k) const;
    void setMasterSwitch (MasterSwitch w, bool on);
    bool getMasterSwitch (MasterSwitch w) const;
    void setAssign (Assign a);
    void press (Key k);
    void release (Key k);
    bool isDown (Key k) const { return down[(size_t) k]; }
    /** MULTI JOG turned (detents, clockwise positive). */
    void turnJog (int detents);
    /** Jacks in use (Tabletop, or what the audio device offers): affects the effect (AUX 2) and the USB light. */
    void setAux2JackUsed (bool used) { aux2JackUsed = used; }
    void setUsbConnected (bool c) { usbConnected = c; }

    // ---------------------------------------------------------------- outputs to the panel
    enum class Lamp : uint8_t { Off, On, Blink, Red, Green };
    struct Lights
    {
        std::array<Lamp, kStrips> rec {};
        std::array<bool, kStrips> sigGreen {}, sigRed {}, comp {}, mute {};
        bool post = false, pflAfl = false, phantom = false, usb = false;
        Lamp play = Lamp::Off, stop = Lamp::Off, record = Lamp::Off, pairing = Lamp::Off, tempo = Lamp::Off;
        bool aux1Mute = false, aux2Mute = false, fxMute = false, subMute = false, mainMute = false;
        bool blinkPhase = false;
    };
    Lights lights() const;
    /** Draws the display. */
    void render (Lcd& lcd) const;
    /** The level meters since the last call (peaks), and the MAIN meters' state. */
    Mixer::Meters takeMeters();
    /** The display's contrast (0..1) and whether it's dimmed. */
    float displayContrast() const { return (float) (settings.contrast - 10) / 30.0f; }

    // ---------------------------------------------------------------- requests to the owner (message thread)
    /** PAIRING pressed while waiting to pair: the owner asks for an audio file and calls pairWith(). */
    bool takePairingRequest();
    void pairWith (const juce::String& name, std::vector<std::shared_ptr<TakeAudio>> audio);
    juce::String pairedName() const { return btName; }
    /** Something to save in the settings file. */
    bool takeSettingsDirty();
    /** Called regularly on the message thread: collects finished passes, saves songs, times pop-ups. */
    void idle();
    juce::String currentSongName() const { return recorder.info.name; }
    bool songLoaded() const { return loaded; }

    // ---------------------------------------------------------------- audio (audio thread)
    struct AudioIO
    {
        std::array<const float*, kInputs> live {};
        std::array<const float*, kInputs> pc {};
        float *mainL = nullptr, *mainR = nullptr, *subL = nullptr, *subR = nullptr, *aux1 = nullptr, *aux2 = nullptr;
        float *phones1L = nullptr, *phones1R = nullptr, *phones2L = nullptr, *phones2R = nullptr, *clickOut = nullptr;
        std::array<float*, 12> usb {}; // what goes to the computer (USB AUDIO mode)
    };
    void process (int n, const AudioIO& io, juce::MidiBuffer* midiOut);

    // ---------------------------------------------------------------- for tests and the panel
    enum class ScreenId : uint8_t
    {
        Meter, Home, List, Choice, Number, Confirm, Effect, Song, SongNew, TextEdit, SongInfo, AutoPunch, VampEdit, VampPlay,
        TrackClear, Import, ImportTrack, TrackSwap, MixExport, SdPlay, SdPlaying, StorageMode, Information, DateTime, Display,
        DelayTime, SendPoint, Phase, Tempo, Signature
    };
    ScreenId screen() const;
    juce::String screenTitle() const;
    /** The item under the cursor on a list (a menu item, an option, a song, a file). */
    juce::String cursorText() const;
    /** The screen's value being edited (Choice: the option index; Number: the number). */
    int screenValue() const { return top().value; }
    juce::String popupText() const { return popup.lines.joinIntoString (" "); }
    bool popupOpen() const { return popup.active; }
    bool clickRunning() const { return metroOn; }
    int64_t tempoBeat() const { return beatIndex; }
    Mixer::Source sourceOf (int strip) const { return sources[(size_t) strip]; }
    bool fxUsable() const { return ! aux2JackUsed; }

private:
    friend struct DeskMenus;
    struct Screen
    {
        ScreenId id = ScreenId::Home;
        juce::String title;
        int cursor = 0, scroll = 0, field = 0, param = 0;
        // List
        struct Item
        {
            juce::String label;
            std::function<juce::String()> value;
            std::function<void()> open;
        };
        std::vector<Item> items;
        // Choice / Number
        juce::StringArray options;
        int value = 0, original = 0, minimum = 0, maximum = 0, step = 1;
        bool live = false; // applies as it's turned (F1 only goes back)
        std::function<int()> get;
        std::function<void (int)> set;
        std::function<juce::String (int)> format;
        // Confirm
        juce::StringArray lines;
        std::function<void()> action;
        // text editing, browsing
        juce::String text, folder;
        std::vector<MusicEntry> entries;
        std::function<void (const juce::String&)> done;
    };
    struct Popup
    {
        bool active = false;
        juce::StringArray lines;
        int64_t until = 0; // (samples) 0 = until a key
        enum Kind : uint8_t { Message, Confirm, Phantom } kind = Message;
        std::function<void()> onConfirm;
    };

    // control
    void keyDown (Key k);
    void keyUp (Key k);
    void transportKey (Key k);
    void footswitch (int index);
    void recordKey();
    void stopKey();
    void playKey();
    void locateKey (int dir);
    void clickKey();
    void addOrClearMark();
    void message (const juce::String& a, const juce::String& b = {}, double seconds = 1.5);
    void confirm (const juce::StringArray& lines, std::function<void()> action);
    void pullUp (const juce::String& s);
    bool stopped() const;
    bool canRecord (bool punchIn);
    std::array<bool, kTracks> armedTracks() const;
    int armedTrackCount() const;
    void loadSong (const juce::String& name);
    void saveSong();
    void newSong (double rate, int bits, const juce::String& name);
    void collectPasses();
    void applyMetronomeSettings();
    void settingsChanged() { settingsDirty = true; }
    void tapTempo();
    void sdPlayStart (int index);
    void sdPlayStop();
    void setFxParamFromTap (double ms);
    void updateLibrary();

    // screens (DeskMenu.cpp)
    void openHome();
    void openMeter();
    void openMenu();
    void openEffect();
    void push (Screen s);
    void pop();
    Screen& top() { return stack.back(); }
    const Screen& top() const { return stack.back(); }
    void screenKey (Key k);
    void screenJog (int d);
    void drawScreen (Lcd& lcd) const;
    void drawHome (Lcd& lcd) const;
    void drawMeters (Lcd& lcd) const;
    void drawFunctionBar (Lcd& lcd, const juce::String& f1, const juce::String& f2, const juce::String& f3, const juce::String& f4) const;
    void drawTitle (Lcd& lcd, const juce::String& title) const;
    void drawTransportIcon (Lcd& lcd, int x, int y) const;
    void drawPopup (Lcd& lcd) const;
    juce::String timeText (int64_t samples, bool frames = false) const;
    juce::String counterText() const;
    void choiceScreen (const juce::String& title, const juce::StringArray& options, std::function<int()> get, std::function<void (int)> set,
                       bool live = false);
    void numberScreen (const juce::String& title, int lo, int hi, std::function<int()> get, std::function<void (int)> set,
                       std::function<juce::String (int)> format, bool live = false, int step = 1);
    void listScreen (const juce::String& title, std::vector<Screen::Item> items);
    void confirmScreen (const juce::String& title, const juce::StringArray& lines, std::function<void()> action);
    void textScreen (const juce::String& title, const juce::String& text, int maxLength, std::function<void (const juce::String&)> done);
    void openSongList();
    void openSongPopup (int index);
    void openMtrMenu();
    void openVampMenu();
    void openClickMenu();
    void openMetronomeMenu();
    void openMidiMenu();
    void openMixerMenu();
    void openSystemMenu();
    void openMusicBrowser (ScreenId id, const juce::String& folder);
    bool vampScreenOpen() const;

    double sampleRate = 48000.0;
    int maxBlock = 512;
    Storage* storage = nullptr;
    bool loaded = false;
    std::vector<Screen> stack;
    Popup popup;
    juce::String pullUpText;
    int64_t pullUpUntil = 0;
    std::array<bool, 16> down {};
    std::array<int64_t, 16> downAt {};
    bool searching = false;
    int searchDir = 0;
    bool aux2JackUsed = false, usbConnected = false;
    bool settingsDirty = false;
    bool phantomOn = false; // (the power itself, after the confirmation)
    bool marksUnsaved = false;
    std::vector<SongSummary> songCache;
    juce::String songToLoadNext;

    // time (samples), advanced by process()
    int64_t clock = 0;

    // the mixer and the recorder's audio
    Mixer mixer;
    Mixer::Meters meters, meterAccum;
    std::array<Mixer::Source, kStrips> sources {};
    std::vector<std::vector<float>> scratch; // tracks (playback), tracks (record), click, outputs not wired
    std::shared_ptr<TakeSink> pendingSink;

    // metronome
    dsp::ClickVoice clickVoice;
    bool metroOn = false;       // CLICK key: running by hand
    int64_t beatIndex = -1;     // the last beat sounded
    double freeBeatPhase = 0.0; // (beats, when free running)
    int64_t tempoLampUntil = 0;
    bool tempoLampRed = false;
    std::vector<int64_t> taps; // (clock, samples)
    // count-in before recording
    int64_t countInLeft = 0, countInBeatLength = 0, countInNext = 0;
    int countInBeats = 0, countInBeatNo = 0;
    bool countInAccentFirst = false;
    int countInBarBeats = 0;

    // MIDI
    int64_t lastMidiPosition = -1;
    bool wasRolling = false;
    bool needSpp = true, needFullFrame = true;
    double clockPhase = 0.0, mtcPhase = 0.0;
    int mtcPiece = 0;
    int64_t mtcFramePos = 0;

    // the paired device
    enum class Pairing : uint8_t { Off, Waiting, Paired } pairing = Pairing::Waiting;
    int64_t pairingSince = 0;
    bool pairingRequest = false;
    juce::String btName;
    std::vector<std::shared_ptr<TakeAudio>> btAudio;
    int64_t btPos = 0;

    // SD PLAY
    std::vector<std::shared_ptr<TakeAudio>> sdAudio;
    std::vector<MusicEntry> sdFiles;
    int sdIndex = -1;
    int64_t sdPos = 0;
    bool sdRolling = false, sdPaused = false;
    int sdRepeat = 0; // 0 none (folder then stop), 1 one file, 2 folder
    bool sdMode = false;

    int64_t vampPendingIn = -1; // (VAMP I/O SET on the footswitch)
    // the effect screen
    float fxUndoValue = -1.0f;
    bool fxTurned = false;
};
} // namespace trackdesk
