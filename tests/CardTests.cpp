#include "engine/CardFolder.h"

using namespace trackdesk;

class CardTests : public juce::UnitTest
{
public:
    CardTests() : juce::UnitTest ("Card folder", "trackdesk") {}

    static void writeWav (const juce::File& f, double rate, int bits, int channels, int length, float value)
    {
        f.getParentDirectory().createDirectory();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (channels).withBitsPerSample (bits));
        juce::AudioBuffer<float> b (channels, length);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < length; ++i)
                b.setSample (c, i, c == 0 ? value : -value);
        w->writeFromAudioSampleBuffer (b, 0, length);
    }

    void runTest() override
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("trackdesk-card-test");
        root.deleteRecursively();
        {
            CardFolder card (root);
            card.setDeviceRate (48000.0);
            card.setUseTrash (false);

            beginTest ("songs: create, name, list");
            Settings st;
            const auto name = card.newSongName (st);
            expect (name.length() == 11 && name.containsChar ('_'), name);
            SongInfo info;
            info.name = name;
            info.sampleRate = 48000.0;
            info.bits = 24;
            info.metronome.tempo = 133.0;
            expect (card.createSong (info));
            expect (! card.createSong (info), "not twice");
            expect (card.newSongName (st) != name, "the next name is new");
            st.nameByDate = false;
            st.nameWord = "TAKE";
            expect (card.newSongName (st).startsWith ("TAKE_0001"));
            auto list = card.songs();
            expectEquals ((int) list.size(), 1);
            expectEquals (list[0].bits, 24);

            beginTest ("recording a pass: background writers, then memory-mapped takes");
            Recorder r;
            r.prepare (48000.0);
            expect (card.loadSong (name, r));
            expectEquals (r.info.metronome.tempo, 133.0);
            auto sink = card.newTake (r.info);
            expect (sink != nullptr);
            std::array<bool, kTracks> armed {};
            armed[2] = true;
            expect (r.record (armed, sink));
            std::vector<float> block (480, 0.25f), mix (480, -0.5f);
            for (int b = 0; b < 100; ++b) // one second
            {
                std::array<const float*, kTracks> in {};
                in[2] = block.data();
                in[kMixTrack] = in[kMixTrack + 1] = mix.data();
                r.capture (480, in.data());
                r.advance (480);
            }
            r.stop();
            Recorder::Pass p;
            expect (r.takeFinishedPass (p));
            std::array<std::shared_ptr<TakeAudio>, kTracks> audio {};
            for (int t = 0; t < kTracks; ++t)
                audio[(size_t) t] = p.sink->finish (t);
            expect (audio[2] != nullptr && audio[kMixTrack] != nullptr && audio[0] == nullptr);
            r.addPass (p, audio);
            expect (card.saveSong (r));
            const auto wavs = card.songFolder (name).findChildFiles (juce::File::findFiles, false, "*.wav");
            expectEquals (wavs.size(), 3, "tracks 3, 11, 12 (the unused files are removed)");
            float v = 0.0f;
            audio[2]->read (24000, &v, 1);
            expectWithinAbsoluteError (v, 0.25f, 1.0e-4f);

            beginTest ("loading it back: the regions and their audio");
            Recorder again;
            again.prepare (48000.0);
            expect (card.loadSong (name, again));
            expect (again.tracks[2].hasAudio() && again.tracks[kMixTrack].hasAudio() && ! again.tracks[0].hasAudio());
            expectEquals ((int) again.songLength(), 48000);
            std::vector<float> out (100);
            renderTrack (again.tracks[kMixTrack], 1000, out.data(), 100);
            expectWithinAbsoluteError (out[50], -0.5f, 1.0e-4f);

            beginTest ("protect, marks, rename, delete");
            again.info.marks = { 100, 200 };
            expect (card.saveSong (again));
            expect (card.clearMarks (name));
            expect (card.setProtected (name, true));
            Recorder third;
            expect (card.loadSong (name, third));
            expect (third.info.protect && third.info.marks.empty());
            expect (card.renameSong (name, "MYSONG"));
            expect (card.songFolder ("MYSONG").isDirectory() && ! card.songFolder (name).exists());
            Recorder fourth;
            expect (card.loadSong ("MYSONG", fourth));
            expect (fourth.tracks[2].hasAudio(), "the takes moved with it");

            beginTest ("the MUSIC folder: listing, SD PLAY, IMPORT (with a stereo file summed), STEREO MIX EXPORT");
            writeWav (card.musicFolder().getChildFile ("loop.wav"), 48000.0, 16, 2, 4800, 0.4f);
            writeWav (card.musicFolder().getChildFile ("sub/inner.wav"), 44100.0, 24, 1, 4410, 0.2f);
            auto top = card.music ({});
            expectEquals ((int) top.size(), 2);
            expect (top[0].folder && top[0].name == "sub");
            expect (top[1].name == "loop.wav" && top[1].channels == 2 && top[1].bits == 16);
            auto inner = card.music ("sub");
            expectEquals ((int) inner.size(), 1);
            expectEquals (inner[0].sampleRate, 44100.0);
            auto play = card.openMusic ("loop.wav");
            expectEquals ((int) play.size(), 2);
            play[1]->read (100, &v, 1);
            expectWithinAbsoluteError (v, -0.4f, 1.0e-3f);
            auto converted = card.openMusic ("sub/inner.wav"); // 44.1 kHz on a 48 kHz device: converted
            expect (converted.size() == 1 && std::abs ((int) converted[0]->length() - 4800) <= 2);
            auto imported = card.importAudio (fourth.info, "loop.wav", -1);
            expect (imported != nullptr);
            imported->read (100, &v, 1);
            expectWithinAbsoluteError (v, 0.0f, 1.0e-3f); // (L and R cancel when summed)
            expect (fourth.importTrack (5, imported));
            expect (! card.exportMixExists (fourth.info));
            expect (card.exportMix (fourth));
            expect (card.exportMixExists (fourth.info));
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> mixReader (wav.createReaderFor (card.musicFolder().getChildFile ("MYSONG_2-MIX_01.wav").createInputStream().release(), true));
            expect (mixReader != nullptr && mixReader->numChannels == 2 && mixReader->lengthInSamples == 48000);

            beginTest ("format: everything gone, the folders back");
            expect (card.deleteSong ("MYSONG"));
            expect (card.songs().empty());
            expect (card.format());
            expect (card.music ({}).empty() && root.getChildFile ("MTR").isDirectory());
            expect (card.freeBytes() > 0 && card.totalBytes() > 0);
        }
        root.deleteRecursively();
    }
};

static CardTests cardTests;
