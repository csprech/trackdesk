#pragma once

// The "SD card": a folder on the computer (~/Music/Trackdesk Card) with the card's folders.
//   MTR/<song>/song.json   the song: format, marks, punch and vamp points, metronome, and each track's regions
//   MTR/<song>/*.wav       its takes (mono WAV, one per track per pass, at the song's format)
//   MUSIC/                 WAV files to IMPORT and play with SD PLAY, and STEREO MIX EXPORT's files
// Takes are written by background threads while recording and read memory-mapped while playing.

#include "../core/Desk.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace trackdesk
{
class CardFolder : public Storage
{
public:
    explicit CardFolder (juce::File root);
    ~CardFolder() override;
    juce::File getRoot() const { return root; }
    juce::File songFolder (const juce::String& name) const { return root.getChildFile ("MTR").getChildFile (name); }
    juce::File musicFolder() const { return root.getChildFile ("MUSIC"); }

    std::vector<SongSummary> songs() override;
    juce::String newSongName (const Settings&) override;
    bool createSong (const SongInfo&) override;
    bool loadSong (const juce::String& name, Recorder& recorder) override;
    bool saveSong (const Recorder& recorder) override;
    bool deleteSong (const juce::String& name) override;
    bool renameSong (const juce::String& from, const juce::String& to) override;
    bool setProtected (const juce::String& name, bool protect) override;
    bool clearMarks (const juce::String& name) override;
    std::shared_ptr<TakeSink> newTake (const SongInfo& song) override;
    std::vector<MusicEntry> music (const juce::String& folder) override;
    std::vector<std::shared_ptr<TakeAudio>> openMusic (const juce::String& path) override;
    std::shared_ptr<TakeAudio> importAudio (const SongInfo& song, const juce::String& path, int channel) override;
    bool exportMixExists (const SongInfo& song) override;
    bool exportMix (const Recorder& recorder) override;
    bool format() override;
    int64_t freeBytes() override;
    int64_t totalBytes() override;
    void reveal() override;

    /** Deleted songs and formatted folders go to the computer's bin (on), or are removed for good (tests). */
    void setUseTrash (bool t) { useTrash = t; }
    /** The rate the audio device runs at (files at another rate are converted when opened for SD PLAY). */
    void setDeviceRate (double r) { deviceRate = r; }
    /** Opens any audio file as one TakeAudio per channel, in memory at the device's rate (the paired device). */
    std::vector<std::shared_ptr<TakeAudio>> openAnyAudio (const juce::File& file);

private:
    juce::File root;
    juce::AudioFormatManager formats;
    juce::TimeSliceThread diskThread { "Trackdesk disk" };
    double deviceRate = 48000.0;
    bool useTrash = true;
    juce::File mixFileFor (const SongInfo& song) const;
    std::shared_ptr<TakeAudio> openTake (const juce::File& file, int channel);
    juce::var readSongJson (const juce::String& name) const;
    bool writeSongJson (const juce::String& name, const juce::var& v) const;
};

/** Renders a track's regions into dest (later regions over earlier ones), from `start` for n samples. */
void renderTrack (const Track& track, int64_t start, float* dest, int n);
} // namespace trackdesk
