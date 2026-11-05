#pragma once

// Audio, MIDI and files around the console.
//  * The Desk lives under one lock: the message thread holds it for each control, menu step and file job; the
//    audio thread tries it each block (when a file job holds it, that block is silent, as on the original).
//  * Standalone, the device's inputs 1-10 are the input jacks and 11-20 the computer (USB); its outputs are
//    MAIN L/R, SUB L/R, AUX 1, AUX 2, PHONES 1 L/R, PHONES 2 L/R, CLICK, then the USB sends (1-12).
//  * MIDI clock and time code go to a virtual MIDI port ("Trackdesk") and the chosen MIDI output.
//  * The settings (and the panel's knobs) are kept in a file; the songs are in the card folder.

#include "CardFolder.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>

namespace trackdesk
{
class Engine : public juce::AudioIODeviceCallback, private juce::Timer
{
public:
    /** Standalone, the engine opens the audio device and MIDI itself and keeps its settings in a file.
        Hosted (in a studio or a plug-in host), the host calls prepareHosted() and processHosted(), and the
        settings travel with the host's session (getState / setState). */
    explicit Engine (bool hosted = false);
    ~Engine() override;
    bool isHosted() const { return hosted; }

    void prepareHosted (double sampleRate, int maxBlockSize);
    /** Hosted: one block. live[0..9] the input jacks (null when unplugged), pc[0..1] the computer (null when not
        connected); out[0..10]: MAIN L, R, SUB L, R, AUX 1, AUX 2, PHONES 1 L, R, PHONES 2 L, R, CLICK (any may be
        null). `midi` returns the clock and time code (the MIDI OUT jack). */
    void processHosted (const float* const* live, const float* const* pc, float* const* out, int numOut, int numSamples, juce::MidiBuffer& midi);
    /** Hosted: which jacks are in use (the AUX 2 jack turns the effect off; the computer lights USB). */
    void setJacks (bool aux2Used, bool pcConnected);

    template <typename Fn>
    auto withDesk (Fn&& fn)
    {
        const juce::ScopedLock sl (lock);
        return fn (desk);
    }

    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::Array<juce::MidiDeviceInfo> getMidiOutputs() const { return juce::MidiOutput::getAvailableDevices(); }
    void setMidiOutput (const juce::String& identifier);
    juce::String getMidiOutputIdentifier() const { return midiOutId; }
    /** Outputs 1-2: MAIN OUT (the normal order), or PHONES 1 for a computer's own two outputs. */
    void setPhonesOnFirstOutputs (bool p);
    bool getPhonesOnFirstOutputs() const { return phonesFirst; }
    std::function<void (const juce::String&)> onMessage;
    /** PAIRING: the panel asks for an audio file to "pair" with and hands it back here. */
    std::function<void()> onPairingRequest;
    void pairWithFile (const juce::File& file);

    juce::var getState();
    void setState (const juce::var& state);
    CardFolder& getCard() { return *card; }
    juce::File getSettingsFile() const;
    float getOutputLevel() const { return outputLevel.load(); }

private:
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice*) override;
    void audioDeviceStopped() override {}
    void timerCallback() override;
    void prepare (double sampleRate, int block);
    void saveSettings();
    void loadSettings (const juce::var& v);
    void matchDeviceRate();

    const bool hosted;
    juce::CriticalSection lock;
    std::unique_ptr<CardFolder> card;
    Desk desk;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::MidiOutput> midiOut, virtualOut;
    juce::String midiOutId;
    juce::MidiBuffer midiBlock;
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<bool> prepared { false };
    bool started = false, phonesFirst = false;
    juce::String lastSong;
    std::vector<std::vector<float>> scratch;
    juce::uint32 lastSave = 0;
    bool settingsPending = false;
    double lastAskedRate = 0.0;
};
} // namespace trackdesk
