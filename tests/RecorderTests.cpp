#include "Rig.h"

using namespace trackdesk;
using namespace trackdesk::test;

namespace
{
/** Drives a recorder like the console does: segments up to each event, capture, advance. */
struct Tape
{
    double sr = 1000.0; // (a low rate keeps the numbers readable: 1 sample = 1 ms)
    Recorder r;
    std::array<std::vector<float>, kTracks> pb;
    /** What each track gets: track t records the value `level[t]` while recording. */
    std::array<float, kTracks> level {};
    std::vector<std::pair<int64_t, float>> heard; // (position, track 0 playback) per segment start

    Tape()
    {
        r.prepare (sr);
        r.info.sampleRate = sr;
        for (auto& v : pb)
            v.assign (64, 0.0f);
    }
    void run (int64_t n)
    {
        while (n > 0)
        {
            int m = (int) std::min<int64_t> (n, 64);
            m = r.samplesUntilEvent (m);
            std::array<float*, kTracks> out {};
            for (int t = 0; t < kTracks; ++t)
                out[(size_t) t] = pb[(size_t) t].data();
            r.renderPlayback (m, out.data());
            heard.push_back ({ r.getPosition(), pb[0][0] });
            std::array<std::vector<float>, kTracks> in;
            std::array<const float*, kTracks> ip {};
            for (int t = 0; t < kTracks; ++t)
            {
                in[(size_t) t].assign ((size_t) m, level[(size_t) t]);
                ip[(size_t) t] = in[(size_t) t].data();
            }
            r.capture (m, ip.data());
            r.advance (m);
            n -= m;
        }
    }
    /** Collects finished passes like the console does. */
    int collect()
    {
        int count = 0;
        Recorder::Pass p;
        while (r.takeFinishedPass (p))
        {
            std::array<std::shared_ptr<TakeAudio>, kTracks> audio {};
            for (int t = 0; t < kTracks; ++t)
                audio[(size_t) t] = p.sink->finish (t);
            r.addPass (p, audio);
            ++count;
        }
        return count;
    }
    float at (int track, int64_t pos)
    {
        const auto keep = r.getPosition();
        const auto st = r.getState();
        juce::ignoreUnused (st);
        float v = 0.0f;
        for (auto& reg : r.tracks[(size_t) track].regions)
            if (pos >= reg.start && pos < reg.end())
                reg.audio->read (reg.offset + pos - reg.start, &v, 1);
        juce::ignoreUnused (keep);
        return v;
    }
    static std::array<bool, kTracks> arm (std::initializer_list<int> tracks)
    {
        std::array<bool, kTracks> a {};
        for (int t : tracks)
            a[(size_t) t] = true;
        return a;
    }
};
} // namespace

class RecorderTests : public juce::UnitTest
{
public:
    RecorderTests() : juce::UnitTest ("Recorder", "trackdesk") {}

    void runTest() override
    {
        beginTest ("record from stop, then play it back; 11/12 always record");
        {
            Tape tp;
            tp.level[0] = 0.5f;
            tp.level[kMixTrack] = 0.25f;
            expect (tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>()));
            expect (tp.r.isRecording());
            tp.run (1000);
            tp.r.stop();
            expectEquals (tp.collect(), 1);
            expect (tp.r.tracks[0].hasAudio() && tp.r.tracks[kMixTrack].hasAudio() && tp.r.tracks[kMixTrack + 1].hasAudio());
            expect (! tp.r.tracks[1].hasAudio(), "unarmed tracks stay empty");
            expectEquals ((int) tp.r.songLength(), 1000);
            expectEquals ((int) tp.r.info.lastRecStart, 0);
            expectEquals ((int) tp.r.info.lastRecEnd, 1000);
            expectWithinAbsoluteError (tp.at (0, 500), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (kMixTrack, 500), 0.25f, 1.0e-6f);
            expect (tp.r.canUndo());
        }

        beginTest ("STOP while paused returns to zero; PLAY pauses and resumes");
        {
            Tape tp;
            tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>());
            tp.run (2000);
            tp.r.stop();
            tp.collect();
            tp.r.locate (500);
            tp.r.play();
            tp.run (100);
            tp.r.pause();
            expect (tp.r.getState() == Recorder::State::Paused);
            expectEquals ((int) tp.r.getPosition(), 600);
            tp.run (100);
            expectEquals ((int) tp.r.getPosition(), 600, "paused: it doesn't move");
            tp.r.stop();
            expectEquals ((int) tp.r.getPosition(), 0);
        }

        beginTest ("a manual punch in plays over the old take; STOP punches out and stops 2 s later");
        {
            Tape tp;
            tp.level[0] = 0.1f;
            tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>());
            tp.run (5000);
            tp.r.stop();
            tp.collect();
            tp.r.locate (0);
            tp.r.play();
            tp.run (1000);
            tp.level[0] = 0.9f;
            expect (tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>()));
            tp.run (1000);
            tp.r.stop(); // punch out at 2000
            expect (tp.r.getState() == Recorder::State::Playing, "back to playback");
            tp.run (1999);
            expect (tp.r.getState() == Recorder::State::Playing);
            tp.run (1);
            expect (tp.r.getState() == Recorder::State::Paused, "stops (PLAY blinks) 2 s after the punch out");
            expectEquals (tp.collect(), 1);
            expectWithinAbsoluteError (tp.at (0, 500), 0.1f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (0, 1500), 0.9f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (0, 2500), 0.1f, 1.0e-6f);
            beginTest ("undo and redo the punch");
            tp.r.undo();
            expectWithinAbsoluteError (tp.at (0, 1500), 0.1f, 1.0e-6f);
            expect (tp.r.canRedo());
            tp.r.redo();
            expectWithinAbsoluteError (tp.at (0, 1500), 0.9f, 1.0e-6f);
        }

        beginTest ("auto punch: pre-roll, in and out at the points, a rehearsal records nothing");
        {
            Tape tp;
            tp.level[0] = 0.1f;
            tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>());
            tp.run (10000);
            tp.r.stop();
            tp.collect();
            tp.r.info.punchIn = 4000;
            tp.r.info.punchOut = 6000;
            tp.r.info.autoPunch = true;
            tp.r.preRollSec = 2;
            // rehearsal (PLAY)
            tp.r.locate (0);
            tp.r.play();
            expectEquals ((int) tp.r.getPosition(), 2000, "from the pre-roll point");
            tp.run (2000);
            expect (tp.r.isPunchedIn() && ! tp.r.isRecording(), "rehearsing between the points");
            tp.run (2000);
            expect (! tp.r.isPunchedIn());
            tp.run (2000);
            expect (tp.r.getState() == Recorder::State::Paused, "stopped 2 s after the out point");
            expectEquals (tp.collect(), 0, "nothing recorded");
            // the real thing (RECORD)
            tp.r.stop();
            tp.level[0] = 0.7f;
            expect (tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>()));
            expect (! tp.r.isRecording(), "playing up to the in point");
            tp.run (2000);
            expect (tp.r.isRecording());
            tp.run (2000);
            expect (! tp.r.isRecording());
            tp.run (2000);
            tp.r.stop();
            expectEquals (tp.collect(), 1);
            expectWithinAbsoluteError (tp.at (0, 3999), 0.1f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (0, 4000), 0.7f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (0, 5999), 0.7f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (0, 6000), 0.1f, 1.0e-6f);
        }

        beginTest ("repeat: play then stop, or loop");
        {
            Tape tp;
            tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>());
            tp.run (1000);
            tp.r.stop();
            tp.collect();
            tp.r.repeat = Repeat::PlayStop;
            tp.r.locate (0);
            tp.r.play();
            tp.run (1500);
            expect (tp.r.getState() == Recorder::State::Stopped);
            tp.r.repeat = Repeat::Loop;
            tp.r.locate (0);
            tp.r.play();
            tp.run (1500);
            expect (tp.r.getState() == Recorder::State::Playing);
            expectEquals ((int) tp.r.getPosition(), 500, "looped to the start");
        }

        beginTest ("vamp: automatic loops at each range until F4; manual waits for F4");
        {
            Tape tp;
            tp.r.record (Tape::arm ({ 0 }), std::make_shared<MemSink>());
            tp.run (10000);
            tp.r.stop();
            tp.collect();
            tp.r.info.vamps = { { 2000, 4000 }, { 6000, 8000 } };
            tp.r.info.vampOn = true;
            tp.r.vampAuto = true;
            tp.r.locate (0);
            tp.r.play();
            tp.run (5000); // 0 -> 2000, then loops 2000..4000
            expect (tp.r.isVamping());
            expectEquals ((int) tp.r.getPosition(), 3000);
            tp.r.vampToggle(); // carry on
            tp.run (2000);
            expect (! tp.r.isVamping());
            tp.run (2000); // into the second range
            expect (tp.r.isVamping(), "the next range starts by itself");
            // manual
            tp.r.stop();
            tp.r.vampAuto = false;
            tp.r.info.vampOn = false;
            tp.r.locate (0);
            tp.r.play();
            tp.run (5000);
            expect (! tp.r.isVamping(), "manual: nothing until F4");
            tp.r.vampToggle(); // on: arms the next range
            expect (tp.r.info.vampOn);
            tp.run (2000); // 5000 -> 7000: 6000 starts it
            expect (tp.r.isVamping());
            tp.r.vampToggle(); // leave: the function turns off
            expect (! tp.r.info.vampOn && ! tp.r.isVamping());
        }

        beginTest ("track edits: clear, swap (pairs with pairs), import into an empty track");
        {
            Tape tp;
            tp.level[0] = 0.2f;
            tp.level[6] = 0.6f;
            tp.level[7] = 0.7f;
            tp.r.record (Tape::arm ({ 0, 6, 7 }), std::make_shared<MemSink>());
            tp.run (100);
            tp.r.stop();
            tp.collect();
            tp.r.swapTracks (0, 3, false);
            expect (! tp.r.tracks[0].hasAudio() && tp.r.tracks[3].hasAudio());
            tp.r.swapTracks (6, 8, true);
            expectWithinAbsoluteError (tp.at (8, 50), 0.6f, 1.0e-6f);
            expectWithinAbsoluteError (tp.at (9, 50), 0.7f, 1.0e-6f);
            tp.r.clearTrack (3);
            expect (! tp.r.tracks[3].hasAudio());
            tp.r.undo();
            expect (tp.r.tracks[3].hasAudio(), "undo a clear");
            auto a = std::make_shared<MemAudio>();
            a->data.assign (300, 0.3f);
            expect (tp.r.importTrack (1, a));
            expect (! tp.r.importTrack (1, a), "not into a recorded track");
            expectEquals ((int) tp.r.songLength(), 300);
        }

        beginTest ("locate points: zero, end, punch points, marks, vamps");
        {
            Tape tp;
            tp.r.info.marks = { 300, 100 };
            tp.r.info.punchIn = 200;
            auto p = tp.r.locatePoints();
            expectEquals ((int) p.size(), 4);
            expectEquals ((int) p[1], 100);
        }
    }
};

static RecorderTests recorderTests;
