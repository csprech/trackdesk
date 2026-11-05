#include "Recorder.h"

namespace trackdesk
{
void Recorder::prepare (double sr) { sampleRate = sr; }

int64_t Recorder::songLength() const
{
    int64_t e = 0;
    for (auto& t : tracks)
        e = std::max (e, t.end());
    return e;
}

void Recorder::clearSong()
{
    for (auto& t : tracks)
        t.regions.clear();
    info = SongInfo();
    position = 0;
    state = State::Stopped;
    undoAvailable = undone = false;
    punchArmed = rehearsal = rehearsingIn = false;
    stopAt = -1;
    vampIndex = -1;
}

std::vector<int64_t> Recorder::locatePoints() const
{
    std::vector<int64_t> p { 0, songLength() };
    if (info.punchIn >= 0)
        p.push_back (info.punchIn);
    if (info.punchOut >= 0)
        p.push_back (info.punchOut);
    for (auto m : info.marks)
        p.push_back (m);
    for (auto& v : info.vamps)
    {
        p.push_back (v.first);
        p.push_back (v.second);
    }
    std::sort (p.begin(), p.end());
    p.erase (std::unique (p.begin(), p.end()), p.end());
    return p;
}

void Recorder::locate (int64_t pos)
{
    position = std::max<int64_t> (0, pos);
    rehearsingIn = false;
}

// ---------------------------------------------------------------- transport

void Recorder::play()
{
    if (state == State::Recording)
        return;
    stopAt = -1;
    if (info.autoPunch && info.punchIn >= 0 && state == State::Stopped)
    {
        // a rehearsal: from the pre-roll point; at the in point we monitor as if recording, without recording
        rehearsal = true;
        punchArmed = false;
        if (preRollSec > 0)
            position = std::max<int64_t> (0, info.punchIn - (int64_t) (preRollSec * sampleRate));
    }
    state = State::Playing;
}

void Recorder::pause()
{
    if (state == State::Playing)
        state = State::Paused;
    else if (state == State::Paused)
        state = State::Playing;
}

void Recorder::stop()
{
    if (state == State::Recording && punchedFromPlay)
    {
        // a manual punch out: back to playback, and stop two seconds later
        endCapture();
        state = State::Playing;
        stopAt = position + (int64_t) (2.0 * sampleRate);
        return;
    }
    if (state == State::Recording)
        endCapture();
    if (passOpen)
        endPass();
    if (state == State::Paused && stopAt < 0)
        position = 0; // STOP while paused: back to the beginning (p.23)
    state = State::Stopped;
    punchArmed = rehearsal = rehearsingIn = false;
    stopAt = -1;
    vampIndex = -1;
}

bool Recorder::record (const std::array<bool, kTracks>& tracksArmed, std::shared_ptr<TakeSink> takeSink)
{
    if (state == State::Recording || takeSink == nullptr)
        return false;
    armed = tracksArmed;
    armed[(size_t) kMixTrack] = armed[(size_t) kMixTrack + 1] = true; // the MAIN MIX is always recorded (p.40)
    sink = std::move (takeSink);
    stopAt = -1;
    if (info.autoPunch && (info.punchIn >= 0 || info.punchOut >= 0) && state == State::Stopped)
    {
        beginPass();
        punchArmed = true;
        rehearsal = false;
        if (info.punchIn >= 0)
        {
            if (preRollSec > 0)
                position = std::max<int64_t> (0, info.punchIn - (int64_t) (preRollSec * sampleRate));
            state = State::Playing; // recording starts at the in point
        }
        else
        {
            startCapture(); // only an out point: record now, punch out there
            state = State::Recording;
        }
        punchedFromPlay = false;
        return true;
    }
    beginPass();
    punchedFromPlay = state == State::Playing || state == State::Paused;
    startCapture();
    state = State::Recording;
    info.lastRecStart = position;
    return true;
}

void Recorder::beginPass()
{
    passOpen = true;
    capturing.fill (false);
    captureStart.fill (0);
    captureLength.fill (0);
}

void Recorder::startCapture()
{
    for (int t = 0; t < kTracks; ++t)
        if (armed[(size_t) t] && ! capturing[(size_t) t] && captureLength[(size_t) t] == 0)
        {
            capturing[(size_t) t] = true;
            captureStart[(size_t) t] = position;
        }
    info.lastRecStart = position;
}

void Recorder::endCapture()
{
    capturing.fill (false);
    info.lastRecEnd = position;
}

void Recorder::endPass()
{
    passOpen = false;
    for (auto& f : finished)
        if (! f.done)
        {
            f.done = true;
            f.sink = sink;
            f.tracks = armed;
            for (int t = 0; t < kTracks; ++t)
                if (captureLength[(size_t) t] == 0)
                    f.tracks[(size_t) t] = false; // armed but never reached the in point
            f.start = captureStart;
            f.length = captureLength;
            break;
        }
    sink.reset();
    armed.fill (false);
}

void Recorder::vampToggle()
{
    if (vampIndex >= 0)
    {
        // leave the vamp and carry on; in manual mode the function turns off (p.59)
        vampIndex = -1;
        if (! vampAuto)
            info.vampOn = manualArmed = false;
        return;
    }
    if (vampAuto)
        info.vampOn = ! info.vampOn;
    else
    {
        // manual: on, and the next vamp in point starts it
        info.vampOn = ! (info.vampOn && manualArmed);
        manualArmed = info.vampOn;
    }
}

// ---------------------------------------------------------------- per block

int Recorder::samplesUntilEvent (int n) const
{
    if (state != State::Playing && state != State::Recording)
        return n;
    int64_t next = position + n;
    auto consider = [&] (int64_t p) {
        if (p > position && p < next)
            next = p;
    };
    if (info.autoPunch)
    {
        if ((punchArmed || rehearsal) && info.punchIn >= 0)
            consider (info.punchIn);
        if (info.punchOut >= 0)
            consider (info.punchOut);
    }
    if (stopAt >= 0)
        consider (stopAt);
    if (vampIndex >= 0)
        consider (info.vamps[(size_t) vampIndex].second);
    else if (info.vampOn && (vampAuto || manualArmed))
        for (auto& v : info.vamps)
            consider (v.first);
    if (repeat != Repeat::Continue && state == State::Playing)
        consider (songLength());
    return (int) std::max<int64_t> (1, next - position);
}

void Recorder::renderPlayback (int n, float* const* out) const
{
    for (int t = 0; t < kTracks; ++t)
        if (out[t] != nullptr)
            std::fill (out[t], out[t] + n, 0.0f);
    if (state != State::Playing && state != State::Recording)
        return;
    const int64_t a = position, b = position + n;
    for (int t = 0; t < kTracks; ++t)
    {
        if (out[t] == nullptr)
            continue;
        for (auto& r : tracks[(size_t) t].regions) // in order, so later passes overwrite earlier ones
        {
            const int64_t from = std::max (a, r.start), to = std::min (b, r.end());
            if (to <= from || r.audio == nullptr)
                continue;
            r.audio->read (r.offset + (from - r.start), out[t] + (from - a), (int) (to - from));
        }
    }
}

void Recorder::capture (int n, const float* const* in)
{
    if (state != State::Recording || sink == nullptr)
        return;
    for (int t = 0; t < kTracks; ++t)
        if (capturing[(size_t) t] && in[t] != nullptr)
        {
            sink->write (t, in[t], n);
            captureLength[(size_t) t] += n;
        }
}

void Recorder::advance (int n)
{
    if (state != State::Playing && state != State::Recording)
        return;
    const int64_t previous = position;
    position += n;
    // auto punch: in, out
    if (info.autoPunch)
    {
        if (info.punchIn >= 0 && position == info.punchIn)
        {
            if (punchArmed && state == State::Playing)
            {
                startCapture();
                state = State::Recording;
            }
            else if (rehearsal)
                rehearsingIn = true;
        }
        if (info.punchOut >= 0 && position == info.punchOut && (state == State::Recording || rehearsingIn))
        {
            if (state == State::Recording)
                endCapture();
            state = State::Playing;
            rehearsingIn = false;
            stopAt = position + (int64_t) (2.0 * sampleRate); // playback carries on two seconds (p.44)
        }
    }
    if (stopAt >= 0 && position >= stopAt)
    {
        const bool wasRehearsal = rehearsal;
        stop();
        juce::ignoreUnused (wasRehearsal);
        state = State::Paused; // the PLAY key blinks (p.44): paused at the stop point
        stopAt = -1;
        return;
    }
    // vamp: loop the range
    if (vampIndex >= 0 && position >= info.vamps[(size_t) vampIndex].second)
        position = info.vamps[(size_t) vampIndex].first;
    else if (info.vampOn && (vampAuto || manualArmed) && vampIndex < 0)
        for (size_t i = 0; i < info.vamps.size(); ++i)
            if (previous <= info.vamps[i].first && info.vamps[i].first <= position) // (reached, or started on, an in point)
            {
                vampIndex = (int) i;
                manualArmed = false;
            }
    // repeat at the end of the song
    const int64_t len = songLength();
    if (state == State::Playing && len > 0 && position >= len)
    {
        if (repeat == Repeat::Loop)
            position = 0;
        else if (repeat == Repeat::PlayStop)
        {
            state = State::Stopped;
            position = len;
        }
    }
}

void Recorder::search (int dir, int n)
{
    if (state == State::Recording)
        return;
    position = std::max<int64_t> (0, position + (int64_t) dir * n * 12);
}

// ---------------------------------------------------------------- passes and edits

bool Recorder::takeFinishedPass (Pass& out)
{
    for (auto& f : finished)
        if (f.done)
        {
            out = f;
            f = Pass();
            return true;
        }
    return false;
}

void Recorder::addPass (const Pass& pass, const std::array<std::shared_ptr<TakeAudio>, kTracks>& audio)
{
    rememberForUndo();
    for (int t = 0; t < kTracks; ++t)
        if (pass.tracks[(size_t) t] && audio[(size_t) t] != nullptr && pass.length[(size_t) t] > 0)
        {
            Region r;
            r.audio = audio[(size_t) t];
            r.start = pass.start[(size_t) t];
            r.offset = 0;
            r.length = std::min (pass.length[(size_t) t], audio[(size_t) t]->length());
            tracks[(size_t) t].regions.push_back (r);
        }
    dirty = true;
}

void Recorder::clearTrack (int track)
{
    rememberForUndo();
    if (track < 0)
        for (auto& t : tracks)
            t.regions.clear();
    else
        tracks[(size_t) track].regions.clear();
    dirty = true;
}

void Recorder::swapTracks (int a, int b, bool pairs)
{
    if (a == b)
        return;
    rememberForUndo();
    std::swap (tracks[(size_t) a], tracks[(size_t) b]);
    if (pairs)
        std::swap (tracks[(size_t) a + 1], tracks[(size_t) b + 1]);
    dirty = true;
}

bool Recorder::importTrack (int track, std::shared_ptr<TakeAudio> audio)
{
    if (audio == nullptr || tracks[(size_t) track].hasAudio())
        return false;
    rememberForUndo();
    tracks[(size_t) track].regions.push_back ({ audio, 0, 0, audio->length() });
    dirty = true;
    return true;
}

void Recorder::rememberForUndo()
{
    undoTracks = tracks;
    undoAvailable = true;
    undone = false;
}

void Recorder::undo()
{
    if (! canUndo())
        return;
    std::swap (tracks, undoTracks);
    undone = true;
    dirty = true;
}

void Recorder::redo()
{
    if (! canRedo())
        return;
    std::swap (tracks, undoTracks);
    undone = false;
    dirty = true;
}
} // namespace trackdesk
