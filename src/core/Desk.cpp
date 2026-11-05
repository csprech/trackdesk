#include "Desk.h"

namespace trackdesk
{
namespace
{
    constexpr int kScratchTracks = 0;              // playback, one per track
    constexpr int kScratchRec = kTracks;           // record taps, one per track
    constexpr int kScratchClick = kTracks * 2;     // the metronome
    constexpr int kScratchBt = kScratchClick + 1;  // the paired device L, R
    constexpr int kScratchSink = kScratchBt + 2;   // outputs nobody listens to
    constexpr int kScratchCount = kScratchSink + 1;
    constexpr double kHoldSeconds = 0.45;          // REW / FF: a press, or held to search

    bool rolling (Recorder::State s) { return s == Recorder::State::Playing || s == Recorder::State::Recording; }
} // namespace

Desk::Desk()
{
    stack.push_back ({});
    openMeter(); // the meter screen opens after start-up (p.33)
}

Desk::~Desk() = default;

void Desk::prepare (double sr, int block)
{
    sampleRate = sr;
    maxBlock = std::max (16, block);
    mixer.prepare (sr, maxBlock);
    recorder.prepare (sr);
    clickVoice.prepare (sr);
    scratch.assign ((size_t) kScratchCount, std::vector<float> ((size_t) maxBlock, 0.0f));
}

// ======================================================================== the panel's controls

void Desk::setKnob (int s, StripKnob k, float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    auto& c = strips[(size_t) s];
    switch (k)
    {
        case StripKnob::Gain: c.gain = v; break;
        case StripKnob::Comp: c.comp = v; break;
        case StripKnob::EqHigh: c.eqHigh = v; break;
        case StripKnob::EqMidFreq: c.eqMidFreq = v; break;
        case StripKnob::EqMid: c.eqMid = v; break;
        case StripKnob::EqLow: c.eqLow = v; break;
        case StripKnob::Aux1: c.aux1 = v; break;
        case StripKnob::Aux2: c.aux2 = v; break;
        case StripKnob::Pan: c.pan = v; break;
        case StripKnob::Fader: c.fader = v; break;
    }
    settingsChanged();
}

float Desk::getKnob (int s, StripKnob k) const
{
    const auto& c = strips[(size_t) s];
    switch (k)
    {
        case StripKnob::Gain: return c.gain;
        case StripKnob::Comp: return c.comp;
        case StripKnob::EqHigh: return c.eqHigh;
        case StripKnob::EqMidFreq: return c.eqMidFreq;
        case StripKnob::EqMid: return c.eqMid;
        case StripKnob::EqLow: return c.eqLow;
        case StripKnob::Aux1: return c.aux1;
        case StripKnob::Aux2: return c.aux2;
        case StripKnob::Pan: return c.pan;
        case StripKnob::Fader: return c.fader;
    }
    return 0.0f;
}

void Desk::setSwitch (int s, StripSwitch w, bool on)
{
    auto& c = strips[(size_t) s];
    switch (w)
    {
        case StripSwitch::LowCut: c.lowCut = on; break;
        case StripSwitch::Inst: c.inst = on; break;
        case StripSwitch::Mute: c.mute = on; break;
        case StripSwitch::Main: c.main = on; break;
        case StripSwitch::Sub: c.sub = on; break;
        case StripSwitch::Solo: c.solo = on; break;
        case StripSwitch::Rec:
            // the REC keys can't change while recording (a pass is open)
            if (recorder.isRecording() || countInLeft > 0)
                return;
            recKey[(size_t) s] = on;
            return;
    }
    settingsChanged();
}

bool Desk::getSwitch (int s, StripSwitch w) const
{
    const auto& c = strips[(size_t) s];
    switch (w)
    {
        case StripSwitch::LowCut: return c.lowCut;
        case StripSwitch::Inst: return c.inst;
        case StripSwitch::Mute: return c.mute;
        case StripSwitch::Main: return c.main;
        case StripSwitch::Sub: return c.sub;
        case StripSwitch::Solo: return c.solo;
        case StripSwitch::Rec: return recKey[(size_t) s];
    }
    return false;
}

void Desk::setMode (int s, Mode m)
{
    strips[(size_t) s].mode = m;
    settingsChanged();
}

void Desk::setMasterKnob (MasterKnob k, float v)
{
    v = juce::jlimit (0.0f, 1.0f, v);
    auto& m = master;
    switch (k)
    {
        case MasterKnob::EqHigh: m.eqHigh = v; break;
        case MasterKnob::EqMidFreq: m.eqMidFreq = v; break;
        case MasterKnob::EqMid: m.eqMid = v; break;
        case MasterKnob::EqLow: m.eqLow = v; break;
        case MasterKnob::Aux1Out: m.aux1Out = v; break;
        case MasterKnob::Aux2Out: m.aux2Out = v; break;
        case MasterKnob::PflAfl: m.pflAfl = v; break;
        case MasterKnob::Phones1: m.phones1 = v; break;
        case MasterKnob::Phones2: m.phones2 = v; break;
        case MasterKnob::FxToAux1: m.fxToAux1 = v; break;
        case MasterKnob::FxFader: m.fxFader = v; break;
        case MasterKnob::SubFader: m.subFader = v; break;
        case MasterKnob::MainFader: m.mainFader = v; break;
    }
    settingsChanged();
}

float Desk::getMasterKnob (MasterKnob k) const
{
    const auto& m = master;
    switch (k)
    {
        case MasterKnob::EqHigh: return m.eqHigh;
        case MasterKnob::EqMidFreq: return m.eqMidFreq;
        case MasterKnob::EqMid: return m.eqMid;
        case MasterKnob::EqLow: return m.eqLow;
        case MasterKnob::Aux1Out: return m.aux1Out;
        case MasterKnob::Aux2Out: return m.aux2Out;
        case MasterKnob::PflAfl: return m.pflAfl;
        case MasterKnob::Phones1: return m.phones1;
        case MasterKnob::Phones2: return m.phones2;
        case MasterKnob::FxToAux1: return m.fxToAux1;
        case MasterKnob::FxFader: return m.fxFader;
        case MasterKnob::SubFader: return m.subFader;
        case MasterKnob::MainFader: return m.mainFader;
    }
    return 0.0f;
}

void Desk::setMasterSwitch (MasterSwitch w, bool on)
{
    auto& m = master;
    switch (w)
    {
        case MasterSwitch::EqNarrow: m.eqNarrow = on; break;
        case MasterSwitch::EqOnAux: m.eqOnAux = on; break;
        case MasterSwitch::EqIn: m.eqIn = on; break;
        case MasterSwitch::Phantom:
            m.phantom = on;
            if (on)
            {
                // phantom power comes on after a confirmation on the display (p.39)
                popup = {};
                popup.active = true;
                popup.kind = Popup::Phantom;
                popup.lines = { "Phantom power ON", "Are you sure?" };
                popup.onConfirm = [this] { phantomOn = master.phantom; };
            }
            else
            {
                phantomOn = false;
                if (popup.active && popup.kind == Popup::Phantom)
                    popup = {};
            }
            return;
        case MasterSwitch::Aux1Mute: m.aux1Mute = on; break;
        case MasterSwitch::Aux1Afl: m.aux1Afl = on; break;
        case MasterSwitch::Aux2Mute: m.aux2Mute = on; break;
        case MasterSwitch::Aux2Afl: m.aux2Afl = on; break;
        case MasterSwitch::SdReturn: m.sdReturn = on; break;
        case MasterSwitch::Phones1Aux: m.phones1Aux = on; break;
        case MasterSwitch::Phones2Aux: m.phones2Aux = on; break;
        case MasterSwitch::FxMute: m.fxMute = on; break;
        case MasterSwitch::FxAux1: m.fxAux1 = on; break;
        case MasterSwitch::FxMain: m.fxMain = on; break;
        case MasterSwitch::FxSub: m.fxSub = on; break;
        case MasterSwitch::FxAfl: m.fxAfl = on; break;
        case MasterSwitch::SubMute: m.subMute = on; break;
        case MasterSwitch::SubMain: m.subMain = on; break;
        case MasterSwitch::MainMute: m.mainMute = on; break;
        case MasterSwitch::PostRec: m.postRec = on; break;
    }
    settingsChanged();
}

bool Desk::getMasterSwitch (MasterSwitch w) const
{
    const auto& m = master;
    switch (w)
    {
        case MasterSwitch::EqNarrow: return m.eqNarrow;
        case MasterSwitch::EqOnAux: return m.eqOnAux;
        case MasterSwitch::EqIn: return m.eqIn;
        case MasterSwitch::Phantom: return m.phantom;
        case MasterSwitch::Aux1Mute: return m.aux1Mute;
        case MasterSwitch::Aux1Afl: return m.aux1Afl;
        case MasterSwitch::Aux2Mute: return m.aux2Mute;
        case MasterSwitch::Aux2Afl: return m.aux2Afl;
        case MasterSwitch::SdReturn: return m.sdReturn;
        case MasterSwitch::Phones1Aux: return m.phones1Aux;
        case MasterSwitch::Phones2Aux: return m.phones2Aux;
        case MasterSwitch::FxMute: return m.fxMute;
        case MasterSwitch::FxAux1: return m.fxAux1;
        case MasterSwitch::FxMain: return m.fxMain;
        case MasterSwitch::FxSub: return m.fxSub;
        case MasterSwitch::FxAfl: return m.fxAfl;
        case MasterSwitch::SubMute: return m.subMute;
        case MasterSwitch::SubMain: return m.subMain;
        case MasterSwitch::MainMute: return m.mainMute;
        case MasterSwitch::PostRec: return m.postRec;
    }
    return false;
}

void Desk::setAssign (Assign a)
{
    master.assign = a;
    settingsChanged();
}

// ======================================================================== keys

void Desk::press (Key k)
{
    if (down[(size_t) k])
        return;
    down[(size_t) k] = true;
    downAt[(size_t) k] = clock;
    keyDown (k);
}

void Desk::release (Key k)
{
    if (! down[(size_t) k])
        return;
    down[(size_t) k] = false;
    keyUp (k);
}

void Desk::keyDown (Key k)
{
    // the footswitch: on press (normally open) or release (normally closed)
    if (k == Key::Foot1 || k == Key::Foot2)
    {
        const int i = k == Key::Foot1 ? 0 : 1;
        if (! settings.footNormallyClosed[(size_t) i])
            footswitch (i);
        return;
    }
    if (popup.active)
    {
        if (popup.kind == Popup::Message)
        {
            popup = {};
            if (k == Key::F1 || k == Key::Jog || k == Key::F4 || k == Key::Menu)
                return; // (the key just closes the message)
        }
        else
        {
            if (k == Key::F1)
            {
                if (popup.kind != Popup::Phantom)
                    popup = {};
                return;
            }
            if (k == Key::Jog || (k == Key::F4 && popup.kind == Popup::Confirm))
            {
                auto fn = popup.onConfirm;
                popup = {};
                if (fn)
                    fn();
                return;
            }
            if (popup.kind == Popup::Phantom || popup.kind == Popup::Confirm)
                return; // (a confirmation waits for its answer)
        }
    }
    switch (k)
    {
        case Key::Rew:
        case Key::Ff:
            if (down[(size_t) Key::Stop] && ! sdMode)
            {
                // STOP + REW / FF: where recording last started / stopped (p.23)
                if (! recorder.isRecording())
                {
                    recorder.locate (k == Key::Rew ? recorder.info.lastRecStart : recorder.info.lastRecEnd);
                    needSpp = needFullFrame = true;
                }
                downAt[(size_t) k] = std::numeric_limits<int64_t>::max() / 2; // (no search)
            }
            return; // (a press locates on release; held, it searches)
        case Key::Stop:
        case Key::Play:
        case Key::Record:
            transportKey (k);
            return;
        case Key::Click:
            clickKey();
            return;
        case Key::Pairing:
            if (pairing == Pairing::Off)
            {
                pairing = Pairing::Waiting;
                pairingSince = clock;
            }
            else if (pairing == Pairing::Waiting)
                pairingRequest = true;
            return;
        case Key::FxSelect:
            openEffect();
            return;
        default:
            screenKey (k);
            return;
    }
}

void Desk::keyUp (Key k)
{
    if (k == Key::Foot1 || k == Key::Foot2)
    {
        const int i = k == Key::Foot1 ? 0 : 1;
        if (settings.footNormallyClosed[(size_t) i])
            footswitch (i);
        return;
    }
    if (k == Key::Rew || k == Key::Ff)
    {
        const bool wasSearching = searching;
        searching = false;
        searchDir = 0;
        const int64_t held = clock - downAt[(size_t) k];
        if (! wasSearching && held >= 0 && held < (int64_t) (kHoldSeconds * sampleRate))
            locateKey (k == Key::Rew ? -1 : 1);
    }
}

void Desk::turnJog (int d)
{
    if (d == 0 || popup.active)
        return;
    screenJog (d);
}

bool Desk::stopped() const
{
    const auto s = recorder.getState();
    return (s == Recorder::State::Stopped || s == Recorder::State::Paused) && countInLeft == 0;
}

std::array<bool, kTracks> Desk::armedTracks() const
{
    std::array<bool, kTracks> a {};
    for (int s = 0; s < kStrips; ++s)
        if (recKey[(size_t) s])
        {
            a[(size_t) firstInputOf (s)] = true;
            if (isStereo (s))
                a[(size_t) firstInputOf (s) + 1] = true;
        }
    return a;
}

int Desk::armedTrackCount() const
{
    int n = 0;
    for (auto a : armedTracks())
        n += a ? 1 : 0;
    return n;
}

void Desk::transportKey (Key k)
{
    if (sdMode)
    {
        // SD PLAY: the transport plays the MUSIC folder's files
        if (k == Key::Play)
        {
            if (sdIndex < 0)
                return;
            if (! sdRolling)
                sdRolling = true, sdPaused = false;
            else
                sdPaused = ! sdPaused;
        }
        else if (k == Key::Stop)
        {
            if (! sdRolling || sdPaused)
                sdPos = 0;
            sdRolling = false;
            sdPaused = false;
        }
        else if (k == Key::Record)
            message ("SD PLAY:", "cannot record");
        return;
    }
    if (k == Key::Play)
        playKey();
    else if (k == Key::Stop)
        stopKey();
    else
        recordKey();
}

void Desk::playKey()
{
    if (countInLeft > 0)
        return;
    if (! loaded)
    {
        message ("Song is not", "loaded");
        return;
    }
    if (std::abs (recorder.info.sampleRate - sampleRate) > 1.0)
    {
        message ("USB Fs mismatch");
        return;
    }
    switch (recorder.getState())
    {
        case Recorder::State::Stopped:
            if (recorder.info.autoPunch && recorder.info.punchIn < 0 && recorder.info.punchOut < 0)
            {
                message ("Need to set", "I/O point.");
                return;
            }
            recorder.preRollSec = settings.preRollSec;
            recorder.play();
            break;
        case Recorder::State::Paused: recorder.play(); break;
        case Recorder::State::Playing: recorder.pause(); break;
        case Recorder::State::Recording: break;
    }
}

void Desk::stopKey()
{
    if (countInLeft > 0)
    {
        countInLeft = 0;
        pendingSink.reset();
        return;
    }
    if (searching)
        return;
    recorder.stop();
    needSpp = needFullFrame = true;
}

bool Desk::canRecord (bool punchIn)
{
    if (! loaded)
    {
        message ("Song is not", "loaded");
        return false;
    }
    if (recorder.info.protect)
    {
        message ("Song Protected");
        return false;
    }
    if (vampScreenOpen() || recorder.isVamping())
    {
        message ("VAMP MODE:", "cannot record");
        return false;
    }
    if (std::abs (recorder.info.sampleRate - sampleRate) > 1.0)
    {
        message ("USB Fs mismatch");
        return false;
    }
    if ((punchIn || recorder.info.autoPunch) && armedTrackCount() > 8)
    {
        message ("8 track", "punch in limit");
        return false;
    }
    if (recorder.info.autoPunch && recorder.info.punchIn < 0 && recorder.info.punchOut < 0)
    {
        message ("Need to set", "I/O point.");
        return false;
    }
    if (storage != nullptr && storage->freeBytes() < (int64_t) (sampleRate * 3 * 12 * 2))
    {
        message ("Card Full");
        return false;
    }
    return true;
}

void Desk::recordKey()
{
    if (recorder.isRecording() || countInLeft > 0)
        return;
    const bool fromStop = recorder.getState() == Recorder::State::Stopped;
    if (! canRecord (! fromStop) || storage == nullptr)
        return;
    auto sink = storage->newTake (recorder.info);
    if (sink == nullptr)
    {
        message ("Write error", "REC stop");
        return;
    }
    recorder.preRollSec = settings.preRollSec;
    const auto& mt = recorder.info.metronome;
    if (fromStop && mt.clickOut == ClickOut::CountInRec && ! recorder.info.autoPunch)
    {
        // a count-in first, then recording starts (p.52)
        const double beat = 60.0 / mt.tempo * sampleRate;
        countInBeatLength = (int64_t) beat;
        switch (mt.countIn)
        {
            case CountIn::Sig1: countInBeats = mt.sigNum; break;
            case CountIn::Sig2: countInBeats = mt.sigNum * 2; break;
            case CountIn::Sig4: countInBeats = mt.sigNum * 4; break;
            case CountIn::Beat2p4: countInBeats = 6; break; // two halves then four quarters
            case CountIn::Beat2: countInBeats = 2; break;
            case CountIn::Beat3: countInBeats = 3; break;
            case CountIn::Beat4: countInBeats = 4; break;
        }
        countInAccentFirst = mt.countIn == CountIn::Sig1 || mt.countIn == CountIn::Sig2 || mt.countIn == CountIn::Sig4;
        countInBarBeats = mt.sigNum;
        countInBeatNo = 0;
        countInNext = 0;
        countInLeft = mt.countIn == CountIn::Beat2p4 ? countInBeatLength * 8 : countInBeatLength * countInBeats;
        pendingSink = sink;
        return;
    }
    if (recorder.record (armedTracks(), sink))
        recorder.leaveVamp();
}

void Desk::locateKey (int dir)
{
    if (sdMode)
    {
        if (sdIndex >= 0)
        {
            const int next = dir < 0 ? (sdPos > (int64_t) (sampleRate * 2) ? sdIndex : sdIndex - 1) : sdIndex + 1;
            if (next >= 0 && next < (int) sdFiles.size())
                sdPlayStart (next);
            else if (dir < 0)
                sdPos = 0;
        }
        return;
    }
    if (recorder.isRecording())
        return;
    const auto pos = recorder.getPosition();
    std::vector<std::pair<int64_t, juce::String>> points { { 0, {} }, { recorder.songLength(), {} } };
    const auto& info = recorder.info;
    if (info.punchIn >= 0)
        points.push_back ({ info.punchIn, "PUNCH IN" });
    if (info.punchOut >= 0)
        points.push_back ({ info.punchOut, "PUNCH OUT" });
    for (size_t i = 0; i < info.marks.size(); ++i)
        points.push_back ({ info.marks[i], "MARK " + juce::String ((int) i + 1).paddedLeft ('0', 2) });
    if (vampScreenOpen())
        for (size_t i = 0; i < info.vamps.size(); ++i)
        {
            points.push_back ({ info.vamps[i].first, "VAMP-" + juce::String ((int) i + 1) + " IN" });
            points.push_back ({ info.vamps[i].second, "VAMP-" + juce::String ((int) i + 1) + " OUT" });
        }
    std::sort (points.begin(), points.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    // going back while playing skips the point just passed (half a second's grace)
    const int64_t grace = rolling (recorder.getState()) ? (int64_t) (sampleRate * 0.5) : 0;
    const std::pair<int64_t, juce::String>* target = nullptr;
    if (dir < 0)
    {
        for (auto& p : points)
            if (p.first < pos - grace)
                target = &p;
    }
    else
        for (auto& p : points)
            if (p.first > pos)
            {
                target = &p;
                break;
            }
    if (target == nullptr)
        return;
    recorder.locate (target->first);
    needSpp = needFullFrame = true;
    if (target->second.isNotEmpty())
        pullUp (target->second);
}

void Desk::clickKey()
{
    const auto& t = top();
    // on the EFFECT screen with a delay's TIME selected, or the TEMPO screen: tap (p.42, p.51)
    if (t.id == ScreenId::Effect && t.field == 1 && ((fx.type >= 7 && fx.type <= 9) || fx.type == 12 || fx.type == 13))
    {
        taps.push_back (clock);
        if (taps.size() >= 2)
        {
            const double ms = (double) (taps.back() - taps[taps.size() - 2]) / sampleRate * 1000.0;
            if (ms > 2000.0)
                taps.erase (taps.begin(), taps.end() - 1);
            else
                setFxParamFromTap (ms);
        }
        return;
    }
    if (t.id == ScreenId::Tempo)
    {
        tapTempo();
        return;
    }
    metroOn = ! metroOn;
    freeBeatPhase = 0.0;
    beatIndex = -1;
}

void Desk::tapTempo()
{
    taps.push_back (clock);
    if (taps.size() < 2)
        return;
    const double gap = (double) (taps.back() - taps[taps.size() - 2]) / sampleRate;
    if (gap > 3.0)
    {
        taps.erase (taps.begin(), taps.end() - 1);
        return;
    }
    // the average of the last few taps
    const size_t from = taps.size() > 5 ? taps.size() - 5 : 0;
    const double avg = (double) (taps.back() - taps[from]) / (double) (taps.size() - 1 - from) / sampleRate;
    auto& mt = recorder.info.metronome;
    mt.tempo = juce::jlimit (20.0, 250.0, std::round (60.0 / avg));
    if (top().id == ScreenId::Tempo)
        top().value = (int) mt.tempo;
    recorder.markDirty();
}

void Desk::setFxParamFromTap (double ms)
{
    fx.param[(size_t) fx.type] = juce::jlimit (0.0f, 1.0f, (float) ((ms - 20.0) / 980.0));
    updateLibrary();
    settingsChanged();
}

void Desk::updateLibrary()
{
    if (fx.libSlot >= 0)
        fx.library[(size_t) fx.libSlot] = { true, fx.type, fx.param[(size_t) fx.type] };
}

void Desk::footswitch (int i)
{
    switch (settings.footFn[(size_t) i])
    {
        case FootFn::Off: break;
        case FootFn::PlayPause:
            if (recorder.getState() != Recorder::State::Recording)
                playKey();
            break;
        case FootFn::PunchIO:
            if (recorder.isRecording())
                stopKey();
            else if (recorder.getState() == Recorder::State::Playing)
                recordKey();
            break;
        case FootFn::FxMute:
            master.fxMute = ! master.fxMute;
            settingsChanged();
            break;
        case FootFn::RecStop:
            if (recorder.isRecording() || countInLeft > 0)
                stopKey();
            else
                recordKey();
            break;
        case FootFn::Marker:
            if (rolling (recorder.getState()))
                addOrClearMark();
            break;
        case FootFn::MarkSkip:
        {
            const auto pos = recorder.getPosition();
            for (auto m : recorder.info.marks)
                if (m > pos)
                {
                    recorder.locate (m);
                    needSpp = needFullFrame = true;
                    break;
                }
            break;
        }
        case FootFn::SongIncrement:
            if (stopped() && storage != nullptr)
            {
                auto list = storage->songs();
                for (size_t s = 0; s < list.size(); ++s)
                    if (list[s].name == recorder.info.name && s + 1 < list.size())
                    {
                        loadSong (list[s + 1].name);
                        break;
                    }
            }
            break;
        case FootFn::Tap:
            if (top().id == ScreenId::Effect || top().id == ScreenId::Tempo)
                clickKey();
            break;
        case FootFn::Vamp: recorder.vampToggle(); recorder.markDirty(); break;
        case FootFn::VampIOSet:
        {
            auto& v = recorder.info.vamps;
            if (recorder.isVamping())
            {
                recorder.vampToggle();
                break;
            }
            const auto st = recorder.getState();
            if (st != Recorder::State::Playing && st != Recorder::State::Paused)
                break;
            auto& pendingIn = vampPendingIn;
            const auto pos = recorder.getPosition();
            if (pendingIn < 0)
            {
                if ((int) v.size() >= kMaxVamps)
                    message ("VAMP full");
                else
                {
                    pendingIn = pos;
                    pullUp ("VAMP IN");
                }
            }
            else
            {
                if (pos - pendingIn < (int64_t) (2.0 * sampleRate))
                    message ("I/O Too Short");
                else
                {
                    v.push_back ({ pendingIn, pos });
                    std::sort (v.begin(), v.end());
                    recorder.info.vampOn = true;
                    recorder.markDirty();
                    pullUp ("VAMP OUT");
                }
                pendingIn = -1;
            }
            break;
        }
    }
}

void Desk::addOrClearMark()
{
    auto& marks = recorder.info.marks;
    const auto pos = recorder.getPosition();
    const auto st = recorder.getState();
    if (rolling (st))
    {
        if ((int) marks.size() >= kMaxMarks)
            return;
        marks.push_back (pos);
        std::sort (marks.begin(), marks.end());
        const auto index = (int) (std::find (marks.begin(), marks.end(), pos) - marks.begin());
        pullUp ("MARK " + juce::String (index + 1).paddedLeft ('0', 2));
        if (st == Recorder::State::Recording)
            recorder.markDirty(); // (saved when recording stops)
        else
            marksUnsaved = true;
        return;
    }
    // stopped or paused on a mark: clear it after a confirmation (p.49)
    const auto it = std::find (marks.begin(), marks.end(), pos);
    if (it == marks.end())
        return;
    const auto index = (int) (it - marks.begin());
    confirmScreen ("MARK CLEAR", { "MARK " + juce::String (index + 1).paddedLeft ('0', 2), "Clear?" }, [this, pos] {
        auto& m = recorder.info.marks;
        m.erase (std::remove (m.begin(), m.end(), pos), m.end());
        marksUnsaved = true;
        pop();
    });
}

void Desk::message (const juce::String& a, const juce::String& b, double seconds)
{
    popup = {};
    popup.active = true;
    popup.kind = Popup::Message;
    popup.lines.add (a);
    if (b.isNotEmpty())
        popup.lines.add (b);
    popup.until = clock + (int64_t) (seconds * sampleRate);
}

void Desk::confirm (const juce::StringArray& lines, std::function<void()> action)
{
    popup = {};
    popup.active = true;
    popup.kind = Popup::Confirm;
    popup.lines = lines;
    popup.onConfirm = std::move (action);
}

void Desk::pullUp (const juce::String& s)
{
    pullUpText = s;
    pullUpUntil = clock + (int64_t) (1.5 * sampleRate);
}

// ======================================================================== songs

void Desk::startUp (const juce::String& lastSong)
{
    if (storage == nullptr)
        return;
    auto list = storage->songs();
    juce::String name;
    for (auto& s : list)
        if (s.name == lastSong)
            name = s.name;
    if (name.isEmpty() && ! list.empty())
    {
        auto newest = list.front();
        for (auto& s : list)
            if (s.written > newest.written)
                newest = s;
        name = newest.name;
    }
    if (name.isEmpty())
    {
        newSong (sampleRate > 46000.0 ? 48000.0 : 44100.0, 24, storage->newSongName (settings));
        return;
    }
    loadSong (name);
}

void Desk::loadSong (const juce::String& name)
{
    if (storage == nullptr || ! stopped())
        return;
    recorder.stop();
    recorder.clearSong();
    if (! storage->loadSong (name, recorder))
    {
        loaded = false;
        message ("File Not Found");
        return;
    }
    loaded = true;
    marksUnsaved = false;
    recorder.forgetUndo();
    recorder.locate (0);
    applyMetronomeSettings();
    needSpp = needFullFrame = true;
    settingsChanged(); // (the last song is remembered)
}

void Desk::saveSong()
{
    if (storage == nullptr || ! loaded)
        return;
    if (storage->saveSong (recorder))
        marksUnsaved = false;
    else
        message ("Can't Save Data");
}

void Desk::newSong (double rate, int bits, const juce::String& name)
{
    if (storage == nullptr)
        return;
    if (loaded && ! recorder.info.protect)
        saveSong();
    SongInfo info;
    info.name = name;
    info.sampleRate = rate;
    info.bits = bits;
    info.metronome = Metronome(); // (tempo 120, 4/4: p.35)
    info.metronome.clickOut = recorder.info.metronome.clickOut;
    if (! storage->createSong (info))
    {
        message ("Song number full");
        return;
    }
    loadSong (name);
}

void Desk::applyMetronomeSettings()
{
    recorder.vampAuto = settings.vampAuto;
    recorder.preRollSec = settings.preRollSec;
}

void Desk::collectPasses()
{
    Recorder::Pass p;
    bool any = false;
    while (recorder.takeFinishedPass (p))
    {
        std::array<std::shared_ptr<TakeAudio>, kTracks> audio {};
        for (int t = 0; t < kTracks; ++t)
            if (p.sink != nullptr)
                audio[(size_t) t] = p.sink->finish (t);
        recorder.addPass (p, audio);
        any = true;
    }
    if (any || recorder.takeDirty())
        saveSong();
}

bool Desk::takeSettingsDirty()
{
    const bool d = settingsDirty;
    settingsDirty = false;
    return d;
}

bool Desk::takePairingRequest()
{
    const bool r = pairingRequest;
    pairingRequest = false;
    return r;
}

void Desk::pairWith (const juce::String& name, std::vector<std::shared_ptr<TakeAudio>> audio)
{
    if (audio.empty())
        return;
    btName = name;
    btAudio = std::move (audio);
    btPos = 0;
    pairing = Pairing::Paired;
}

void Desk::idle()
{
    collectPasses();
    if (popup.active && popup.kind == Popup::Message && popup.until > 0 && clock >= popup.until)
        popup = {};
    if (pullUpText.isNotEmpty() && clock >= pullUpUntil)
        pullUpText = {};
    // pairing: two minutes waiting (p.32); held two seconds to unpair
    if (pairing == Pairing::Waiting && clock - pairingSince > (int64_t) (120.0 * sampleRate))
        pairing = Pairing::Off;
    if (down[(size_t) Key::Pairing] && pairing == Pairing::Paired && clock - downAt[(size_t) Key::Pairing] > (int64_t) (2.0 * sampleRate))
    {
        pairing = Pairing::Waiting;
        pairingSince = clock;
        btAudio.clear();
        btName = {};
    }
    // SD PLAY: at the end of a file, the next one (or again)
    if (sdMode && sdRolling && sdIndex >= 0 && ! sdAudio.empty() && sdPos >= sdAudio[0]->length())
    {
        if (sdRepeat == 1)
            sdPos = 0;
        else if (sdIndex + 1 < (int) sdFiles.size())
            sdPlayStart (sdIndex + 1);
        else if (sdRepeat == 2)
            sdPlayStart (0);
        else
        {
            sdRolling = false;
            sdPos = 0;
        }
    }
    if (songToLoadNext.isNotEmpty())
    {
        auto n = songToLoadNext;
        songToLoadNext = {};
        loadSong (n);
    }
}

void Desk::sdPlayStart (int index)
{
    if (storage == nullptr || index < 0 || index >= (int) sdFiles.size())
        return;
    auto audio = storage->openMusic (sdFiles[(size_t) index].path);
    if (audio.empty())
    {
        message ("Non-supported");
        return;
    }
    sdAudio = std::move (audio);
    sdIndex = index;
    sdPos = 0;
    sdRolling = true;
    sdPaused = false;
}

void Desk::sdPlayStop()
{
    sdRolling = false;
    sdPaused = false;
    sdAudio.clear();
    sdIndex = -1;
    sdPos = 0;
}

// ======================================================================== lights and meters

Desk::Lights Desk::lights() const
{
    Lights l;
    l.blinkPhase = (clock / (int64_t) (sampleRate * 0.25)) % 2 == 0;
    const auto st = recorder.getState();
    const bool rec = st == Recorder::State::Recording;
    for (int s = 0; s < kStrips; ++s)
    {
        // REC: blinking when armed, lit while recording (p.40)
        l.rec[(size_t) s] = ! recKey[(size_t) s] ? Lamp::Off : rec ? Lamp::On : Lamp::Blink;
        l.sigGreen[(size_t) s] = meters.sigGreen[(size_t) s];
        l.sigRed[(size_t) s] = meters.sigRed[(size_t) s];
        l.comp[(size_t) s] = meters.compActive[(size_t) s];
        l.mute[(size_t) s] = strips[(size_t) s].mute;
    }
    l.post = settings.aux1PostFader;
    l.pflAfl = Mixer::anySolo (strips) || master.aux1Afl || master.aux2Afl || master.fxAfl;
    l.phantom = master.phantom;
    l.usb = usbConnected;
    if (sdMode)
    {
        l.play = sdRolling ? (sdPaused ? Lamp::Blink : Lamp::On) : Lamp::Off;
        l.stop = sdRolling ? Lamp::Off : Lamp::On;
    }
    else
    {
        l.play = rolling (st) ? Lamp::On : st == Recorder::State::Paused ? Lamp::Blink : Lamp::Off;
        l.stop = st == Recorder::State::Stopped && countInLeft == 0 ? Lamp::On : Lamp::Off;
        l.record = rec ? Lamp::On : (recorder.isRehearsal() && recorder.isPunchedIn()) || countInLeft > 0 ? Lamp::Blink : Lamp::Off;
    }
    l.pairing = pairing == Pairing::Paired ? Lamp::On : pairing == Pairing::Waiting ? Lamp::Blink : Lamp::Off;
    l.tempo = clock < tempoLampUntil ? (tempoLampRed ? Lamp::Red : Lamp::Green) : Lamp::Off;
    l.aux1Mute = master.aux1Mute;
    l.aux2Mute = master.aux2Mute;
    l.fxMute = master.fxMute;
    l.subMute = master.subMute;
    l.mainMute = master.mainMute;
    return l;
}

Mixer::Meters Desk::takeMeters()
{
    auto m = meterAccum;
    meterAccum = Mixer::Meters();
    return m;
}

// ======================================================================== audio

void Desk::process (int n, const AudioIO& io, juce::MidiBuffer* midiOut)
{
    if (scratch.empty())
        prepare (sampleRate, std::max (n, maxBlock));
    int done = 0;
    while (done < n)
    {
        int m = std::min (n - done, maxBlock);
        // ---- searching (REW / FF held)
        for (auto k : { Key::Rew, Key::Ff })
            if (down[(size_t) k] && ! sdMode && ! recorder.isRecording() && clock - downAt[(size_t) k] >= (int64_t) (kHoldSeconds * sampleRate))
            {
                searching = true;
                searchDir = k == Key::Rew ? -1 : 1;
            }
        if (searching)
        {
            const double heldFor = (double) (clock - downAt[(size_t) (searchDir < 0 ? Key::Rew : Key::Ff)]) / sampleRate;
            const int speed = heldFor > 3.0 ? 60 : 10;
            recorder.locate (recorder.getPosition() + (int64_t) searchDir * m * speed);
            needSpp = needFullFrame = true;
        }
        // ---- the count-in: recording starts when it ends
        bool countingIn = countInLeft > 0;
        if (countingIn)
            m = (int) std::min<int64_t> (m, countInLeft);
        else
            m = recorder.samplesUntilEvent (m);
        m = std::max (1, m);

        const auto st = recorder.getState();
        const bool roll = rolling (st) && ! searching;

        // ---- the recorder's playback (and SD PLAY on 9/10, the paired device)
        std::array<float*, kTracks> pb {};
        for (int t = 0; t < kTracks; ++t)
            pb[(size_t) t] = scratch[(size_t) (kScratchTracks + t)].data();
        recorder.renderPlayback (m, pb.data());
        if (sdMode)
        {
            for (int t : { 8, 9 })
                std::fill (pb[(size_t) t], pb[(size_t) t] + m, 0.0f);
            if (sdRolling && ! sdPaused && ! sdAudio.empty())
            {
                const int64_t len = sdAudio[0]->length();
                const int k = (int) juce::jlimit<int64_t> (0, m, len - sdPos);
                if (k > 0)
                {
                    sdAudio[0]->read (sdPos, pb[8], k);
                    sdAudio[sdAudio.size() > 1 ? 1 : 0]->read (sdPos, pb[9], k);
                }
                sdPos += m;
            }
        }
        float* btL = scratch[(size_t) kScratchBt].data();
        float* btR = scratch[(size_t) kScratchBt + 1].data();
        const bool bt = pairing == Pairing::Paired && ! btAudio.empty();
        if (bt)
        {
            const int64_t len = btAudio[0]->length();
            for (int i = 0; i < m;)
            {
                if (btPos >= len)
                    btPos = 0;
                const int k = (int) std::min<int64_t> (m - i, len - btPos);
                if (k <= 0)
                    break;
                btAudio[0]->read (btPos, btL + i, k);
                btAudio[btAudio.size() > 1 ? 1 : 0]->read (btPos, btR + i, k);
                btPos += k;
                i += k;
            }
        }

        // ---- what each strip hears in MTR mode (p.39)
        const bool punched = recorder.isPunchedIn();
        for (int s = 0; s < kStrips; ++s)
        {
            const bool armed = recKey[(size_t) s];
            Mixer::Source src;
            if (sdMode && s == 7)
                src = Mixer::Source::Playback;
            else if (! roll || countingIn)
                src = armed ? Mixer::Source::Input : Mixer::Source::Muted;
            else if (punched)
                src = armed ? Mixer::Source::Input : Mixer::Source::Playback;
            else
                src = armed ? Mixer::Source::InputAndPlayback : Mixer::Source::Playback;
            sources[(size_t) s] = src;
        }

        // ---- the metronome
        float* click = scratch[(size_t) kScratchClick].data();
        std::fill (click, click + m, 0.0f);
        const auto& mt = recorder.info.metronome;
        const double beatLen = 60.0 / mt.tempo * sampleRate;
        struct Hit
        {
            int at;
            bool accent;
        };
        Hit hits[8];
        int nHits = 0;
        if (countingIn)
        {
            // the count-in pattern: 2+4 BEAT is two half notes then four quarters
            int64_t t = countInNext;
            while (t < m && countInBeatNo < countInBeats)
            {
                const bool accent = countInAccentFirst && countInBeatNo % countInBarBeats == 0;
                if (nHits < 8)
                    hits[nHits++] = { (int) t, accent };
                const bool half = mt.countIn == CountIn::Beat2p4 && countInBeatNo < 2;
                t += half ? countInBeatLength * 2 : countInBeatLength;
                ++countInBeatNo;
            }
            countInNext = t - m;
        }
        else
        {
            bool sounding = metroOn;
            if (! sounding && roll)
                switch (mt.clickOut)
                {
                    case ClickOut::Off: break;
                    case ClickOut::CountInRec:
                    case ClickOut::Rec: sounding = st == Recorder::State::Recording; break;
                    case ClickOut::RecPlay: sounding = true; break;
                }
            if (sounding)
            {
                if (roll)
                {
                    // locked to the song position, so it agrees with the bar display and the MIDI clock
                    const int64_t pos = recorder.getPosition();
                    int64_t k = (int64_t) std::ceil ((double) pos / beatLen);
                    for (;; ++k)
                    {
                        const int64_t at = (int64_t) std::llround ((double) k * beatLen) - pos;
                        if (at >= m)
                            break;
                        if (at >= 0 && k != beatIndex && nHits < 8)
                        {
                            hits[nHits++] = { (int) at, k % std::max (1, mt.sigNum) == 0 };
                            beatIndex = k;
                        }
                    }
                }
                else
                {
                    // free running (the CLICK key while stopped)
                    const double before = freeBeatPhase;
                    freeBeatPhase += (double) m / beatLen;
                    for (int64_t k = (int64_t) std::ceil (before); (double) k < freeBeatPhase; ++k)
                        if (nHits < 8)
                        {
                            const int at = juce::jlimit (0, m - 1, (int) (((double) k - before) * beatLen));
                            hits[nHits++] = { at, k % std::max (1, mt.sigNum) == 0 };
                            beatIndex = k;
                        }
                }
            }
        }
        {
            int from = 0;
            for (int h = 0; h < nHits; ++h)
            {
                clickVoice.render (click + from, hits[h].at - from);
                from = hits[h].at;
                clickVoice.trigger (mt.sound, hits[h].accent, mt.accent);
                tempoLampRed = hits[h].accent;
                tempoLampUntil = clock + hits[h].at + (int64_t) (0.08 * sampleRate);
            }
            clickVoice.render (click + from, m - from);
        }

        // ---- the mixer
        Mixer::IO mio;
        for (int i = 0; i < kInputs; ++i)
        {
            mio.live[(size_t) i] = io.live[(size_t) i] != nullptr ? io.live[(size_t) i] + done : nullptr;
            mio.pc[(size_t) i] = io.pc[(size_t) i] != nullptr ? io.pc[(size_t) i] + done : nullptr;
        }
        for (int t = 0; t < kTracks; ++t)
        {
            mio.mtr[(size_t) t] = pb[(size_t) t];
            mio.rec[(size_t) t] = scratch[(size_t) (kScratchRec + t)].data();
        }
        mio.btL = bt ? btL : nullptr;
        mio.btR = bt ? btR : nullptr;
        mio.click = nHits > 0 || clickVoice.active() ? click : nullptr;
        auto at = [done] (float* p) { return p != nullptr ? p + done : nullptr; };
        mio.mainL = at (io.mainL);
        mio.mainR = at (io.mainR);
        mio.subL = at (io.subL);
        mio.subR = at (io.subR);
        mio.aux1 = at (io.aux1);
        mio.aux2 = at (io.aux2);
        mio.phones1L = at (io.phones1L);
        mio.phones1R = at (io.phones1R);
        mio.phones2L = at (io.phones2L);
        mio.phones2R = at (io.phones2R);
        mio.clickOut = at (io.clickOut);
        // the computer's mix: USB 11/12 (MULTI INPUT) or 1/2 (STEREO MIX)
        const int mixAt = settings.usbStereoMix ? 0 : 10;
        mio.usbMixL = at (io.usb[(size_t) mixAt]);
        mio.usbMixR = at (io.usb[(size_t) mixAt + 1]);
        mio.source = sources;
        mio.fxAvailable = ! aux2JackUsed;
        Mixer::Meters segMeters;
        mixer.process (m, mio, strips, master, settings, fx, mt, segMeters);
        meters = segMeters;
        meterAccum.mergeFrom (segMeters);
        if (! settings.usbStereoMix)
            for (int t = 0; t < kInputs; ++t)
                if (io.usb[(size_t) t] != nullptr)
                    std::copy_n (scratch[(size_t) (kScratchRec + t)].data(), m, io.usb[(size_t) t] + done);

        // ---- recording
        std::array<const float*, kTracks> recIn {};
        for (int t = 0; t < kTracks; ++t)
            recIn[(size_t) t] = scratch[(size_t) (kScratchRec + t)].data();
        recorder.capture (m, recIn.data());

        // ---- MIDI: clock and song position, time code (p.53)
        if (midiOut != nullptr && ! countingIn)
        {
            const int64_t pos = recorder.getPosition();
            const double pulse = beatLen / 24.0;
            if (settings.midiClock)
            {
                if (roll && ! wasRolling)
                {
                    const int spp = (int) ((double) pos / (pulse * 6.0));
                    midiOut->addEvent (juce::MidiMessage::songPositionPointer (spp), done);
                    midiOut->addEvent (spp == 0 ? juce::MidiMessage::midiStart() : juce::MidiMessage::midiContinue(), done);
                    needSpp = false;
                }
                else if (! roll && wasRolling)
                    midiOut->addEvent (juce::MidiMessage::midiStop(), done);
                if (! roll && needSpp)
                {
                    midiOut->addEvent (juce::MidiMessage::songPositionPointer ((int) ((double) pos / (pulse * 6.0))), done);
                    needSpp = false;
                }
                if (roll)
                    for (int64_t k = (int64_t) std::ceil ((double) pos / pulse);; ++k)
                    {
                        const int64_t a = (int64_t) std::llround ((double) k * pulse) - pos;
                        if (a >= m)
                            break;
                        if (a >= 0)
                            midiOut->addEvent (juce::MidiMessage::midiClock(), done + (int) a);
                    }
            }
            if (settings.midiTimecode)
            {
                auto tc = [this] (int64_t p, int& h, int& mi, int& s, int& f) {
                    const double secs = (double) p / sampleRate;
                    const int64_t frames = (int64_t) (secs * 30.0);
                    f = (int) (frames % 30);
                    s = (int) ((frames / 30) % 60);
                    mi = (int) ((frames / 1800) % 60);
                    h = (int) ((frames / 108000) % 24);
                };
                if (needFullFrame || (roll && ! wasRolling))
                {
                    int h, mi, s, f;
                    tc (pos, h, mi, s, f);
                    midiOut->addEvent (juce::MidiMessage::fullFrame (h, mi, s, f, juce::MidiMessage::fps30), done);
                    needFullFrame = false;
                }
                if (roll)
                {
                    const double qf = sampleRate / 120.0; // four quarter frames a frame, 30 frames a second
                    for (int64_t k = (int64_t) std::ceil ((double) pos / qf);; ++k)
                    {
                        const int64_t a = (int64_t) std::llround ((double) k * qf) - pos;
                        if (a >= m)
                            break;
                        if (a < 0)
                            continue;
                        const int piece = (int) (k % 8);
                        int h, mi, s, f;
                        tc ((int64_t) ((double) (k - piece) * qf), h, mi, s, f);
                        int v = 0;
                        switch (piece)
                        {
                            case 0: v = f & 15; break;
                            case 1: v = f >> 4; break;
                            case 2: v = s & 15; break;
                            case 3: v = s >> 4; break;
                            case 4: v = mi & 15; break;
                            case 5: v = mi >> 4; break;
                            case 6: v = h & 15; break;
                            case 7: v = (h >> 4) | (3 << 1); break; // 30 fps non-drop
                        }
                        midiOut->addEvent (juce::MidiMessage::quarterFrame (piece, v), done + (int) a);
                    }
                }
            }
        }
        wasRolling = roll;

        // ---- on to the next segment
        if (countingIn)
        {
            countInLeft -= m;
            if (countInLeft <= 0)
            {
                countInLeft = 0;
                if (pendingSink != nullptr && recorder.record (armedTracks(), pendingSink))
                    recorder.leaveVamp();
                pendingSink.reset();
            }
        }
        else if (! searching)
            recorder.advance (m);
        clock += m;
        done += m;
    }
}
} // namespace trackdesk
