#include "core/Dsp.h"
#include <juce_audio_basics/juce_audio_basics.h>

using namespace trackdesk;

namespace
{
/** The gain (dB) of a filter chain at a frequency, measured with a sine. */
template <typename Fn>
float measureDb (double sr, double hz, Fn&& process)
{
    float inPeak = 0.0f, outPeak = 0.0f;
    const int n = (int) (sr * 0.5);
    for (int i = 0; i < n; ++i)
    {
        const float x = (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / sr);
        const float y = process (x);
        if (i > n / 2)
        {
            inPeak = std::max (inPeak, std::abs (x));
            outPeak = std::max (outPeak, std::abs (y));
        }
    }
    return juce::Decibels::gainToDecibels (outPeak / inPeak);
}
} // namespace

class DspTests : public juce::UnitTest
{
public:
    DspTests() : juce::UnitTest ("DSP", "trackdesk") {}

    void runTest() override
    {
        const double sr = 48000.0;

        beginTest ("the panel's laws");
        expectWithinAbsoluteError (law::gainDb (0.2f, 0), 0.0f, 0.01f);   // unity on a line input, channels 1-6
        expectWithinAbsoluteError (law::gainDb (0.0f, 0), -10.0f, 0.01f);
        expectWithinAbsoluteError (law::gainDb (1.0f, 0), 40.0f, 0.01f);
        expectWithinAbsoluteError (law::gainDb (0.0f, 7), -20.0f, 0.01f); // -20..+30 on 7-10
        expectWithinAbsoluteError (law::gainDb (1.0f, 7), 30.0f, 0.01f);
        expectWithinAbsoluteError (law::faderDb (law::kFaderUnity), 0.0f, 0.01f);
        expectWithinAbsoluteError (law::faderDb (1.0f), 10.0f, 0.01f);
        expect (law::faderGain (0.0f) == 0.0f, "-infinity at the bottom");
        expectWithinAbsoluteError (law::faderPosForDb (-10.0f), 0.482f, 0.001f);
        expectWithinAbsoluteError (law::sendGain (0.75f), 1.0f, 0.001f);
        expectWithinAbsoluteError (juce::Decibels::gainToDecibels (law::sendGain (1.0f)), 15.0f, 0.01f);
        expect (law::sendGain (0.0f) == 0.0f);
        expectWithinAbsoluteError (law::levelGain (0.75f), 1.0f, 0.001f);
        expectWithinAbsoluteError (law::midHz (0.0f), 100.0f, 0.1f);
        expectWithinAbsoluteError (law::midHz (1.0f), 8000.0f, 1.0f);
        expectWithinAbsoluteError (law::midHz (0.4f), 579.0f, 25.0f); // (the 600 Hz default)
        float l = 0, r = 0;
        law::pan (0.5f, l, r);
        expectWithinAbsoluteError (juce::Decibels::gainToDecibels (l), -3.0f, 0.05f); // -3 dB centre (p.21)
        law::pan (0.0f, l, r);
        expect (l > 0.999f && r < 0.001f, "hard left: left only");
        law::balance (0.5f, l, r);
        expect (l == 1.0f && r == 1.0f, "balance: unity at the centre");
        law::balance (1.0f, l, r);
        expect (l == 0.0f && r == 1.0f);

        beginTest ("channel EQ: shelves and the mid band");
        {
            dsp::ThreeBand eq;
            eq.set (sr, 0.0f, 80.0f, 0.0f, 600.0f, 1.0f, 15.0f, 10000.0f);
            expectWithinAbsoluteError (measureDb (sr, 16000.0, [&] (float x) { return eq.process (x); }), 15.0f, 1.5f);
            eq.reset();
            expectWithinAbsoluteError (measureDb (sr, 200.0, [&] (float x) { return eq.process (x); }), 0.0f, 0.5f);
            dsp::ThreeBand low;
            low.set (sr, -15.0f, 80.0f, 0.0f, 600.0f, 1.0f, 0.0f, 10000.0f);
            expectWithinAbsoluteError (measureDb (sr, 30.0, [&] (float x) { return low.process (x); }), -15.0f, 2.0f);
            dsp::ThreeBand mid;
            mid.set (sr, 0.0f, 80.0f, 12.0f, 1000.0f, 1.0f, 0.0f, 10000.0f);
            expectWithinAbsoluteError (measureDb (sr, 1000.0, [&] (float x) { return mid.process (x); }), 12.0f, 0.5f);
            expect (! mid.flat());
            dsp::ThreeBand flat;
            flat.set (sr, 0.0f, 80.0f, 0.0f, 600.0f, 1.0f, 0.0f, 10000.0f);
            expect (flat.flat(), "a flat EQ is skipped");
        }

        beginTest ("LOW CUT: 100 Hz");
        {
            dsp::Biquad hp;
            hp.setHighPass (sr, 100.0);
            expect (measureDb (sr, 30.0, [&] (float x) { return hp.process (x); }) < -15.0f);
            hp.reset();
            expectWithinAbsoluteError (measureDb (sr, 1000.0, [&] (float x) { return hp.process (x); }), 0.0f, 0.3f);
        }

        beginTest ("COMP: off does nothing; up, it holds loud signals down");
        {
            dsp::Compressor c;
            c.prepare (sr);
            c.setAmount (0.0f);
            float g = 1.0f;
            for (int i = 0; i < 4800; ++i)
                g = c.gainFor (0.9f);
            expectWithinAbsoluteError (g, 1.0f, 0.001f);
            c.setAmount (1.0f);
            for (int i = 0; i < 9600; ++i)
                g = c.gainFor (0.9f);
            expect (c.reductionDb() > 10.0f, "heavy reduction at MAX");
            const float loud = 0.9f * g;
            for (int i = 0; i < 9600; ++i)
                g = c.gainFor (0.05f);
            expect (0.05f * g > 0.05f * 0.9f, "make-up gain on quiet signals");
            expect (loud < 0.9f);
        }

        beginTest ("the 16 effects: each makes a wet signal and dies away");
        {
            for (int type = 0; type < kFxTypes; ++type)
            {
                dsp::Effect fx;
                fx.prepare (sr);
                fx.setType (type, 0.5f);
                std::vector<float> in (512, 0.0f), outL (512), outR (512);
                in[0] = 1.0f;
                float energy = 0.0f;
                for (int b = 0; b < 200; ++b)
                {
                    fx.process (in.data(), outL.data(), outR.data(), 512);
                    std::fill (in.begin(), in.end(), 0.0f);
                    for (int i = 0; i < 512; ++i)
                        energy += outL[(size_t) i] * outL[(size_t) i] + outR[(size_t) i] * outR[(size_t) i];
                }
                expect (energy > 1.0e-4f, juce::String (fxName (type)) + " is audible");
                // after a long silence it's quiet again
                for (int b = 0; b < 1200; ++b)
                    fx.process (in.data(), outL.data(), outR.data(), 512);
                float tail = 0.0f;
                for (int i = 0; i < 512; ++i)
                    tail = std::max (tail, std::abs (outL[(size_t) i]) + std::abs (outR[(size_t) i]));
                expect (tail < 0.01f, juce::String (fxName (type)) + " decays");
            }
        }

        beginTest ("the click's eight sounds, with an accent");
        {
            dsp::ClickVoice v;
            v.prepare (sr);
            for (int s = 0; s < 8; ++s)
            {
                std::vector<float> a (2048, 0.0f), b (2048, 0.0f);
                v.trigger ((ClickSound) s, false, Accent::High);
                v.render (a.data(), 2048);
                v.trigger ((ClickSound) s, true, Accent::High);
                v.render (b.data(), 2048);
                float pa = 0, pb = 0;
                for (int i = 0; i < 2048; ++i)
                {
                    pa = std::max (pa, std::abs (a[(size_t) i]));
                    pb = std::max (pb, std::abs (b[(size_t) i]));
                }
                expect (pa > 0.01f, juce::String (clickSoundName ((ClickSound) s)) + " sounds");
                expect (pb > pa * 1.5f, "the accent is louder");
            }
        }
    }
};

static DspTests dspTests;
