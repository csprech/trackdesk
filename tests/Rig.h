#pragma once

// A test rig: the console with an in-memory song folder, inputs to feed and outputs to read.

#include "core/Desk.h"
#include <map>

namespace trackdesk::test
{
struct MemAudio : TakeAudio
{
    std::vector<float> data;
    int64_t length() const override { return (int64_t) data.size(); }
    void read (int64_t start, float* dest, int n) const override
    {
        for (int i = 0; i < n; ++i)
        {
            const int64_t k = start + i;
            dest[i] = k >= 0 && k < (int64_t) data.size() ? data[(size_t) k] : 0.0f;
        }
    }
};

struct MemSink : TakeSink
{
    std::array<std::vector<float>, kTracks> buf;
    void write (int track, const float* x, int n) override { buf[(size_t) track].insert (buf[(size_t) track].end(), x, x + n); }
    std::shared_ptr<TakeAudio> finish (int track) override
    {
        if (buf[(size_t) track].empty())
            return {};
        auto a = std::make_shared<MemAudio>();
        a->data = buf[(size_t) track];
        return a;
    }
};

struct MemStorage : Storage
{
    struct Song
    {
        SongInfo info;
        std::array<Track, kTracks> tracks;
        juce::Time written;
    };
    std::map<juce::String, Song> store;
    struct File
    {
        double rate = 48000.0;
        int bits = 24;
        std::vector<std::vector<float>> channels;
    };
    std::map<juce::String, File> musicFiles; // path -> audio ("folder/name.wav")
    std::vector<juce::String> exported;
    int songCounter = 0;
    int64_t free = (int64_t) 8 << 30;
    bool revealed = false;

    std::vector<SongSummary> songs() override
    {
        std::vector<SongSummary> v;
        for (auto& [name, s] : store)
            v.push_back ({ name, s.info.sampleRate, s.info.bits, s.info.protect, 1000, s.written });
        return v;
    }
    juce::String newSongName (const Settings& st) override
    {
        return (st.nameByDate ? juce::String ("261104") : st.nameWord) + "_" + juce::String (++songCounter).paddedLeft ('0', 4);
    }
    bool createSong (const SongInfo& info) override
    {
        if (store.count (info.name) != 0)
            return false;
        store[info.name] = { info, {}, juce::Time::getCurrentTime() };
        return true;
    }
    bool loadSong (const juce::String& name, Recorder& r) override
    {
        auto it = store.find (name);
        if (it == store.end())
            return false;
        r.info = it->second.info;
        r.tracks = it->second.tracks;
        return true;
    }
    bool saveSong (const Recorder& r) override
    {
        auto it = store.find (r.info.name);
        if (it == store.end())
            return false;
        it->second.info = r.info;
        it->second.tracks = r.tracks;
        it->second.written = juce::Time::getCurrentTime();
        return true;
    }
    bool deleteSong (const juce::String& name) override { return store.erase (name) > 0; }
    bool renameSong (const juce::String& from, const juce::String& to) override
    {
        auto it = store.find (from);
        if (it == store.end() || store.count (to) != 0)
            return false;
        auto s = it->second;
        s.info.name = to;
        store.erase (it);
        store[to] = s;
        return true;
    }
    bool setProtected (const juce::String& name, bool p) override
    {
        auto it = store.find (name);
        if (it == store.end())
            return false;
        it->second.info.protect = p;
        return true;
    }
    bool clearMarks (const juce::String& name) override
    {
        auto it = store.find (name);
        if (it == store.end())
            return false;
        it->second.info.marks.clear();
        return true;
    }
    std::shared_ptr<TakeSink> newTake (const SongInfo&) override { return std::make_shared<MemSink>(); }
    std::vector<MusicEntry> music (const juce::String& folder) override
    {
        std::vector<MusicEntry> v;
        juce::StringArray folders;
        for (auto& [path, f] : musicFiles)
        {
            const auto dir = path.containsChar ('/') ? path.upToLastOccurrenceOf ("/", false, false) : juce::String();
            if (dir == folder)
                v.push_back ({ path.fromLastOccurrenceOf ("/", false, false), path, false, f.rate, f.bits, (int) f.channels.size(),
                               (int64_t) f.channels[0].size() });
            else if (folder.isEmpty() && dir.isNotEmpty() && ! folders.contains (dir))
                folders.add (dir);
        }
        for (auto& d : folders)
            v.insert (v.begin(), { d, d, true });
        return v;
    }
    std::vector<std::shared_ptr<TakeAudio>> openMusic (const juce::String& path) override
    {
        std::vector<std::shared_ptr<TakeAudio>> v;
        auto it = musicFiles.find (path);
        if (it == musicFiles.end())
            return v;
        for (auto& ch : it->second.channels)
        {
            auto a = std::make_shared<MemAudio>();
            a->data = ch;
            v.push_back (a);
        }
        return v;
    }
    std::shared_ptr<TakeAudio> importAudio (const SongInfo&, const juce::String& path, int channel) override
    {
        auto it = musicFiles.find (path);
        if (it == musicFiles.end())
            return {};
        auto a = std::make_shared<MemAudio>();
        const auto& chs = it->second.channels;
        if (channel < 0) // both channels summed
        {
            a->data = chs[0];
            for (size_t i = 0; i < a->data.size() && chs.size() > 1; ++i)
                a->data[i] = 0.5f * (chs[0][i] + chs[1][i]);
        }
        else
            a->data = chs[(size_t) std::min<int> (channel, (int) chs.size() - 1)];
        return a;
    }
    bool exportMixExists (const SongInfo& s) override { return std::find (exported.begin(), exported.end(), s.name) != exported.end(); }
    bool exportMix (const Recorder& r) override
    {
        exported.push_back (r.info.name);
        return true;
    }
    bool format() override
    {
        store.clear();
        musicFiles.clear();
        return true;
    }
    int64_t freeBytes() override { return free; }
    int64_t totalBytes() override { return (int64_t) 16 << 30; }
    void reveal() override { revealed = true; }
};

/** The console, fed and read a block at a time. */
struct Rig
{
    static constexpr int kBlock = 256;
    double sr;
    MemStorage storage;
    Desk desk;
    std::array<std::vector<float>, kInputs> live, pc;
    std::vector<float> mainL, mainR, subL, subR, aux1, aux2, p1L, p1R, p2L, p2R, clickOut;
    std::array<std::vector<float>, 12> usb;
    juce::MidiBuffer midi;
    std::vector<juce::MidiMessage> midiLog;
    /** Generates the input on jack i at sample t (absolute). */
    std::function<float (int input, int64_t t)> liveSource, pcSource;
    int64_t t = 0;
    std::array<bool, kInputs> liveConnected {}, pcConnected {};

    explicit Rig (double sampleRate = 48000.0, bool startUp = true) : sr (sampleRate)
    {
        desk.setStorage (&storage);
        desk.prepare (sr, kBlock);
        for (auto* v : { &mainL, &mainR, &subL, &subR, &aux1, &aux2, &p1L, &p1R, &p2L, &p2R, &clickOut })
            v->assign (kBlock, 0.0f);
        for (auto& v : live)
            v.assign (kBlock, 0.0f);
        for (auto& v : pc)
            v.assign (kBlock, 0.0f);
        for (auto& v : usb)
            v.assign (kBlock, 0.0f);
        // levels up: faders at 0 dB (the console starts with them down, as the manual asks)
        for (int i = 0; i < kStrips; ++i)
            desk.setKnob (i, StripKnob::Fader, law::kFaderUnity);
        desk.setMasterKnob (MasterKnob::MainFader, law::kFaderUnity);
        desk.setMasterKnob (MasterKnob::Phones1, 0.75f);
        if (startUp)
            desk.startUp ({});
    }

    /** Runs n samples; `each` sees every block's outputs. */
    void run (int64_t n, std::function<void (const Rig&, int)> each = {})
    {
        while (n > 0)
        {
            const int m = (int) std::min<int64_t> (n, kBlock);
            Desk::AudioIO io;
            for (int i = 0; i < kInputs; ++i)
            {
                if (liveSource && liveConnected[(size_t) i])
                {
                    for (int k = 0; k < m; ++k)
                        live[(size_t) i][(size_t) k] = liveSource (i, t + k);
                    io.live[(size_t) i] = live[(size_t) i].data();
                }
                if (pcSource && pcConnected[(size_t) i])
                {
                    for (int k = 0; k < m; ++k)
                        pc[(size_t) i][(size_t) k] = pcSource (i, t + k);
                    io.pc[(size_t) i] = pc[(size_t) i].data();
                }
            }
            io.mainL = mainL.data();
            io.mainR = mainR.data();
            io.subL = subL.data();
            io.subR = subR.data();
            io.aux1 = aux1.data();
            io.aux2 = aux2.data();
            io.phones1L = p1L.data();
            io.phones1R = p1R.data();
            io.phones2L = p2L.data();
            io.phones2R = p2R.data();
            io.clickOut = clickOut.data();
            for (int i = 0; i < 12; ++i)
                io.usb[(size_t) i] = usb[(size_t) i].data();
            midi.clear();
            desk.process (m, io, &midi);
            for (const auto meta : midi)
                midiLog.push_back (meta.getMessage().withTimeStamp ((double) (t + meta.samplePosition)));
            if (each)
                each (*this, m);
            t += m;
            n -= m;
            desk.idle();
        }
    }
    void seconds (double s, std::function<void (const Rig&, int)> each = {}) { run ((int64_t) (s * sr), each); }

    /** The peak of an output over some time. */
    float peak (const std::vector<float>& out, double s)
    {
        float p = 0.0f;
        seconds (s, [&p, &out] (const Rig&, int m) {
            for (int i = 0; i < m; ++i)
                p = std::max (p, std::abs (out[(size_t) i]));
        });
        return p;
    }
    void tap (Key k)
    {
        desk.press (k);
        desk.release (k);
    }
    void jog (int d) { desk.turnJog (d); }
    juce::String screen()
    {
        Lcd lcd;
        desk.render (lcd);
        return lcd.allText();
    }
    bool shows (const juce::String& s)
    {
        Lcd lcd;
        desk.render (lcd);
        return lcd.shows (s);
    }
    /** Turns the dial until a list's cursor is on the item labelled `label`. */
    bool select (const juce::String& label)
    {
        jog (-60);
        for (int tries = 0; tries < 60; ++tries)
        {
            Lcd lcd;
            desk.render (lcd);
            juce::ignoreUnused (lcd);
            if (cursorLabel() == label)
                return true;
            jog (1);
        }
        return false;
    }
    /** Opens MENU > path... (from the Home Screen, recorder stopped). */
    bool menu (std::initializer_list<const char*> path)
    {
        tap (Key::Menu);
        if (desk.screen() == Desk::ScreenId::Meter)
            tap (Key::Menu);
        if (desk.screen() != Desk::ScreenId::Home)
            tap (Key::Menu);
        tap (Key::Menu); // Home -> MENU
        for (auto* p : path)
        {
            for (int i = 0; i < 20; ++i)
                jog (-1);
            if (! select (p))
                return false;
            tap (Key::Jog);
        }
        return true;
    }
    juce::String cursorLabel();
    /** A sine on an input jack. */
    void sine (int input, float amp, double hz)
    {
        liveConnected[(size_t) input] = true;
        auto prev = liveSource;
        const double r = sr;
        liveSource = [prev, input, amp, hz, r] (int i, int64_t tt) {
            if (i == input)
                return amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) tt / r);
            return prev ? prev (i, tt) : 0.0f;
        };
    }
};
} // namespace trackdesk::test

inline juce::String trackdesk::test::Rig::cursorLabel() { return desk.cursorText(); }
