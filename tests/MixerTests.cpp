#include "core/Mixer.h"
#include <juce_audio_basics/juce_audio_basics.h>

using namespace trackdesk;

namespace
{
/** One mixer, a sine on chosen inputs, all outputs captured. */
struct Bench
{
    static constexpr int N = 4800; // 0.1 s blocks
    double sr = 48000.0;
    Mixer mixer;
    std::array<Strip, kStrips> strips;
    Master master;
    Settings settings;
    FxSettings fx;
    Metronome metro;
    Mixer::Meters meters;
    std::array<std::vector<float>, kInputs> live, pc;
    std::array<std::vector<float>, kTracks> mtr, rec;
    std::vector<float> btL, btR, click;
    std::vector<float> mainL, mainR, subL, subR, aux1, aux2, p1L, p1R, p2L, p2R, clickOut, usbL, usbR;
    std::array<bool, kInputs> liveOn {}, pcOn {};
    std::array<bool, kTracks> mtrOn {};
    bool btOn = false, clickOn = false, fxAvailable = true;
    std::array<Mixer::Source, kStrips> source {};
    int64_t t = 0;

    Bench()
    {
        mixer.prepare (sr, N);
        fx.on = false; // (effects off unless a test wants them)
        for (auto* v : { &btL, &btR, &click, &mainL, &mainR, &subL, &subR, &aux1, &aux2, &p1L, &p1R, &p2L, &p2R, &clickOut, &usbL, &usbR })
            v->assign (N, 0.0f);
        for (auto& v : live)
            v.assign (N, 0.0f);
        for (auto& v : pc)
            v.assign (N, 0.0f);
        for (auto& v : mtr)
            v.assign (N, 0.0f);
        for (auto& v : rec)
            v.assign (N, 0.0f);
        for (auto& s : strips)
            s.fader = law::kFaderUnity; // 0 dB
        master.mainFader = master.subFader = law::kFaderUnity;
        master.phones1 = master.phones2 = 0.75f;
        source.fill (Mixer::Source::Input);
    }
    static void fillSine (std::vector<float>& v, float amp, double hz, double sr, int64_t t0)
    {
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * (double) (t0 + (int64_t) i) / sr);
    }
    void run (int blocks = 3)
    {
        for (int b = 0; b < blocks; ++b)
        {
            Mixer::IO io;
            for (int i = 0; i < kInputs; ++i)
            {
                io.live[(size_t) i] = liveOn[(size_t) i] ? live[(size_t) i].data() : nullptr;
                io.pc[(size_t) i] = pcOn[(size_t) i] ? pc[(size_t) i].data() : nullptr;
            }
            for (int k = 0; k < kTracks; ++k)
            {
                io.mtr[(size_t) k] = mtrOn[(size_t) k] ? mtr[(size_t) k].data() : nullptr;
                io.rec[(size_t) k] = rec[(size_t) k].data();
            }
            io.btL = btOn ? btL.data() : nullptr;
            io.btR = btOn ? btR.data() : nullptr;
            io.click = clickOn ? click.data() : nullptr;
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
            io.usbMixL = usbL.data();
            io.usbMixR = usbR.data();
            io.source = source;
            io.fxAvailable = fxAvailable;
            mixer.process (N, io, strips, master, settings, fx, metro, meters);
            t += N;
        }
    }
    static float peak (const std::vector<float>& v)
    {
        float p = 0;
        for (auto x : v)
            p = std::max (p, std::abs (x));
        return p;
    }
    static float db (const std::vector<float>& v) { return juce::Decibels::gainToDecibels (peak (v), -120.0f); }
    void line (int input, float amp = 0.5f, double hz = 1000.0)
    {
        liveOn[(size_t) input] = true;
        fillSine (live[(size_t) input], amp, hz, sr, 0); // (whole cycles per block at 1 kHz)
    }
};
} // namespace

class MixerTests : public juce::UnitTest
{
public:
    MixerTests() : juce::UnitTest ("Mixer", "trackdesk") {}

    void runTest() override
    {
        beginTest ("a line input at unity reaches MAIN at -3 dB each side (panned centre)");
        {
            Bench b;
            b.line (0);
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.5f) - 3.0f, 0.2f);
            expectWithinAbsoluteError (Bench::db (b.mainR), Bench::db (b.mainL), 0.05f);
            expect (b.meters.sigGreen[0] && ! b.meters.sigRed[0], "SIG green, not red");
            expect (Bench::peak (b.subL) == 0.0f, "nothing on SUB (its switch is off)");
        }

        beginTest ("GAIN, the fader, MUTE and the MAIN switch");
        {
            Bench b;
            b.line (2, 0.1f);
            b.strips[2].gain = 0.4f; // +10 dB
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.1f) + 10.0f - 3.0f, 0.3f);
            b.strips[2].gain = 0.2f;
            b.strips[2].fader = law::faderPosForDb (-10.0f);
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.1f) - 10.0f - 3.0f, 0.3f);
            b.strips[2].mute = true;
            b.run();
            expect (Bench::peak (b.mainL) < 1.0e-6f, "MUTE");
            b.strips[2].mute = false;
            b.strips[2].main = false;
            b.run();
            expect (Bench::peak (b.mainL) < 1.0e-6f, "MAIN off");
            b.strips[2].sub = true;
            b.run();
            expect (Bench::peak (b.subL) > 0.01f, "SUB on");
        }

        beginTest ("SIG goes red near clipping; the stereo channels' gain range");
        {
            Bench b;
            b.line (0, 0.5f);
            b.strips[0].gain = 0.4f; // +10 dB: 1.58
            b.run();
            expect (b.meters.sigRed[0]);
            Bench s;
            s.line (6, 0.5f);
            s.line (7, 0.5f);
            s.strips[6].gain = 0.4f; // 0 dB on 7-10
            s.run();
            expectWithinAbsoluteError (Bench::db (s.mainL), juce::Decibels::gainToDecibels (0.5f), 0.2f); // (balance: unity at the centre)
        }

        beginTest ("PAN and BAL");
        {
            Bench b;
            b.line (0);
            b.strips[0].pan = 0.0f;
            b.run();
            expect (Bench::peak (b.mainR) < 1.0e-4f && Bench::peak (b.mainL) > 0.4f, "hard left");
            Bench s;
            s.line (8);
            s.line (9);
            s.strips[7].gain = 0.4f;
            s.strips[7].pan = 1.0f;
            s.run();
            expect (Bench::peak (s.mainL) < 1.0e-4f && Bench::peak (s.mainR) > 0.4f, "balance right");
        }

        beginTest ("LOW CUT, PHASE");
        {
            Bench b;
            b.line (0, 0.5f, 30.0);
            b.run();
            const float open = Bench::peak (b.mainL);
            b.strips[0].lowCut = true;
            b.run (6);
            expect (Bench::peak (b.mainL) < open * 0.3f, "30 Hz is cut");
            Bench p;
            p.line (0);
            p.line (1);
            p.strips[1].phaseInvert = true;
            p.run();
            expect (Bench::peak (p.mainL) < 1.0e-4f, "an inverted copy cancels");
        }

        beginTest ("MODE: PC skips the preamp; MTR follows the recorder");
        {
            Bench b;
            b.pcOn[0] = true;
            Bench::fillSine (b.pc[0], 0.5f, 1000.0, b.sr, 0);
            b.strips[0].gain = 1.0f; // (+40 dB on the jack: shouldn't matter)
            b.strips[0].mode = Mode::Pc;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.5f) - 3.0f, 0.2f);
            b.strips[0].mode = Mode::Mtr;
            b.mtrOn[0] = true;
            Bench::fillSine (b.mtr[0], 0.25f, 1000.0, b.sr, 0);
            b.source[0] = Mixer::Source::Playback;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.25f) - 3.0f, 0.2f);
            b.source[0] = Mixer::Source::Muted;
            b.run();
            expect (Bench::peak (b.mainL) < 1.0e-6f);
        }

        beginTest ("what's recorded: PRE COMP is the input jack; POST EQ has the EQ");
        {
            Bench b;
            b.line (0, 0.5f, 1000.0);
            b.strips[0].eqMid = 1.0f;
            b.strips[0].eqMidFreq = law::midKnobForHz (1000.0f);
            b.strips[0].fader = 0.0f; // (the fader never affects the recording)
            b.run();
            expectWithinAbsoluteError (Bench::db (b.rec[0]), juce::Decibels::gainToDecibels (0.5f), 0.2f);
            b.strips[0].sendPoint = SendPoint::PostEq;
            b.run();
            expect (Bench::db (b.rec[0]) > juce::Decibels::gainToDecibels (0.5f) + 10.0f, "post EQ");
            // PC mode, PRE COMP: still the input jack (p.54)
            Bench p;
            p.pcOn[0] = true;
            Bench::fillSine (p.pc[0], 0.3f, 1000.0, p.sr, 0);
            p.line (0, 0.1f);
            p.strips[0].mode = Mode::Pc;
            p.run();
            expectWithinAbsoluteError (Bench::db (p.rec[0]), juce::Decibels::gainToDecibels (0.1f), 0.2f);
            p.strips[0].sendPoint = SendPoint::PostComp;
            p.run();
            expectWithinAbsoluteError (Bench::db (p.rec[0]), juce::Decibels::gainToDecibels (0.3f), 0.2f);
        }

        beginTest ("tracks 11/12: the MAIN MIX before the fader, after it with POST REC");
        {
            Bench b;
            b.line (0);
            b.master.mainFader = law::faderPosForDb (-20.0f);
            b.run();
            expectWithinAbsoluteError (Bench::db (b.rec[kMixTrack]), juce::Decibels::gainToDecibels (0.5f) - 3.0f, 0.2f);
            expectWithinAbsoluteError (Bench::db (b.usbL), Bench::db (b.rec[kMixTrack]), 0.1f);
            b.master.postRec = true;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.rec[kMixTrack]), juce::Decibels::gainToDecibels (0.5f) - 23.0f, 0.3f);
        }

        beginTest ("solo: PFL to the phones and meters; in place mutes the others");
        {
            Bench b;
            b.line (0, 0.5f);
            b.line (1, 0.1f, 500.0);
            b.strips[0].solo = true;
            b.strips[0].fader = 0.0f; // PFL is before the fader
            b.run();
            expect (Bench::peak (b.p1L) > 0.3f && Bench::peak (b.p2L) > 0.3f, "both phones hear the SOLO bus");
            expect (Mixer::soloBusActive (b.strips, b.master, b.settings));
            expectWithinAbsoluteError (b.meters.mainL, Bench::peak (b.p1L), 0.01f);
            b.settings.soloMode = SoloMode::InPlace;
            b.strips[0].fader = law::kFaderUnity;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.5f) - 3.0f, 0.3f); // only channel 1
            expect (! Mixer::soloBusActive (b.strips, b.master, b.settings));
            b.strips[0].solo = false;
            b.run();
            expect (Bench::peak (b.mainL) > 0.36f, "both again");
        }

        beginTest ("AUX 1 pre/post fader, AUX 2 post fader, AUX outputs, AFL");
        {
            Bench b;
            b.line (0);
            b.strips[0].aux1 = 0.75f;
            b.strips[0].aux2 = 0.75f;
            b.strips[0].fader = 0.0f;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.aux1), juce::Decibels::gainToDecibels (0.5f), 0.2f);
            expect (Bench::peak (b.aux2) < 1.0e-6f, "AUX 2 is post fader");
            b.settings.aux1PostFader = true;
            b.run();
            expect (Bench::peak (b.aux1) < 1.0e-6f, "AUX 1 post fader");
            b.settings.aux1PostFader = false;
            b.strips[0].fader = law::kFaderUnity;
            b.master.aux1Mute = true;
            b.run();
            expect (Bench::peak (b.aux1) < 1.0e-6f && Bench::peak (b.aux2) > 0.4f, "AUX 1 MUTE");
            b.master.aux1Mute = false;
            b.master.aux2Afl = true;
            b.run();
            expectWithinAbsoluteError (Bench::peak (b.p1L), Bench::peak (b.aux2), 0.01f);
            b.master.aux2Afl = false;
            b.master.phones1Aux = true;
            b.run();
            expectWithinAbsoluteError (Bench::peak (b.p1L), Bench::peak (b.aux1), 0.01f);
            expectWithinAbsoluteError (Bench::peak (b.p1R), Bench::peak (b.aux2), 0.01f);
        }

        beginTest ("the effect: fed by AUX 2, back through the FX fader; not with the AUX 2 jack in use");
        {
            Bench b;
            b.line (0);
            b.strips[0].main = false;
            b.strips[0].aux2 = 0.75f;
            b.fx.on = true;
            b.run (5);
            expect (Bench::peak (b.mainL) > 0.01f, "the return on MAIN");
            expect (b.meters.fx > 0.01f);
            b.master.fxMute = true;
            b.run (2);
            expect (Bench::peak (b.mainL) < 1.0e-5f, "FX MUTE");
            b.master.fxMute = false;
            b.master.fxMain = false;
            b.master.fxSub = true;
            b.run (2);
            expect (Bench::peak (b.mainL) < 1.0e-5f && Bench::peak (b.subL) > 0.01f, "to SUB");
            b.master.fxAux1 = true;
            b.master.fxToAux1 = 0.75f;
            b.run (2);
            expect (Bench::peak (b.aux1) > 0.01f, "TO AUX 1");
            b.fxAvailable = false;
            b.run (20);
            expect (Bench::peak (b.subL) < 1.0e-5f, "N/A when AUX 2 is plugged");
        }

        beginTest ("SUB into MAIN, SD MAIN MIX RETURN, the output pads and delay");
        {
            Bench b;
            b.line (0);
            b.strips[0].main = false;
            b.strips[0].sub = true;
            b.run();
            expect (Bench::peak (b.mainL) < 1.0e-6f);
            b.master.subMain = true;
            b.run();
            expect (Bench::peak (b.mainL) > 0.3f, "SUB > MAIN");
            b.master.subMain = false;
            b.mtrOn[kMixTrack] = b.mtrOn[kMixTrack + 1] = true;
            Bench::fillSine (b.mtr[kMixTrack], 0.2f, 1000.0, b.sr, 0);
            b.master.sdReturn = true;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.2f), 0.2f);
            b.master.sdReturn = false;
            b.master.subMain = true;
            b.settings.padMainDb = -20;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.5f) - 3.0f - 20.0f, 0.3f);
            b.settings.padSubDb = -6;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.subL), juce::Decibels::gainToDecibels (0.5f) - 3.0f - 6.0f, 0.3f);
            // OUTPUT DELAY: an impulse arrives 100 ms later on MAIN
            Bench d;
            d.liveOn[0] = true;
            d.live[0][0] = 1.0f;
            d.settings.outputDelayOn = d.settings.outputDelayOnMain = true;
            d.settings.outputDelayMs = 50;
            d.run (1);
            int at = -1;
            for (int i = 0; i < Bench::N; ++i)
                if (std::abs (d.mainL[(size_t) i]) > 0.1f && at < 0)
                    at = i;
            expectEquals (at, 2400);
        }

        beginTest ("the paired device: into 9/10 or the MAIN MIX");
        {
            Bench b;
            b.btOn = true;
            Bench::fillSine (b.btL, 0.3f, 1000.0, b.sr, 0);
            Bench::fillSine (b.btR, 0.3f, 1000.0, b.sr, 0);
            b.master.assign = Assign::Off;
            b.run();
            expect (Bench::peak (b.mainL) < 1.0e-6f);
            b.master.assign = Assign::ToMain;
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.3f), 0.2f);
            b.master.assign = Assign::To910;
            b.strips[7].fader = law::faderPosForDb (-6.0f);
            b.run();
            expectWithinAbsoluteError (Bench::db (b.mainL), juce::Decibels::gainToDecibels (0.3f) - 6.0f, 0.3f);
        }

        beginTest ("the click: CLICK jack, the phones and buses, never recorded");
        {
            Bench b;
            b.clickOn = true;
            std::fill (b.click.begin(), b.click.end(), 0.0f);
            b.click[100] = 1.0f;
            b.metro.clickVolume = 50;
            b.metro.routeOn[0] = true;  // PHONES 1
            b.metro.routeOn[3] = true;  // MAIN bus
            b.metro.routeVolume[3] = 50;
            b.run (1);
            expectWithinAbsoluteError (Bench::peak (b.clickOut), 1.0f, 0.001f);
            expect (Bench::peak (b.p1L) > 0.1f, "in the phones");
            expect (Bench::peak (b.mainL) > 0.5f, "on MAIN");
            expect (Bench::peak (b.rec[kMixTrack]) == 0.0f, "not on tracks 11/12");
            b.metro.routeOn[3] = false;
            b.run (1);
            expect (Bench::peak (b.p2L) == 0.0f, "not on PHONES 2 (off)");
            expect (Bench::peak (b.p1L) > 0.1f);
        }

        beginTest ("the master EQ: on MAIN or on the AUX outputs");
        {
            Bench b;
            b.line (0, 0.25f, 12000.0);
            b.strips[0].aux1 = 0.75f;
            b.master.eqHigh = 1.0f;
            b.run();
            const float flat = Bench::peak (b.mainL);
            b.master.eqIn = true;
            b.run (4);
            expect (Bench::peak (b.mainL) > flat * 2.5f, "EQ IN on MAIN");
            const float auxFlat = Bench::peak (b.aux1);
            b.master.eqOnAux = true;
            b.run (4);
            expect (Bench::peak (b.mainL) < flat * 1.1f && Bench::peak (b.aux1) > auxFlat * 2.5f, "on AUX 1/2 instead");
        }
    }
};

static MixerTests mixerTests;
