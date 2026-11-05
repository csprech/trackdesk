#include "Engine.h"

namespace trackdesk
{
namespace
{
    constexpr int kOutputs = 11; // MAIN L/R, SUB L/R, AUX 1, AUX 2, PHONES 1 L/R, PHONES 2 L/R, CLICK

    juce::File cardRoot()
    {
        if (auto* dir = std::getenv ("TRACKDESK_CARD_DIR")) // testing: keep the real card out of it
            return juce::File (dir);
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Trackdesk Card");
    }
} // namespace

Engine::Engine (bool isHosted) : hosted (isHosted)
{
    card = std::make_unique<CardFolder> (cardRoot());
    desk.setStorage (card.get());
    if (! hosted)
    {
        const auto f = getSettingsFile();
        if (f.existsAsFile())
            loadSettings (juce::JSON::parse (f));
        deviceManager.initialiseWithDefaultDevices (20, kOutputs + 12);
        deviceManager.addAudioCallback (this);
#if JUCE_MAC || JUCE_LINUX
        virtualOut = juce::MidiOutput::createNewDevice ("Trackdesk");
        if (virtualOut != nullptr)
            virtualOut->startBackgroundThread();
#endif
    }
    startTimerHz (30);
}

Engine::~Engine()
{
    stopTimer();
    if (! hosted)
    {
        deviceManager.removeAudioCallback (this);
        deviceManager.closeAudioDevice();
    }
    {
        const juce::ScopedLock sl (lock);
        desk.recorder.stop();
        desk.idle(); // (a pass that was recording is finished and saved)
    }
    if (! hosted)
        saveSettings();
}

juce::File Engine::getSettingsFile() const
{
    if (auto* dir = std::getenv ("TRACKDESK_SETTINGS_DIR")) // testing
        return juce::File (dir).getChildFile ("settings.json");
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Trackdesk").getChildFile ("settings.json");
}

juce::var Engine::getState()
{
    const juce::ScopedLock sl (lock);
    auto v = settingsToJson (desk.settings, desk.fx, desk.strips, desk.master);
    v.getDynamicObject()->setProperty ("lastSong", desk.currentSongName());
    v.getDynamicObject()->setProperty ("phonesFirst", phonesFirst);
    v.getDynamicObject()->setProperty ("midiOut", midiOutId);
    return v;
}

void Engine::setState (const juce::var& v)
{
    const juce::ScopedLock sl (lock);
    loadSettings (v);
    if (started && lastSong.isNotEmpty() && lastSong != desk.currentSongName())
        desk.startUp (lastSong);
}

void Engine::loadSettings (const juce::var& v)
{
    settingsFromJson (v, desk.settings, desk.fx, desk.strips, desk.master);
    lastSong = v.getProperty ("lastSong", {}).toString();
    phonesFirst = (bool) v.getProperty ("phonesFirst", false);
    desk.recorder.vampAuto = desk.settings.vampAuto;
    desk.recorder.preRollSec = desk.settings.preRollSec;
    const auto out = v.getProperty ("midiOut", {}).toString();
    if (! hosted && out.isNotEmpty() && out != midiOutId)
        juce::MessageManager::callAsync ([this, out] { setMidiOutput (out); });
}

void Engine::saveSettings()
{
    if (hosted)
        return;
    const auto f = getSettingsFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (juce::JSON::toString (getState()));
}

void Engine::setMidiOutput (const juce::String& identifier)
{
    auto out = identifier.isEmpty() ? nullptr : juce::MidiOutput::openDevice (identifier);
    if (out != nullptr)
        out->startBackgroundThread();
    {
        const juce::ScopedLock sl (lock);
        std::swap (midiOut, out);
        midiOutId = identifier;
    }
    settingsPending = true;
}

void Engine::setPhonesOnFirstOutputs (bool p)
{
    phonesFirst = p;
    settingsPending = true;
}

void Engine::setJacks (bool aux2Used, bool pcConnected)
{
    const juce::ScopedLock sl (lock);
    desk.setAux2JackUsed (aux2Used);
    desk.setUsbConnected (pcConnected);
}

void Engine::pairWithFile (const juce::File& file)
{
    auto audio = card->openAnyAudio (file);
    if (audio.empty())
    {
        if (onMessage)
            onMessage ("Can't read " + file.getFileName());
        return;
    }
    const juce::ScopedLock sl (lock);
    desk.pairWith (file.getFileNameWithoutExtension(), std::move (audio));
}

void Engine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepare (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
    const juce::ScopedLock sl (lock);
    desk.setUsbConnected (device->getActiveInputChannels().countNumberOfSetBits() > kInputs);
}

void Engine::prepareHosted (double sr, int maxBlock) { prepare (sr, maxBlock); }

void Engine::prepare (double sr, int block)
{
    const int maxBlock = std::max (512, block);
    {
        const juce::ScopedLock sl (lock);
        desk.prepare (sr, maxBlock);
        scratch.assign ((size_t) (kOutputs + 12), std::vector<float> ((size_t) maxBlock, 0.0f));
        card->setDeviceRate (sr);
    }
    midiBlock.ensureSize (4096);
    sampleRate = sr;
    prepared = true;
}

void Engine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut, int n,
                                               const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], n);
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || ! prepared || scratch.empty())
        return;
    int done = 0;
    float level = 0.0f;
    while (done < n)
    {
        const int m = std::min (n - done, (int) scratch[0].size());
        Desk::AudioIO io;
        for (int i = 0; i < kInputs; ++i)
        {
            io.live[(size_t) i] = i < numIn && in[i] != nullptr ? in[i] + done : nullptr;
            io.pc[(size_t) i] = kInputs + i < numIn && in[kInputs + i] != nullptr ? in[kInputs + i] + done : nullptr;
        }
        float* o[kOutputs + 12];
        for (int c = 0; c < kOutputs + 12; ++c)
            o[c] = scratch[(size_t) c].data();
        io.mainL = o[0];
        io.mainR = o[1];
        io.subL = o[2];
        io.subR = o[3];
        io.aux1 = o[4];
        io.aux2 = o[5];
        io.phones1L = o[6];
        io.phones1R = o[7];
        io.phones2L = o[8];
        io.phones2R = o[9];
        io.clickOut = o[10];
        for (int u = 0; u < 12; ++u)
            io.usb[(size_t) u] = o[kOutputs + u];
        midiBlock.clear();
        desk.process (m, io, &midiBlock);
        for (int c = 0; c < std::min (numOut, kOutputs + 12); ++c)
        {
            if (out[c] == nullptr)
                continue;
            int src = c;
            if (phonesFirst && c < 2)
                src = 6 + c; // PHONES 1 on the computer's own two outputs
            juce::FloatVectorOperations::copy (out[c] + done, o[src], m);
        }
        for (int i = 0; i < m; ++i)
            level = std::max (level, std::abs (o[0][i]));
        if (! midiBlock.isEmpty())
        {
            const double rate = sampleRate.load();
            for (auto* port : { midiOut.get(), virtualOut.get() })
                if (port != nullptr)
                    port->sendBlockOfMessages (midiBlock, juce::Time::getMillisecondCounterHiRes() + 1000.0 * done / rate, rate);
        }
        done += m;
    }
    outputLevel = std::max (level, outputLevel.load() * 0.9f);
}

void Engine::processHosted (const float* const* live, const float* const* pc, float* const* out, int numOut, int n, juce::MidiBuffer& midi)
{
    midi.clear();
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], n);
    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked() || ! prepared || scratch.empty())
        return;
    int done = 0;
    while (done < n)
    {
        const int m = std::min (n - done, (int) scratch[0].size());
        Desk::AudioIO io;
        for (int i = 0; i < kInputs; ++i)
            io.live[(size_t) i] = live != nullptr && live[i] != nullptr ? live[i] + done : nullptr;
        for (int i = 0; i < 2; ++i)
            io.pc[(size_t) i] = pc != nullptr && pc[i] != nullptr ? pc[i] + done : nullptr;
        float* o[kOutputs];
        for (int c = 0; c < kOutputs; ++c)
            o[c] = c < numOut && out[c] != nullptr ? out[c] + done : scratch[(size_t) c].data();
        io.mainL = o[0];
        io.mainR = o[1];
        io.subL = o[2];
        io.subR = o[3];
        io.aux1 = o[4];
        io.aux2 = o[5];
        io.phones1L = o[6];
        io.phones1R = o[7];
        io.phones2L = o[8];
        io.phones2R = o[9];
        io.clickOut = o[10];
        midiBlock.clear();
        desk.process (m, io, &midiBlock);
        midi.addEvents (midiBlock, 0, m, done);
        done += m;
    }
}

void Engine::matchDeviceRate()
{
    // a song at another rate: ask the device for it (standalone); otherwise the console says "USB Fs mismatch"
    if (hosted)
        return;
    double want = 0.0;
    {
        const juce::ScopedLock sl (lock);
        if (! desk.songLoaded() || desk.recorder.getState() != Recorder::State::Stopped)
            return;
        want = desk.recorder.info.sampleRate;
    }
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr || std::abs (device->getCurrentSampleRate() - want) < 1.0 || std::abs (lastAskedRate - want) < 1.0)
        return;
    lastAskedRate = want;
    if (! device->getAvailableSampleRates().contains (want))
        return;
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.sampleRate = want;
    deviceManager.setAudioDeviceSetup (setup, true);
}

void Engine::timerCallback()
{
    if (! prepared)
        return;
    bool pairing = false, dirty = false;
    {
        const juce::ScopedLock sl (lock);
        if (! started)
        {
            started = true;
            desk.startUp (lastSong);
        }
        desk.idle();
        pairing = desk.takePairingRequest();
        dirty = desk.takeSettingsDirty();
    }
    if (pairing && onPairingRequest)
        onPairingRequest();
    if (dirty)
        settingsPending = true;
    const auto now = juce::Time::getMillisecondCounter();
    if (settingsPending && now - lastSave > 2000)
    {
        settingsPending = false;
        lastSave = now;
        saveSettings();
    }
    matchDeviceRate();
}
} // namespace trackdesk
