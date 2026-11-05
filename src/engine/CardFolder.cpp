#include "CardFolder.h"

namespace trackdesk
{
namespace
{
    /** A channel of a WAV, read memory-mapped (shared between the channels of one file). */
    struct MappedTake : TakeAudio
    {
        std::shared_ptr<juce::MemoryMappedAudioFormatReader> reader;
        int channel = 0;
        int64_t len = 0;
        int64_t length() const override { return len; }
        void read (int64_t start, float* dest, int n) const override
        {
            std::fill (dest, dest + n, 0.0f);
            const int64_t from = std::max<int64_t> (0, start), to = std::min<int64_t> (len, start + n);
            if (to <= from || reader == nullptr)
                return;
            float* chans[2] = { channel == 0 ? dest + (from - start) : nullptr, channel == 1 ? dest + (from - start) : nullptr };
            reader->read (chans, std::min (2, (int) reader->numChannels), from, (int) (to - from));
        }
    };

    /** Audio held in memory. */
    struct MemoryTake : TakeAudio
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

    /** A pass being recorded: a background writer per track (made before recording, so write() never allocates). */
    struct FileSink : TakeSink
    {
        struct Out
        {
            juce::File file;
            std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
            std::atomic<int64_t> written { 0 };
        };
        std::array<Out, kTracks> outs;
        std::function<std::shared_ptr<TakeAudio> (const juce::File&)> open;

        void write (int track, const float* x, int n) override
        {
            auto& o = outs[(size_t) track];
            if (o.writer == nullptr)
                return;
            const float* chans[1] = { x };
            if (o.writer->write (chans, n))
                o.written += n;
        }
        std::shared_ptr<TakeAudio> finish (int track) override
        {
            auto& o = outs[(size_t) track];
            o.writer.reset(); // (flushes the file)
            if (o.written == 0)
            {
                o.file.deleteFile();
                return {};
            }
            return open (o.file);
        }
        ~FileSink() override
        {
            for (auto& o : outs)
            {
                o.writer.reset();
                if (o.written == 0)
                    o.file.deleteFile();
            }
        }
    };

    juce::var regionsToJson (const Track& t)
    {
        juce::Array<juce::var> a;
        for (auto& r : t.regions)
        {
            if (r.audio == nullptr)
                continue;
            auto* o = new juce::DynamicObject();
            o->setProperty ("file", r.audio->file);
            o->setProperty ("start", (juce::int64) r.start);
            o->setProperty ("offset", (juce::int64) r.offset);
            o->setProperty ("length", (juce::int64) r.length);
            a.add (juce::var (o));
        }
        return a;
    }

    juce::var infoToJson (const SongInfo& s)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("format", "trackdesk-song");
        o->setProperty ("name", s.name);
        o->setProperty ("sampleRate", s.sampleRate);
        o->setProperty ("bits", s.bits);
        o->setProperty ("protect", s.protect);
        juce::Array<juce::var> marks, vamps;
        for (auto m : s.marks)
            marks.add ((juce::int64) m);
        for (auto& v : s.vamps)
            vamps.add (juce::Array<juce::var> { (juce::int64) v.first, (juce::int64) v.second });
        o->setProperty ("marks", marks);
        o->setProperty ("punchIn", (juce::int64) s.punchIn);
        o->setProperty ("punchOut", (juce::int64) s.punchOut);
        o->setProperty ("autoPunch", s.autoPunch);
        o->setProperty ("vamps", vamps);
        o->setProperty ("vampOn", s.vampOn);
        o->setProperty ("metronome", metronomeToJson (s.metronome));
        o->setProperty ("lastRecStart", (juce::int64) s.lastRecStart);
        o->setProperty ("lastRecEnd", (juce::int64) s.lastRecEnd);
        return juce::var (o);
    }

    void infoFromJson (const juce::var& v, SongInfo& s)
    {
        s.name = v.getProperty ("name", s.name).toString();
        s.sampleRate = (double) v.getProperty ("sampleRate", 48000.0) > 46000.0 ? 48000.0 : 44100.0;
        s.bits = (int) v.getProperty ("bits", 24) == 16 ? 16 : 24;
        s.protect = (bool) v.getProperty ("protect", false);
        s.marks.clear();
        if (auto* m = v["marks"].getArray())
            for (auto& x : *m)
                s.marks.push_back ((int64_t) (juce::int64) x);
        std::sort (s.marks.begin(), s.marks.end());
        s.punchIn = (int64_t) (juce::int64) v.getProperty ("punchIn", -1);
        s.punchOut = (int64_t) (juce::int64) v.getProperty ("punchOut", -1);
        s.autoPunch = (bool) v.getProperty ("autoPunch", false);
        s.vamps.clear();
        if (auto* a = v["vamps"].getArray())
            for (auto& x : *a)
                if (auto* p = x.getArray(); p != nullptr && p->size() == 2)
                    s.vamps.push_back ({ (int64_t) (juce::int64) (*p)[0], (int64_t) (juce::int64) (*p)[1] });
        s.vampOn = (bool) v.getProperty ("vampOn", false);
        metronomeFromJson (v["metronome"], s.metronome);
        s.lastRecStart = (int64_t) (juce::int64) v.getProperty ("lastRecStart", 0);
        s.lastRecEnd = (int64_t) (juce::int64) v.getProperty ("lastRecEnd", 0);
    }
} // namespace

void renderTrack (const Track& track, int64_t start, float* dest, int n)
{
    std::fill (dest, dest + n, 0.0f);
    const int64_t a = start, b = start + n;
    for (auto& r : track.regions)
    {
        const int64_t from = std::max (a, r.start), to = std::min (b, r.end());
        if (to <= from || r.audio == nullptr)
            continue;
        r.audio->read (r.offset + (from - r.start), dest + (from - a), (int) (to - from));
    }
}

CardFolder::CardFolder (juce::File r) : root (std::move (r))
{
    formats.registerBasicFormats();
    root.getChildFile ("MTR").createDirectory();
    root.getChildFile ("MUSIC").createDirectory();
    diskThread.startThread (juce::Thread::Priority::high);
}

CardFolder::~CardFolder() { diskThread.stopThread (4000); }

juce::var CardFolder::readSongJson (const juce::String& name) const
{
    const auto f = songFolder (name).getChildFile ("song.json");
    return f.existsAsFile() ? juce::JSON::parse (f) : juce::var();
}

bool CardFolder::writeSongJson (const juce::String& name, const juce::var& v) const
{
    const auto f = songFolder (name).getChildFile ("song.json");
    juce::TemporaryFile tmp (f);
    if (! tmp.getFile().replaceWithText (juce::JSON::toString (v)))
        return false;
    return tmp.overwriteTargetFileWithTemporary();
}

std::vector<SongSummary> CardFolder::songs()
{
    std::vector<SongSummary> v;
    for (auto& dir : root.getChildFile ("MTR").findChildFiles (juce::File::findDirectories, false))
    {
        const auto json = dir.getChildFile ("song.json");
        if (! json.existsAsFile())
            continue;
        const auto j = juce::JSON::parse (json);
        SongSummary s;
        s.name = dir.getFileName();
        s.sampleRate = (double) j.getProperty ("sampleRate", 48000.0);
        s.bits = (int) j.getProperty ("bits", 24);
        s.protect = (bool) j.getProperty ("protect", false);
        s.written = json.getLastModificationTime();
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav"))
            s.bytes += f.getSize();
        v.push_back (s);
    }
    std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.name < b.name; });
    return v;
}

juce::String CardFolder::newSongName (const Settings& st)
{
    // DATE: 261104_0001; WORD: SONG_0001 (p.60)
    const juce::String prefix = st.nameByDate ? juce::Time::getCurrentTime().formatted ("%y%m%d") : st.nameWord.substring (0, 6);
    for (int n = 1; n < 10000; ++n)
    {
        const auto name = prefix + "_" + juce::String (n).paddedLeft ('0', 4);
        if (! songFolder (name).exists())
            return name;
    }
    return prefix + "_" + juce::String (juce::Random::getSystemRandom().nextInt (100000));
}

bool CardFolder::createSong (const SongInfo& info)
{
    if (songs().size() >= 100 || info.name.isEmpty())
        return false;
    const auto dir = songFolder (info.name);
    if (dir.exists() || ! dir.createDirectory())
        return false;
    auto j = infoToJson (info);
    j.getDynamicObject()->setProperty ("tracks", juce::Array<juce::var>());
    return writeSongJson (info.name, j);
}

std::shared_ptr<TakeAudio> CardFolder::openTake (const juce::File& file, int channel)
{
    juce::WavAudioFormat wav;
    std::shared_ptr<juce::MemoryMappedAudioFormatReader> reader (wav.createMemoryMappedReader (file));
    if (reader == nullptr || ! reader->mapEntireFile())
        return {};
    auto t = std::make_shared<MappedTake>();
    t->reader = reader;
    t->channel = juce::jlimit (0, (int) reader->numChannels - 1, channel);
    t->len = reader->lengthInSamples;
    t->file = file.getFileName();
    return t;
}

bool CardFolder::loadSong (const juce::String& name, Recorder& recorder)
{
    const auto j = readSongJson (name);
    if (! j.isObject())
        return false;
    recorder.clearSong();
    infoFromJson (j, recorder.info);
    recorder.info.name = name;
    std::map<juce::String, std::shared_ptr<TakeAudio>> open;
    if (auto* tracks = j["tracks"].getArray())
        for (int t = 0; t < std::min (kTracks, tracks->size()); ++t)
            if (auto* regions = (*tracks)[t].getArray())
                for (auto& r : *regions)
                {
                    const auto file = r.getProperty ("file", {}).toString();
                    if (! open.count (file))
                        open[file] = openTake (songFolder (name).getChildFile (file), 0);
                    auto audio = open[file];
                    if (audio == nullptr)
                        continue; // (a missing take: that region is silent)
                    Region reg;
                    reg.audio = audio;
                    reg.start = (int64_t) (juce::int64) r.getProperty ("start", 0);
                    reg.offset = (int64_t) (juce::int64) r.getProperty ("offset", 0);
                    reg.length = std::min ((int64_t) (juce::int64) r.getProperty ("length", 0), audio->length() - reg.offset);
                    recorder.tracks[(size_t) t].regions.push_back (reg);
                }
    return true;
}

bool CardFolder::saveSong (const Recorder& recorder)
{
    const auto& info = recorder.info;
    if (! songFolder (info.name).isDirectory())
        return false;
    auto j = infoToJson (info);
    juce::Array<juce::var> tracks;
    for (auto& t : recorder.tracks)
        tracks.add (regionsToJson (t));
    j.getDynamicObject()->setProperty ("tracks", tracks);
    return writeSongJson (info.name, j);
}

bool CardFolder::deleteSong (const juce::String& name)
{
    const auto dir = songFolder (name);
    return dir.isDirectory() && ((useTrash && dir.moveToTrash()) || dir.deleteRecursively());
}

bool CardFolder::renameSong (const juce::String& from, const juce::String& to)
{
    const auto a = songFolder (from), b = songFolder (to);
    if (! a.isDirectory() || b.exists() || ! juce::File::createLegalFileName (to).equalsIgnoreCase (to))
        return false;
    if (! a.moveFileTo (b))
        return false;
    auto j = readSongJson (to);
    if (j.isObject())
    {
        j.getDynamicObject()->setProperty ("name", to);
        writeSongJson (to, j);
    }
    return true;
}

bool CardFolder::setProtected (const juce::String& name, bool p)
{
    auto j = readSongJson (name);
    if (! j.isObject())
        return false;
    j.getDynamicObject()->setProperty ("protect", p);
    return writeSongJson (name, j);
}

bool CardFolder::clearMarks (const juce::String& name)
{
    auto j = readSongJson (name);
    if (! j.isObject())
        return false;
    j.getDynamicObject()->setProperty ("marks", juce::Array<juce::var>());
    return writeSongJson (name, j);
}

std::shared_ptr<TakeSink> CardFolder::newTake (const SongInfo& song)
{
    const auto dir = songFolder (song.name);
    if (! dir.isDirectory())
        return {};
    auto sink = std::make_shared<FileSink>();
    sink->open = [this] (const juce::File& f) { return openTake (f, 0); };
    const auto stamp = juce::Time::getCurrentTime().formatted ("%H%M%S");
    juce::WavAudioFormat wav;
    for (int t = 0; t < kTracks; ++t)
    {
        auto& o = sink->outs[(size_t) t];
        o.file = dir.getNonexistentChildFile ("TR" + juce::String (t + 1).paddedLeft ('0', 2) + "_" + stamp, ".wav", false);
        std::unique_ptr<juce::OutputStream> os (o.file.createOutputStream().release());
        if (os == nullptr)
            return {};
        const auto opts = juce::AudioFormatWriterOptions().withSampleRate (song.sampleRate).withNumChannels (1).withBitsPerSample (song.bits);
        auto writer = wav.createWriterFor (os, opts);
        if (writer == nullptr)
            return {};
        o.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer.release(), diskThread, 1 << 18);
    }
    return sink;
}

std::vector<MusicEntry> CardFolder::music (const juce::String& folder)
{
    std::vector<MusicEntry> v;
    const auto dir = folder.isEmpty() ? musicFolder() : musicFolder().getChildFile (folder);
    if (! dir.isDirectory())
        return v;
    // folders (two levels below MUSIC: p.64), then WAV files
    if (! folder.containsChar ('/'))
        for (auto& d : dir.findChildFiles (juce::File::findDirectories, false))
            v.push_back ({ d.getFileName(), d.getRelativePathFrom (musicFolder()).replaceCharacter ('\\', '/'), true });
    std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.name < b.name; });
    std::vector<MusicEntry> files;
    juce::WavAudioFormat wav;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.wav;*.WAV"))
    {
        std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (f.createInputStream().release(), true));
        if (r == nullptr)
            continue;
        files.push_back ({ f.getFileName(), f.getRelativePathFrom (musicFolder()).replaceCharacter ('\\', '/'), false, r->sampleRate,
                           (int) r->bitsPerSample, (int) r->numChannels, (int64_t) r->lengthInSamples });
    }
    std::sort (files.begin(), files.end(), [] (auto& a, auto& b) { return a.name < b.name; });
    v.insert (v.end(), files.begin(), files.end());
    return v;
}

std::vector<std::shared_ptr<TakeAudio>> CardFolder::openMusic (const juce::String& path)
{
    const auto file = musicFolder().getChildFile (path);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> probe (wav.createReaderFor (file.createInputStream().release(), true));
    if (probe == nullptr)
        return {};
    if (std::abs (probe->sampleRate - deviceRate) > 1.0)
        return openAnyAudio (file); // (converted to the device's rate)
    std::vector<std::shared_ptr<TakeAudio>> v;
    for (int ch = 0; ch < std::min (2, (int) probe->numChannels); ++ch)
        if (auto t = openTake (file, ch))
            v.push_back (t);
    return v;
}

std::vector<std::shared_ptr<TakeAudio>> CardFolder::openAnyAudio (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (file));
    if (r == nullptr || r->lengthInSamples <= 0 || r->lengthInSamples > (int64_t) r->sampleRate * 60 * 30)
        return {};
    const int chans = std::min (2, (int) r->numChannels);
    juce::AudioBuffer<float> buf (chans, (int) r->lengthInSamples);
    r->read (&buf, 0, (int) r->lengthInSamples, 0, true, chans > 1);
    std::vector<std::shared_ptr<TakeAudio>> v;
    const double ratio = r->sampleRate / deviceRate;
    for (int ch = 0; ch < chans; ++ch)
    {
        auto t = std::make_shared<MemoryTake>();
        if (std::abs (ratio - 1.0) < 1.0e-6)
            t->data.assign (buf.getReadPointer (ch), buf.getReadPointer (ch) + buf.getNumSamples());
        else
        {
            const int outLen = (int) ((double) buf.getNumSamples() / ratio);
            t->data.assign ((size_t) outLen, 0.0f);
            juce::LagrangeInterpolator interp;
            interp.process (ratio, buf.getReadPointer (ch), t->data.data(), outLen, buf.getNumSamples(), 0);
        }
        t->file = file.getFileName();
        v.push_back (t);
    }
    return v;
}

std::shared_ptr<TakeAudio> CardFolder::importAudio (const SongInfo& song, const juce::String& path, int channel)
{
    const auto src = musicFolder().getChildFile (path);
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (src.createInputStream().release(), true));
    if (r == nullptr)
        return {};
    const auto dest = songFolder (song.name).getNonexistentChildFile ("IMPORT_" + src.getFileNameWithoutExtension().substring (0, 16), ".wav", false);
    {
        std::unique_ptr<juce::OutputStream> os (dest.createOutputStream().release());
        if (os == nullptr)
            return {};
        const auto opts = juce::AudioFormatWriterOptions().withSampleRate (song.sampleRate).withNumChannels (1).withBitsPerSample (song.bits);
        auto w = wav.createWriterFor (os, opts);
        if (w == nullptr)
            return {};
        const int block = 32768;
        juce::AudioBuffer<float> buf ((int) r->numChannels, block);
        juce::AudioBuffer<float> mono (1, block);
        for (int64_t pos = 0; pos < r->lengthInSamples; pos += block)
        {
            const int n = (int) std::min<int64_t> (block, r->lengthInSamples - pos);
            r->read (&buf, 0, n, pos, true, true);
            if (channel < 0 && buf.getNumChannels() > 1) // a stereo file into one track: both sides
            {
                mono.copyFrom (0, 0, buf, 0, 0, n);
                mono.addFrom (0, 0, buf, 1, 0, n);
                mono.applyGain (0, 0, n, 0.5f);
            }
            else
                mono.copyFrom (0, 0, buf, juce::jlimit (0, buf.getNumChannels() - 1, std::max (0, channel)), 0, n);
            w->writeFromAudioSampleBuffer (mono, 0, n);
        }
    }
    return openTake (dest, 0);
}

juce::File CardFolder::mixFileFor (const SongInfo& song) const { return musicFolder().getChildFile (song.name + "_2-MIX_01.wav"); }

bool CardFolder::exportMixExists (const SongInfo& song) { return mixFileFor (song).existsAsFile(); }

bool CardFolder::exportMix (const Recorder& recorder)
{
    // tracks 11/12 to one stereo file in MUSIC (p.48)
    const auto& song = recorder.info;
    const auto dest = mixFileFor (song);
    dest.deleteFile();
    const int64_t len = std::max (recorder.tracks[(size_t) kMixTrack].end(), recorder.tracks[(size_t) kMixTrack + 1].end());
    if (len <= 0)
        return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (dest.createOutputStream().release());
    if (os == nullptr)
        return false;
    const auto opts = juce::AudioFormatWriterOptions().withSampleRate (song.sampleRate).withNumChannels (2).withBitsPerSample (song.bits);
    auto w = wav.createWriterFor (os, opts);
    if (w == nullptr)
        return false;
    const int block = 32768;
    juce::AudioBuffer<float> buf (2, block);
    for (int64_t pos = 0; pos < len; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, len - pos);
        renderTrack (recorder.tracks[(size_t) kMixTrack], pos, buf.getWritePointer (0), n);
        renderTrack (recorder.tracks[(size_t) kMixTrack + 1], pos, buf.getWritePointer (1), n);
        if (! w->writeFromAudioSampleBuffer (buf, 0, n))
            return false;
    }
    return true;
}

bool CardFolder::format()
{
    for (auto* name : { "MTR", "MUSIC" })
    {
        auto dir = root.getChildFile (name);
        if (dir.exists() && ! (useTrash && dir.moveToTrash()))
            dir.deleteRecursively();
        dir.createDirectory();
    }
    return true;
}

int64_t CardFolder::freeBytes() { return root.getBytesFreeOnVolume(); }

int64_t CardFolder::totalBytes() { return root.getVolumeTotalSize(); }

void CardFolder::reveal() { root.getChildFile ("MTR").revealToUser(); }
} // namespace trackdesk
