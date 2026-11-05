#pragma once

// Signal processing: the channel and master EQ filters, the one-knob compressor, the built-in effect (16 presets)
// and the metronome's click sounds.

#include "Model.h"
#include <cmath>
#include <vector>

namespace trackdesk::dsp
{
/** A biquad in transposed direct form II, with the usual RBJ designs. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setLowShelf (double sr, double hz, double db, double q = 0.707);
    void setHighShelf (double sr, double hz, double db, double q = 0.707);
    void setPeak (double sr, double hz, double db, double q);
    void setHighPass (double sr, double hz, double q = 0.707);
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
};

/** A 3-band EQ (low shelf, peak, high shelf) for one channel, recalculated only when its settings change. */
struct ThreeBand
{
    Biquad low, mid, high;
    float lastLow = -999, lastMid = -999, lastHigh = -999, lastHz = -1, lastQ = -1;
    void set (double sr, float lowDb, float lowHz, float midDb, float midHz, float midQ, float highDb, float highHz);
    float process (float x) { return high.process (mid.process (low.process (x))); }
    bool flat() const { return std::abs (lastLow) < 0.05f && std::abs (lastMid) < 0.05f && std::abs (lastHigh) < 0.05f; }
    void reset()
    {
        low.reset();
        mid.reset();
        high.reset();
    }
};

/** The one-knob compressor: turning it up lowers the threshold (0 dB down to -36 dB) and raises the ratio
    (1.5:1 to 6:1), with automatic make-up of part of the reduction. Stereo strips share one detector. */
struct Compressor
{
    void prepare (double sr);
    void setAmount (float knob);
    /** Processes one sample (or a linked stereo pair); returns the gain applied. */
    float gainFor (float level);
    float reductionDb() const { return lastReductionDb; }

private:
    double sampleRate = 48000.0;
    float amount = 0.0f, threshDb = 0.0f, ratio = 1.0f, makeupDb = 0.0f;
    float envDb = -120.0f, attack = 0.0f, release = 0.0f, lastReductionDb = 0.0f;
};

/** The built-in effect: mono in (the AUX 2/FX bus), stereo out, one of 16 presets with one parameter. */
class Effect
{
public:
    void prepare (double sampleRate);
    void reset();
    void setType (int type, float param);
    void process (const float* in, float* outL, float* outR, int n);
    int getType() const { return type; }

private:
    struct Comb
    {
        std::vector<float> buf;
        int pos = 0;
        float store = 0, feedback = 0.8f, damp = 0.2f;
        float process (float x)
        {
            const float y = buf[(size_t) pos];
            store = y * (1.0f - damp) + store * damp;
            buf[(size_t) pos] = x + store * feedback;
            if (++pos >= (int) buf.size())
                pos = 0;
            return y;
        }
    };
    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;
        float process (float x)
        {
            const float b = buf[(size_t) pos];
            const float y = -x + b;
            buf[(size_t) pos] = x + b * 0.5f;
            if (++pos >= (int) buf.size())
                pos = 0;
            return y;
        }
    };
    struct Reverb
    {
        std::array<Comb, 8> combL, combR;
        std::array<Allpass, 4> apL, apR;
        std::vector<float> pre;
        int prePos = 0, preLen = 1;
        float wet = 0.3f;
        void prepare (double sr);
        void configure (double sr, float size, float damp, float preDelayMs, float level);
        void process (float in, float& l, float& r);
        void clear();
    };
    struct Delay
    {
        std::vector<float> l, r;
        int pos = 0, len = 1;
        float feedback = 0.35f;
        bool pingPong = false;
        void prepare (double sr);
        void process (float in, float& outL, float& outR);
        void clear();
    };
    struct Modulation
    {
        std::vector<float> buf;
        int pos = 0;
        double phase = 0.0, rate = 0.5;
        float depthMs = 3.0f, baseMs = 15.0f, feedback = 0.0f, last = 0.0f;
        void prepare (double sr);
        void process (double sr, float in, float& outL, float& outR);
        void clear();
    };

    double sampleRate = 48000.0;
    int type = 0;
    float param = 0.5f;
    Reverb reverb;
    Delay delay;
    Modulation mod;
};

/** The metronome's sounds (CLICK, STICK, BELL, KICK, SNARE, PEDAL/OPEN/CLOSED HIHAT), rendered once. */
class ClickVoice
{
public:
    void prepare (double sampleRate);
    void trigger (ClickSound sound, bool accent, Accent level);
    /** Adds the sound into out (mono). */
    void render (float* out, int n);
    bool active() const { return pos < (int) current.size(); }

private:
    double sampleRate = 48000.0;
    std::array<std::vector<float>, 8> sounds;
    std::vector<float> current;
    int pos = 1 << 30;
    float gain = 1.0f;
};

/** A smoothed gain, ramped over a block. */
struct Ramp
{
    float value = 0.0f;
    bool started = false;
    /** Gain for sample i of n, heading to target. */
    void apply (float target, float* x, int n)
    {
        if (! started)
        {
            value = target;
            started = true;
        }
        const float step = (target - value) / (float) std::max (1, n);
        for (int i = 0; i < n; ++i)
            x[i] *= value + step * (float) (i + 1);
        value = target;
    }
};
} // namespace trackdesk::dsp
