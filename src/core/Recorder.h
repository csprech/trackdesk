#pragma once

// The 12-track recorder: the song's tracks, the transport and its punch, repeat and vamp logic.
//
// A track is a list of regions, each a stretch of a recorded pass ("take"); later regions play over earlier ones.
// That makes punching in (a new short region), undoing (the region lists as they were) and swapping tracks
// simple. The audio of a region comes from a TakeAudio (a WAV file in the app, memory in the tests).

#include "Model.h"
#include <memory>

namespace trackdesk
{
/** Recorded audio, read in real time. */
struct TakeAudio
{
    virtual ~TakeAudio() = default;
    virtual int64_t length() const = 0;
    virtual void read (int64_t start, float* dest, int n) const = 0;
    juce::String file; // its file name in the song folder (empty for memory)
};

/** Where a pass is written while it's recorded: write() is called on the audio thread (and must not block or
    allocate); finish() on the message thread once the pass has ended, returning what was recorded. */
struct TakeSink
{
    virtual ~TakeSink() = default;
    virtual void write (int track, const float* samples, int n) = 0;
    virtual std::shared_ptr<TakeAudio> finish (int track) = 0;
};

struct Region
{
    std::shared_ptr<const TakeAudio> audio;
    int64_t start = 0;  // where it plays in the song
    int64_t offset = 0; // into the audio
    int64_t length = 0;
    int64_t end() const { return start + length; }
};

struct Track
{
    std::vector<Region> regions; // later ones play over earlier ones
    bool hasAudio() const { return ! regions.empty(); }
    int64_t end() const
    {
        int64_t e = 0;
        for (auto& r : regions)
            e = std::max (e, r.end());
        return e;
    }
};

class Recorder
{
public:
    enum class State
    {
        Stopped,
        Playing,
        Paused,
        Recording
    };

    void prepare (double sampleRate);
    double getSampleRate() const { return sampleRate; }

    // ---- the song
    std::array<Track, kTracks> tracks;
    SongInfo info;
    int64_t songLength() const;
    void clearSong();

    // ---- transport (called under the console's lock)
    State getState() const { return state; }
    int64_t getPosition() const { return position; }
    void locate (int64_t pos);
    /** Play from here (or, with auto punch on, from the pre-roll point: a rehearsal). */
    void play();
    void pause();
    /** Stop. After a punch-in it punches out and stops two seconds later (p.43). */
    void stop();
    /** Record on these tracks (11/12 always): from stopped it starts recording at once; while playing it punches
        in; with auto punch on it plays from the pre-roll point and punches in at the in point. */
    bool record (const std::array<bool, kTracks>& armed, std::shared_ptr<TakeSink> sink);
    bool isRecording() const { return state == State::Recording; }
    /** In auto punch, between the in and out points (rehearsal or recording): monitor as recording. */
    bool isPunchedIn() const { return state == State::Recording || rehearsingIn; }
    bool isRehearsal() const { return rehearsal; }
    bool isArmedForAutoPunch() const { return punchArmed; }
    Repeat repeat = Repeat::Continue;
    int preRollSec = 2;

    // ---- vamp
    bool vampAuto = true;
    bool isVamping() const { return vampIndex >= 0; }
    bool isVampArmed() const { return manualArmed; }
    /** F4 on the VAMP PLAY screen: leaves the current vamp; otherwise turns the function on or off (in manual mode,
        on means the next vamp in point starts a vamp). */
    void vampToggle();

    // ---- per block (audio thread)
    /** How many samples (up to n) until the transport has something to do (a punch point, a loop, a stop). */
    int samplesUntilEvent (int n) const;
    /** Reads the tracks at the current position (silence where nothing's recorded, or when not rolling). */
    void renderPlayback (int n, float* const* out) const;
    /** Writes what's being recorded on the armed tracks (in[track], may be null). */
    void capture (int n, const float* const* in);
    /** Moves on n samples and does what's due at the new position. */
    void advance (int n);
    /** Searching (REW/FF held): moves without playing. */
    void search (int direction, int n);

    // ---- recorded passes, edits, undo (message thread, under the lock)
    /** A pass that has ended: the console collects its audio from the sink and adds it as regions. */
    struct Pass
    {
        bool done = false;
        std::shared_ptr<TakeSink> sink;
        std::array<bool, kTracks> tracks {};
        std::array<int64_t, kTracks> start {}, length {};
    };
    bool takeFinishedPass (Pass& out);
    void addPass (const Pass& pass, const std::array<std::shared_ptr<TakeAudio>, kTracks>& audio);
    void clearTrack (int track); // -1 = all
    /** Swaps tracks a and b; a pair (7/8, 9/10, 11/12) swaps with a pair. */
    void swapTracks (int a, int b, bool pairs);
    /** Imports audio into an empty track (from the start). */
    bool importTrack (int track, std::shared_ptr<TakeAudio> audio);
    void rememberForUndo();
    bool canUndo() const { return undoAvailable && ! undone; }
    bool canRedo() const { return undoAvailable && undone; }
    void undo();
    void redo();
    /** After SAVE on the song screen, undo and redo are no longer possible (p.36). */
    void forgetUndo() { undoAvailable = undone = false; }
    /** Which tracks have recordings. */
    bool hasAudio (int track) const { return tracks[(size_t) track].hasAudio(); }
    /** Leaves vamp playback (VAMP OFF, or recording). */
    void leaveVamp() { vampIndex = -1; }
    int currentVamp() const { return vampIndex; }
    /** Something new to save in the song (passes, edits, marks). */
    bool takeDirty()
    {
        const bool d = dirty;
        dirty = false;
        return d;
    }
    void markDirty() { dirty = true; }

    /** Points REW / FF can locate to: zero, the end, punch points, marks, vamp points. */
    std::vector<int64_t> locatePoints() const;

private:
    void beginPass();
    void endPass();
    void startCapture();
    void endCapture();

    double sampleRate = 48000.0;
    State state = State::Stopped;
    int64_t position = 0;
    // a pass being recorded
    std::shared_ptr<TakeSink> sink;
    std::array<bool, kTracks> armed {}, capturing {};
    std::array<int64_t, kTracks> captureStart {}, captureLength {};
    bool passOpen = false, punchedFromPlay = false;
    std::array<Pass, 4> finished;
    // auto punch
    bool punchArmed = false, rehearsal = false, rehearsingIn = false;
    int64_t stopAt = -1; // stop two seconds after punching out
    int vampIndex = -1;
    bool manualArmed = false;
    // undo
    std::array<Track, kTracks> undoTracks;
    bool undoAvailable = false, undone = false, dirty = false;
};
} // namespace trackdesk
