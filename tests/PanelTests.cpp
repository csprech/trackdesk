#include "Rig.h"

using namespace trackdesk;
using namespace trackdesk::test;
using S = Desk::ScreenId;

class PanelTests : public juce::UnitTest
{
public:
    PanelTests() : juce::UnitTest ("Panel", "trackdesk") {}

    static void armStrip (Rig& r, int s, bool on = true) { r.desk.setSwitch (s, StripSwitch::Rec, on); }

    void runTest() override
    {
        beginTest ("start-up: a song is made, the Meter Screen shows; MENU goes Meter > Home > MENU");
        {
            Rig r;
            expect (r.desk.songLoaded());
            expectEquals ((int) r.storage.store.size(), 1);
            expect (r.desk.screen() == S::Meter);
            expect (r.shows ("SEL."));
            r.tap (Key::Menu);
            expect (r.desk.screen() == S::Home);
            expect (r.shows ("00") && r.shows ("METR") && r.shows ("MARK"), r.screen());
            expect (r.shows (r.desk.currentSongName()));
            r.tap (Key::Menu);
            expect (r.desk.screen() == S::List && r.desk.screenTitle() == "MENU");
            expect (r.shows ("SONG") && r.shows ("VAMP"));
            r.tap (Key::Menu);
            expect (r.desk.screen() == S::Home, "MENU from a menu: back home");
            r.tap (Key::F1);
            expect (r.desk.screen() == S::Meter, "F1 on Home: the Meter Screen");
        }

        beginTest ("the Meter Screen: SEL cycles LIVE INPUT, PC RETURN, MTR RETURN; 1-8 and 9-12");
        {
            Rig r;
            r.tap (Key::F1);
            expect (r.shows ("LIVE INPUT"));
            r.tap (Key::F1);
            expect (r.shows ("PC RETURN"));
            r.tap (Key::F1);
            expect (r.shows ("MTR RETURN"));
            r.tap (Key::F1);
            r.tap (Key::F3);
            expect (r.shows ("9/10") && r.shows ("MAIN"));
            r.tap (Key::F2);
            expect (r.shows ("7/8"));
        }

        beginTest ("menus: SOLO MODE through the MIXER menu, F1 cancels, the dial and push confirm");
        {
            Rig r;
            expect (r.menu ({ "MIXER", "SOLO MODE" }));
            expect (r.desk.screen() == S::Choice && r.desk.screenTitle() == "SOLO MODE");
            r.jog (1);
            r.tap (Key::F1);
            expect (r.desk.settings.soloMode == SoloMode::Pfl, "F1 cancelled");
            expect (r.desk.screen() == S::List && r.desk.screenTitle() == "MIXER");
            r.tap (Key::Jog);
            r.jog (1);
            r.tap (Key::Jog);
            expect (r.desk.settings.soloMode == SoloMode::InPlace);
            expect (r.shows ("INPLACE"), "the list shows the value");
            expect (r.desk.takeSettingsDirty());
        }

        beginTest ("every MENU item opens and F1 comes back");
        {
            Rig r;
            const char* top[] = { "SONG", "VAMP", "CLICK", "MTR", "MIDI", "MIXER", "STEREO MIX EXPORT", "SD PLAY", "STORAGE", "SYSTEM" };
            for (auto* item : top)
            {
                expect (r.menu ({ item }), item);
                expect (r.desk.screen() != S::Home && r.desk.screen() != S::Meter, item);
                expect (r.screen().isNotEmpty());
                r.tap (Key::F1);
                if (r.desk.popupOpen())
                    r.tap (Key::F1);
            }
            expect (r.menu ({ "DAW CONTROLLER" }));
            expect (r.desk.popupOpen() && r.desk.popupText().contains ("not available"));
        }

        beginTest ("phantom power: on after a confirmation; off while it's asking cancels");
        {
            Rig r;
            r.desk.setMasterSwitch (MasterSwitch::Phantom, true);
            expect (r.desk.popupOpen() && r.desk.popupText().contains ("Phantom"));
            expect (r.desk.lights().phantom);
            r.desk.setMasterSwitch (MasterSwitch::Phantom, false);
            expect (! r.desk.popupOpen());
            r.desk.setMasterSwitch (MasterSwitch::Phantom, true);
            r.tap (Key::Jog);
            expect (! r.desk.popupOpen());
        }

        beginTest ("recording: REC blinks when armed and lights while recording; the take is saved");
        {
            Rig r;
            r.sine (0, 0.5f, 440.0);
            armStrip (r, 0);
            expect (r.desk.lights().rec[0] == Desk::Lamp::Blink);
            r.tap (Key::Record);
            r.seconds (0.01);
            expect (r.desk.recorder.isRecording());
            expect (r.desk.lights().rec[0] == Desk::Lamp::On && r.desk.lights().record == Desk::Lamp::On);
            expect (r.desk.lights().play == Desk::Lamp::On);
            r.seconds (1.0);
            r.tap (Key::Stop);
            r.seconds (0.05);
            expect (r.desk.lights().stop == Desk::Lamp::On);
            const auto& song = r.storage.store[r.desk.currentSongName()];
            expect (song.tracks[0].hasAudio(), "track 1 saved in the song");
            expect (song.tracks[kMixTrack].hasAudio(), "and the MAIN MIX");
            expect (! song.tracks[1].hasAudio());
            beginTest ("play it back in MTR mode: playback, then playback + input when armed (p.39)");
            r.desk.setMode (0, Mode::Mtr);
            armStrip (r, 0, false);
            r.liveConnected[0] = false;
            r.tap (Key::Rew); // (to zero)
            r.tap (Key::Play);
            expect (r.peak (r.mainL, 0.3) > 0.3f, "the take plays");
            expect (r.desk.sourceOf (0) == Mixer::Source::Playback);
            armStrip (r, 0);
            r.seconds (0.01);
            expect (r.desk.sourceOf (0) == Mixer::Source::InputAndPlayback);
            r.tap (Key::Stop);
            r.seconds (0.01);
            expect (r.desk.sourceOf (0) == Mixer::Source::Input);
            armStrip (r, 0, false);
            r.seconds (0.01);
            expect (r.desk.sourceOf (0) == Mixer::Source::Muted, "stopped, REC off: muted");
        }

        beginTest ("undo from the Home Screen: F3 asks, F4 does it");
        {
            Rig r;
            r.sine (0, 0.5f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (0.5);
            r.tap (Key::Stop);
            r.seconds (0.05);
            r.tap (Key::Menu);
            expect (r.shows ("UNDO"));
            r.tap (Key::F3);
            expect (r.desk.popupOpen());
            r.tap (Key::F4);
            expect (! r.desk.recorder.hasAudio (0), "undone");
            expect (r.shows ("REDO"));
            r.tap (Key::F3);
            r.tap (Key::F4);
            expect (r.desk.recorder.hasAudio (0), "redone");
        }

        beginTest ("marks: F4 while playing; REW/FF skip to them; F4 when stopped on one clears it");
        {
            Rig r;
            r.sine (0, 0.5f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (3.0);
            r.tap (Key::Stop);
            r.seconds (0.05);
            r.tap (Key::Rew); // zero
            r.tap (Key::Menu);
            r.tap (Key::Play);
            r.seconds (1.0);
            r.tap (Key::F4);
            expect (r.shows ("MARK 01"), r.screen());
            r.seconds (0.5);
            r.tap (Key::Stop);
            r.seconds (0.01);
            expectEquals ((int) r.desk.recorder.info.marks.size(), 1);
            expect (r.shows ("*"), "unsaved marks");
            r.tap (Key::Stop); // (paused? no: stopped. REW to zero)
            r.tap (Key::Rew);
            r.tap (Key::Rew);
            expectEquals ((int) r.desk.recorder.getPosition(), 0);
            r.tap (Key::Ff);
            expectWithinAbsoluteError ((double) r.desk.recorder.getPosition() / r.sr, 1.0, 0.02);
            r.tap (Key::F4);
            expect (r.desk.screen() == S::Confirm);
            r.tap (Key::F4);
            expectEquals ((int) r.desk.recorder.info.marks.size(), 0);
            r.tap (Key::Ff);
            expectWithinAbsoluteError ((double) r.desk.recorder.getPosition() / r.sr, 3.0, 0.02);
            r.tap (Key::Rew);
            expectEquals ((int) r.desk.recorder.getPosition(), 0);
            beginTest ("STOP + REW / FF: where recording last started and stopped");
            r.desk.press (Key::Stop);
            r.tap (Key::Ff);
            r.desk.release (Key::Stop);
            expectWithinAbsoluteError ((double) r.desk.recorder.getPosition() / r.sr, 3.0, 0.02);
            r.desk.press (Key::Stop);
            r.tap (Key::Rew);
            r.desk.release (Key::Stop);
            expectEquals ((int) r.desk.recorder.getPosition(), 0);
            beginTest ("REW / FF held: search");
            r.desk.press (Key::Ff);
            r.seconds (1.0);
            r.desk.release (Key::Ff);
            expect (r.desk.recorder.getPosition() > (int64_t) (r.sr * 4.0), "searched forward");
        }

        beginTest ("the metronome: CLICK starts and stops it; it sounds on the CLICK jack; TEMPO flashes");
        {
            Rig r;
            r.tap (Key::Click);
            expect (r.desk.clickRunning());
            bool red = false, green = false;
            float p = 0.0f;
            r.seconds (2.1, [&] (const Rig& rig, int m) {
                for (int i = 0; i < m; ++i)
                    p = std::max (p, std::abs (rig.clickOut[(size_t) i]));
                const auto l = rig.desk.lights().tempo;
                red = red || l == Desk::Lamp::Red;
                green = green || l == Desk::Lamp::Green;
            });
            expect (p > 0.05f, "the click sounds");
            expect (red && green, "red on the first beat, green on the others");
            r.tap (Key::Click);
            expect (! r.desk.clickRunning());
            expect (r.peak (r.clickOut, 0.5) < 1.0e-4f);
        }

        beginTest ("CLICK OUT: COUNT IN+REC counts in, then records");
        {
            Rig r;
            r.desk.recorder.info.metronome.clickOut = ClickOut::CountInRec;
            r.desk.recorder.info.metronome.countIn = CountIn::Beat4;
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (1.0);
            expect (! r.desk.recorder.isRecording(), "counting in (4 beats at 120)");
            expect (r.desk.lights().record == Desk::Lamp::Blink);
            r.seconds (1.05);
            expect (r.desk.recorder.isRecording(), "recording after the count-in");
            expect (r.peak (r.clickOut, 0.6) > 0.05f, "and the click carries on while recording");
            r.tap (Key::Stop);
        }

        beginTest ("tap tempo on the TEMPO screen");
        {
            Rig r;
            expect (r.menu ({ "CLICK", "METRONOME", "TEMPO" }));
            expect (r.desk.screen() == S::Tempo);
            for (int i = 0; i < 4; ++i)
            {
                r.tap (Key::Click);
                r.seconds (0.5);
            }
            expectWithinAbsoluteError (r.desk.recorder.info.metronome.tempo, 120.0, 1.5);
            r.jog (5);
            r.tap (Key::Jog);
            expect (r.desk.recorder.info.metronome.tempo > 123.0);
        }

        beginTest ("MIDI: clock and song position, time code");
        {
            Rig r;
            r.desk.settings.midiClock = true;
            r.desk.settings.midiTimecode = true;
            r.sine (0, 0.5f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (1.0);
            r.tap (Key::Stop);
            r.seconds (0.1);
            int clocks = 0, starts = 0, stops = 0, qf = 0, full = 0;
            for (auto& m : r.midiLog)
            {
                if (m.isMidiClock())
                    ++clocks;
                if (m.isMidiStart())
                    ++starts;
                if (m.isMidiStop())
                    ++stops;
                if (m.isQuarterFrame())
                    ++qf;
                if (m.isFullFrame())
                    ++full;
            }
            expectWithinAbsoluteError (clocks, 48, 1); // 24 a beat, 2 beats a second at 120
            expectEquals (starts, 1);
            expectEquals (stops, 1);
            expectWithinAbsoluteError (qf, 120, 1); // 30 frames x 4
            expect (full >= 1);
        }

        beginTest ("the footswitch: PLAY/PAUSE by default; REC/STOP; normally closed acts on release");
        {
            Rig r;
            r.sine (0, 0.5f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (1.0);
            r.tap (Key::Stop);
            r.seconds (0.05);
            r.tap (Key::Rew);
            r.tap (Key::Foot1);
            expect (r.desk.recorder.getState() == Recorder::State::Playing);
            r.tap (Key::Foot1);
            expect (r.desk.recorder.getState() == Recorder::State::Paused);
            r.tap (Key::Stop);
            r.desk.settings.footFn[0] = FootFn::RecStop;
            r.tap (Key::Foot1);
            r.seconds (0.1);
            expect (r.desk.recorder.isRecording());
            r.tap (Key::Foot1);
            expect (! r.desk.recorder.isRecording());
            r.desk.settings.footFn[0] = FootFn::FxMute;
            r.desk.settings.footNormallyClosed[0] = true;
            r.desk.press (Key::Foot1);
            expect (! r.desk.master.fxMute);
            r.desk.release (Key::Foot1);
            expect (r.desk.master.fxMute);
        }

        beginTest ("the EFFECT screen: type, parameter, on/off, default and undo, the library");
        {
            Rig r;
            r.tap (Key::FxSelect);
            expect (r.desk.screen() == S::Effect);
            expect (r.shows ("1.HALL 1"));
            r.jog (7);
            expectEquals (r.desk.fx.type, 7);
            expect (r.shows ("DELAY 1") && r.shows ("UNDO"));
            r.tap (Key::F4);
            expectEquals (r.desk.fx.type, 0, "UNDO");
            r.jog (7);
            r.tap (Key::Jog); // PARAMETER
            r.jog (10);
            expectWithinAbsoluteError (r.desk.fx.param[7], 0.6f, 0.001f);
            // tapping the delay time with CLICK
            r.tap (Key::Click);
            r.seconds (0.25);
            r.tap (Key::Click);
            expectWithinAbsoluteError (fxParamText (7, r.desk.fx.param[7]).getIntValue(), 250, 6);
            r.tap (Key::F2);
            expect (! r.desk.fx.on && r.shows ("OFF"));
            r.tap (Key::F2);
            r.tap (Key::Jog); // LIB
            r.jog (1);        // slot 1 is empty: it stores the setting
            expect (r.desk.fx.library[0].used && r.desk.fx.library[0].type == 7);
            r.jog (-1);
            r.tap (Key::Jog); // TYPE
            r.jog (-7);
            expectEquals (r.desk.fx.type, 0);
            r.tap (Key::Jog);
            r.tap (Key::Jog); // LIB
            r.jog (1);        // slot 1 loads
            expectEquals (r.desk.fx.type, 7);
            r.tap (Key::F1);
            expect (r.desk.screen() == S::Home);
            r.desk.setAux2JackUsed (true);
            expect (r.shows ("FX.N/A"));
        }

        beginTest ("songs: NEW, LOAD, PROTECT, RENAME, DELETE (not the current one)");
        {
            Rig r;
            const auto first = r.desk.currentSongName();
            expect (r.menu ({ "SONG" }));
            expect (r.desk.screen() == S::Song);
            r.tap (Key::F4);
            expect (r.desk.screen() == S::SongNew);
            r.tap (Key::F4);
            expectEquals ((int) r.storage.store.size(), 2);
            const auto second = r.desk.currentSongName();
            expect (second != first, "the new song is loaded");
            // load the first again
            expect (r.select (first));
            r.tap (Key::Jog);
            expect (r.desk.cursorText() == "LOAD");
            r.tap (Key::Jog);
            expectEquals (r.desk.currentSongName(), first);
            // protect the second, then it can't be deleted or renamed
            expect (r.select (second));
            r.tap (Key::Jog);
            expect (r.select ("PROTECT"));
            r.tap (Key::Jog);
            r.tap (Key::F4);
            expect (r.storage.store[second].info.protect);
            expect (r.select (second));
            r.tap (Key::Jog);
            expect (r.select ("DELETE"));
            r.tap (Key::Jog);
            expect (r.desk.popupOpen() && r.desk.popupText().contains ("Protected"));
            r.tap (Key::F1);
            expect (r.select ("UNPROTECT"));
            r.tap (Key::Jog);
            r.tap (Key::F4);
            expect (! r.storage.store[second].info.protect);
            // rename it: replace the first letter with 'X'
            expect (r.select (second));
            r.tap (Key::Jog);
            expect (r.select ("RENAME"));
            r.tap (Key::Jog);
            expect (r.desk.screen() == S::TextEdit);
            r.desk.press (Key::F2);
            r.desk.release (Key::F2);
            for (int i = 0; i < 20; ++i)
                r.tap (Key::F2);
            const int xAt = juce::String (" !#$%&'()+,-.;=@[]^_{}~0123456789ABCDEFGHIJKLMNOPQRSTUVW").length();
            r.jog (xAt);
            r.tap (Key::Jog);
            r.tap (Key::F4);
            const auto renamed = "X" + second.substring (1);
            expect (r.storage.store.count (renamed) == 1, "renamed to " + renamed);
            // the current song can't be deleted
            expect (r.select (first));
            r.tap (Key::Jog);
            expect (r.select ("DELETE"));
            r.tap (Key::Jog);
            expect (r.desk.popupText().contains ("Current Song"));
            r.tap (Key::F1);
            r.tap (Key::F1);
            expect (r.select (renamed));
            r.tap (Key::Jog);
            expect (r.select ("DELETE"));
            r.tap (Key::Jog);
            r.tap (Key::F4);
            expectEquals ((int) r.storage.store.size(), 1);
        }

        beginTest ("a protected song can't be recorded on");
        {
            Rig r;
            r.storage.setProtected (r.desk.currentSongName(), true);
            r.desk.recorder.info.protect = true;
            armStrip (r, 0);
            r.tap (Key::Record);
            expect (! r.desk.recorder.isRecording());
            expect (r.desk.popupText().contains ("Protected"));
        }

        beginTest ("AUTO PUNCH screen: set points while playing, turn it on; rehearsal and recording");
        {
            Rig r;
            r.sine (0, 0.2f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (6.0);
            r.tap (Key::Stop);
            r.seconds (0.05);
            r.tap (Key::Rew);
            expect (r.menu ({ "MTR", "AUTO PUNCH" }));
            expect (r.desk.screen() == S::AutoPunch);
            r.tap (Key::Play);
            r.seconds (2.0);
            r.tap (Key::Jog); // IN
            r.seconds (0.5);
            r.tap (Key::Jog); // OUT: too short
            expect (r.desk.popupText().contains ("Too Short"));
            r.tap (Key::F1);
            r.seconds (1.0);
            r.tap (Key::Jog); // OUT
            r.tap (Key::Stop);
            r.tap (Key::F4);
            expect (r.desk.recorder.info.autoPunch);
            expectWithinAbsoluteError ((double) r.desk.recorder.info.punchIn / r.sr, 2.0, 0.02);
            expectWithinAbsoluteError ((double) r.desk.recorder.info.punchOut / r.sr, 3.5, 0.02);
            r.tap (Key::Menu);
            expect (r.shows ("A.PUNCH"));
            r.tap (Key::Stop);
            r.tap (Key::Play); // rehearsal from the pre-roll point (2 s before: zero)
            r.seconds (2.5);
            expect (r.desk.recorder.isRehearsal() && r.desk.recorder.isPunchedIn());
            expect (r.desk.lights().record == Desk::Lamp::Blink, "RECORD blinks in a rehearsal");
            r.seconds (3.5);
            expect (r.desk.recorder.getState() == Recorder::State::Paused, "stopped 2 s after the out point");
            r.tap (Key::Stop);
            r.liveSource = [] (int, int64_t) { return 0.9f; };
            r.tap (Key::Record);
            r.seconds (2.5);
            expect (r.desk.recorder.isRecording());
            r.seconds (3.5);
            r.tap (Key::Stop);
            r.seconds (0.05);
            float v = 0.0f;
            for (auto& reg : r.desk.recorder.tracks[0].regions)
                if (reg.start > 0)
                    reg.audio->read (100, &v, 1);
            expectWithinAbsoluteError (v, 0.9f, 0.001f);
        }

        beginTest ("VAMP: set points while playing; recording isn't possible on the vamp screens");
        {
            Rig r;
            r.sine (0, 0.2f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (8.0);
            r.tap (Key::Stop);
            r.seconds (0.05);
            r.tap (Key::Rew);
            expect (r.menu ({ "VAMP", "I/O EDIT" }));
            r.tap (Key::Play);
            r.seconds (1.0);
            r.tap (Key::F2);
            r.seconds (1.0);
            r.tap (Key::F3);
            expect (r.desk.popupText().contains ("Too Short"));
            r.tap (Key::F1);
            r.seconds (1.5);
            r.tap (Key::F3);
            expectEquals ((int) r.desk.recorder.info.vamps.size(), 1);
            r.tap (Key::Record);
            expect (r.desk.popupText().contains ("VAMP MODE"));
            r.tap (Key::F1);
            r.tap (Key::Stop);
            r.tap (Key::F1);
            expect (r.select ("VAMP PLAY"));
            r.tap (Key::Jog);
            expect (r.desk.screen() == S::VampPlay);
            r.desk.recorder.info.vampOn = true;
            r.tap (Key::Rew);
            r.tap (Key::Play);
            r.seconds (5.0);
            expect (r.desk.recorder.isVamping(), "looping the range");
            expect (r.shows ("VAMP-1"));
            r.tap (Key::F4);
            expect (! r.desk.recorder.isVamping());
        }

        beginTest ("TRACK CLEAR and TRACK SWAP");
        {
            Rig r;
            r.sine (0, 0.2f, 440.0);
            armStrip (r, 0);
            r.tap (Key::Record);
            r.seconds (0.5);
            r.tap (Key::Stop);
            r.seconds (0.05);
            expect (r.menu ({ "MTR", "TRACK EDIT", "TRACK SWAP" }));
            r.jog (2);        // A: 3
            r.tap (Key::F4);  // 1 <-> 3? (B is 1)
            expect (r.desk.recorder.hasAudio (2) && ! r.desk.recorder.hasAudio (0));
            r.tap (Key::Jog); // B
            r.jog (8);        // B: 7/8 -> a pair with a single: invalid
            r.tap (Key::F4);
            expect (r.desk.popupText().contains ("Invalid"));
            r.tap (Key::F1);
            r.tap (Key::F1);
            expect (r.select ("TRACK CLEAR"));
            r.tap (Key::Jog);
            r.jog (2);
            r.tap (Key::Jog);
            expect (r.desk.screen() == S::Confirm);
            r.tap (Key::F4);
            expect (! r.desk.recorder.hasAudio (2));
            r.tap (Key::F2);
            r.tap (Key::F4);
            expect (! r.desk.recorder.hasAudio (kMixTrack), "ALL clears 11/12 too");
        }

        beginTest ("IMPORT: a WAV from the MUSIC folder into an empty track");
        {
            Rig r;
            MemStorage::File f;
            f.rate = 48000.0;
            f.bits = 24;
            f.channels = { std::vector<float> (4800, 0.4f) };
            r.storage.musicFiles["loops/bass.wav"] = f;
            MemStorage::File g = f;
            g.rate = 44100.0;
            r.storage.musicFiles["other.wav"] = g;
            expect (r.menu ({ "MTR", "TRACK EDIT", "IMPORT" }));
            expect (r.desk.screen() == S::Import);
            expect (r.select ("other.wav"));
            r.tap (Key::F4);
            expect (r.desk.popupText().contains ("Unmatch"));
            r.tap (Key::F1);
            expect (r.select ("loops"));
            r.tap (Key::Jog);
            expect (r.select ("bass.wav"));
            r.tap (Key::F4);
            expect (r.desk.screen() == S::ImportTrack);
            r.jog (3);
            r.tap (Key::F4);
            expect (r.desk.recorder.hasAudio (3));
            expectEquals ((int) r.desk.recorder.songLength(), 4800);
        }

        beginTest ("SD PLAY: plays through channel 9/10 in MTR mode; can't record");
        {
            Rig r;
            MemStorage::File f;
            f.channels = { std::vector<float> (48000, 0.3f), std::vector<float> (48000, -0.3f) };
            r.storage.musicFiles["song.wav"] = f;
            r.desk.setMode (7, Mode::Mtr);
            r.desk.strips[7].fader = law::kFaderUnity;
            r.desk.master.mainFader = law::kFaderUnity;
            expect (r.menu ({ "SD PLAY" }));
            expect (r.desk.screen() == S::SdPlay);
            r.tap (Key::Jog);
            expect (r.desk.screen() == S::SdPlaying);
            expect (r.peak (r.mainL, 0.2) > 0.25f, "heard on MAIN");
            r.tap (Key::Record);
            expect (r.desk.popupText().contains ("SD PLAY"));
            r.tap (Key::F1);
            r.tap (Key::F1);
            r.tap (Key::F1);
            expect (r.peak (r.mainL, 0.1) < 1.0e-4f, "leaving SD PLAY stops it");
        }

        beginTest ("STEREO MIX EXPORT, STORAGE");
        {
            Rig r;
            expect (r.menu ({ "STEREO MIX EXPORT" }));
            r.tap (Key::F4);
            expect (r.desk.popupText().contains ("MAIN MIX"), "nothing recorded yet");
            r.tap (Key::F1);
            r.tap (Key::F1);
            r.sine (0, 0.2f, 440.0);
            r.tap (Key::Record);
            r.seconds (0.5);
            r.tap (Key::Stop);
            r.seconds (0.05);
            expect (r.menu ({ "STEREO MIX EXPORT" }));
            r.tap (Key::F4);
            expectEquals ((int) r.storage.exported.size(), 1);
            expect (r.desk.screenTitle() == "MENU", "the MENU Screen reopens");
            r.tap (Key::Jog);
            r.tap (Key::F4);
            expect (r.desk.screen() == S::Confirm, "overwrite?");
            r.tap (Key::F3);
            expectEquals ((int) r.storage.exported.size(), 2);
            r.tap (Key::F1);
            expect (r.menu ({ "STORAGE" }));
            r.tap (Key::F4);
            expect (r.storage.revealed);
        }

        beginTest ("pairing: blinks at start-up; PAIRING asks for a file; paired plays into the MAIN MIX");
        {
            Rig r;
            expect (r.desk.lights().pairing == Desk::Lamp::Blink);
            r.tap (Key::Pairing);
            expect (r.desk.takePairingRequest());
            auto a = std::make_shared<MemAudio>();
            a->data.assign (48000, 0.25f);
            r.desk.pairWith ("phone.wav", { a });
            expect (r.desk.lights().pairing == Desk::Lamp::On);
            r.desk.setAssign (Assign::ToMain);
            r.desk.master.mainFader = law::kFaderUnity;
            expect (r.peak (r.mainL, 0.1) > 0.2f);
            r.desk.setAssign (Assign::Off);
            expect (r.peak (r.mainL, 0.1) < 1.0e-4f);
            r.desk.press (Key::Pairing);
            r.seconds (2.2);
            r.desk.release (Key::Pairing);
            expect (r.desk.lights().pairing == Desk::Lamp::Blink, "held: unpaired");
        }

        beginTest ("system: KEY FUNCTION puts the metronome on F4; OUTPUT PAD; INITIALIZE");
        {
            Rig r;
            expect (r.menu ({ "SYSTEM", "KEY FUNCTION", "F4 KEY" }));
            r.jog (1);
            r.tap (Key::Jog);
            expect (r.desk.settings.f4 == F4Fn::Click);
            r.tap (Key::Menu);
            expect (r.shows ("CLIC"));
            r.tap (Key::F4);
            expect (r.desk.screenTitle() == "METRONOME");
            expect (r.menu ({ "SYSTEM", "OUTPUT PAD", "MAIN OUT" }));
            r.jog (-6);
            expectEquals (r.desk.settings.padMainDb, -6);
            r.tap (Key::Jog);
            expect (r.menu ({ "SYSTEM", "INITIALIZE" }));
            r.tap (Key::F4);
            expectEquals (r.desk.settings.padMainDb, 0);
            expect (r.desk.settings.f4 == F4Fn::Mark);
        }

        beginTest ("the display: Home shows the format, the counter moves, repeat icons");
        {
            Rig r;
            r.tap (Key::Menu);
            expect (r.shows ("48/24"));
            r.tap (Key::F2);
            expect (r.shows ("RPT1"));
            r.tap (Key::F2);
            expect (r.desk.recorder.repeat == Repeat::Loop);
            r.tap (Key::F2);
            r.jog (5);
            expectWithinAbsoluteError ((double) r.desk.recorder.getPosition() / r.sr, 5.0, 0.001);
            r.tap (Key::Jog); // direct locate: seconds, minutes, hours
            r.jog (3);
            r.tap (Key::Jog);
            r.jog (1);
            r.tap (Key::Jog);
            r.tap (Key::Jog);
            expectWithinAbsoluteError ((double) r.desk.recorder.getPosition() / r.sr, 68.0, 0.001);
        }

        beginTest ("lights: POST, PFL/AFL, MUTE");
        {
            Rig r;
            expect (! r.desk.lights().post && ! r.desk.lights().pflAfl);
            r.desk.settings.aux1PostFader = true;
            r.desk.setSwitch (3, StripSwitch::Solo, true);
            r.desk.setSwitch (2, StripSwitch::Mute, true);
            const auto l = r.desk.lights();
            expect (l.post && l.pflAfl && l.mute[2] && ! l.mute[3]);
        }
    }
};

static PanelTests panelTests;
