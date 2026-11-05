#include "Dsp.h"
#include <juce_core/juce_core.h>

namespace trackdesk::dsp
{
namespace
{
    constexpr double kPi = 3.14159265358979323846;
    void normalise (Biquad& f, double b0, double b1, double b2, double a0, double a1, double a2)
    {
        f.b0 = (float) (b0 / a0);
        f.b1 = (float) (b1 / a0);
        f.b2 = (float) (b2 / a0);
        f.a1 = (float) (a1 / a0);
        f.a2 = (float) (a2 / a0);
    }
} // namespace

void Biquad::setLowShelf (double sr, double hz, double db, double q)
{
    const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * hz / sr, c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * q);
    const double sq = 2.0 * std::sqrt (A) * alpha;
    normalise (*this, A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq), (A + 1) + (A - 1) * c + sq,
               -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
}

void Biquad::setHighShelf (double sr, double hz, double db, double q)
{
    const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * hz / sr, c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * q);
    const double sq = 2.0 * std::sqrt (A) * alpha;
    normalise (*this, A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq), (A + 1) - (A - 1) * c + sq,
               2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
}

void Biquad::setPeak (double sr, double hz, double db, double q)
{
    const double A = std::pow (10.0, db / 40.0), w = 2.0 * kPi * hz / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    normalise (*this, 1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
}

void Biquad::setHighPass (double sr, double hz, double q)
{
    const double w = 2.0 * kPi * hz / sr, c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
    normalise (*this, (1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
}

void ThreeBand::set (double sr, float lowDb, float lowHz, float midDb, float midHz, float midQ, float highDb, float highHz)
{
    if (std::abs (lowDb - lastLow) > 0.01f)
    {
        low.setLowShelf (sr, lowHz, lowDb);
        lastLow = lowDb;
    }
    if (std::abs (midDb - lastMid) > 0.01f || std::abs (midHz - lastHz) > 0.5f || std::abs (midQ - lastQ) > 0.001f)
    {
        mid.setPeak (sr, midHz, midDb, midQ);
        lastMid = midDb;
        lastHz = midHz;
        lastQ = midQ;
    }
    if (std::abs (highDb - lastHigh) > 0.01f)
    {
        high.setHighShelf (sr, highHz, highDb);
        lastHigh = highDb;
    }
}

// ---------------------------------------------------------------- compressor

void Compressor::prepare (double sr)
{
    sampleRate = sr;
    attack = (float) std::exp (-1.0 / (0.005 * sr));
    release = (float) std::exp (-1.0 / (0.12 * sr));
    envDb = -120.0f;
}

void Compressor::setAmount (float knob)
{
    amount = juce::jlimit (0.0f, 1.0f, knob);
    threshDb = -36.0f * amount;
    ratio = 1.5f + 4.5f * amount;
    makeupDb = -threshDb * (1.0f - 1.0f / ratio) * 0.5f;
}

float Compressor::gainFor (float level)
{
    if (amount <= 0.001f)
    {
        lastReductionDb = 0.0f;
        return 1.0f;
    }
    const float db = juce::Decibels::gainToDecibels (level, -120.0f);
    envDb = db > envDb ? attack * envDb + (1.0f - attack) * db : release * envDb + (1.0f - release) * db;
    const float over = envDb - threshDb;
    const float reduction = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
    lastReductionDb = reduction;
    return juce::Decibels::decibelsToGain (makeupDb - reduction);
}

// ---------------------------------------------------------------- the effect

void Effect::Reverb::prepare (double sr)
{
    static const int combMs[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static const int apMs[4] = { 556, 441, 341, 225 };
    const double k = sr / 44100.0;
    for (int i = 0; i < 8; ++i)
    {
        combL[(size_t) i].buf.assign ((size_t) (combMs[i] * k * 1.6), 0.0f);
        combR[(size_t) i].buf.assign ((size_t) ((combMs[i] + 23) * k * 1.6), 0.0f);
    }
    for (int i = 0; i < 4; ++i)
    {
        apL[(size_t) i].buf.assign ((size_t) (apMs[i] * k), 0.0f);
        apR[(size_t) i].buf.assign ((size_t) ((apMs[i] + 23) * k), 0.0f);
    }
    pre.assign ((size_t) (sr * 0.2), 0.0f);
}

void Effect::Reverb::configure (double sr, float size, float damp, float preDelayMs, float level)
{
    // size scales how much of each comb is used (longer rooms) and the feedback (longer tails)
    static const int combMs[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    const double k = sr / 44100.0;
    for (int i = 0; i < 8; ++i)
    {
        for (auto* c : { &combL[(size_t) i], &combR[(size_t) i] })
        {
            c->feedback = 0.70f + 0.28f * size;
            c->damp = damp;
        }
        juce::ignoreUnused (combMs, k);
    }
    preLen = juce::jlimit (1, (int) pre.size() - 1, (int) (preDelayMs * 0.001 * sr));
    wet = level;
}

void Effect::Reverb::process (float in, float& l, float& r)
{
    pre[(size_t) prePos] = in;
    int readPos = prePos - preLen;
    if (readPos < 0)
        readPos += (int) pre.size();
    const float x = pre[(size_t) readPos] * 0.015f;
    if (++prePos >= (int) pre.size())
        prePos = 0;
    float sl = 0.0f, sr = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        sl += combL[(size_t) i].process (x);
        sr += combR[(size_t) i].process (x);
    }
    for (int i = 0; i < 4; ++i)
    {
        sl = apL[(size_t) i].process (sl);
        sr = apR[(size_t) i].process (sr);
    }
    l = sl * wet;
    r = sr * wet;
}

void Effect::Reverb::clear()
{
    for (auto* arr : { &combL, &combR })
        for (auto& c : *arr)
        {
            std::fill (c.buf.begin(), c.buf.end(), 0.0f);
            c.store = 0.0f;
        }
    for (auto* arr : { &apL, &apR })
        for (auto& a : *arr)
            std::fill (a.buf.begin(), a.buf.end(), 0.0f);
    std::fill (pre.begin(), pre.end(), 0.0f);
}

void Effect::Delay::prepare (double sr)
{
    l.assign ((size_t) (sr * 1.1), 0.0f);
    r.assign ((size_t) (sr * 1.1), 0.0f);
}

void Effect::Delay::process (float in, float& outL, float& outR)
{
    int rp = pos - len;
    if (rp < 0)
        rp += (int) l.size();
    const float dl = l[(size_t) rp], dr = r[(size_t) rp];
    if (pingPong)
    {
        l[(size_t) pos] = in + dr * feedback; // the echoes cross from side to side
        r[(size_t) pos] = dl * feedback;
    }
    else
    {
        l[(size_t) pos] = in + dl * feedback;
        r[(size_t) pos] = in + dr * feedback;
    }
    if (++pos >= (int) l.size())
        pos = 0;
    outL = dl;
    outR = dr;
}

void Effect::Delay::clear()
{
    std::fill (l.begin(), l.end(), 0.0f);
    std::fill (r.begin(), r.end(), 0.0f);
}

void Effect::Modulation::prepare (double sr) { buf.assign ((size_t) (sr * 0.06), 0.0f); }

void Effect::Modulation::process (double sr, float in, float& outL, float& outR)
{
    buf[(size_t) pos] = in + last * feedback;
    auto tap = [&] (double ph) {
        const double ms = baseMs + depthMs * (0.5 + 0.5 * std::sin (ph));
        double rp = pos - ms * 0.001 * sr;
        while (rp < 0)
            rp += (double) buf.size();
        const int i0 = (int) rp, i1 = (i0 + 1) % (int) buf.size();
        const float t = (float) (rp - i0);
        return buf[(size_t) i0] * (1.0f - t) + buf[(size_t) i1] * t;
    };
    const float a = tap (phase), b = tap (phase + kPi * 0.5);
    last = a;
    phase += 2.0 * kPi * rate / sr;
    if (phase > 2.0 * kPi)
        phase -= 2.0 * kPi;
    if (++pos >= (int) buf.size())
        pos = 0;
    outL = a;
    outR = b;
}

void Effect::Modulation::clear()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    last = 0.0f;
}

void Effect::prepare (double sr)
{
    sampleRate = sr;
    reverb.prepare (sr);
    delay.prepare (sr);
    mod.prepare (sr);
    setType (type, param);
}

void Effect::reset()
{
    reverb.clear();
    delay.clear();
    mod.clear();
}

void Effect::setType (int t, float p)
{
    if (t != type)
        reset();
    type = juce::jlimit (0, kFxTypes - 1, t);
    param = juce::jlimit (0.0f, 1.0f, p);
    // reverbs: each preset its own room; the parameter is the reverb time
    struct RoomPreset
    {
        float size, damp, preMs, level;
    };
    static const RoomPreset rooms[7] = {
        { 0.80f, 0.30f, 25.0f, 0.55f }, // HALL 1
        { 0.90f, 0.25f, 40.0f, 0.55f }, // HALL 2
        { 0.45f, 0.45f, 8.0f, 0.60f },  // ROOM 1
        { 0.60f, 0.35f, 12.0f, 0.60f }, // ROOM 2
        { 0.75f, 0.10f, 2.0f, 0.55f },  // PLATE (bright, no pre-delay)
        { 0.40f, 0.55f, 5.0f, 0.60f },  // STUDIO
        { 0.70f, 0.40f, 30.0f, 0.55f }, // LIVE
    };
    const int room = type <= 6 ? type : (type == 12 || type == 14 ? 0 : 1); // the combos' halls
    const float timeScale = type <= 6 ? param : 0.55f;
    reverb.configure (sampleRate, juce::jlimit (0.0f, 1.0f, rooms[room].size * (0.55f + 0.45f * timeScale) + 0.12f * timeScale), rooms[room].damp,
                      rooms[room].preMs, rooms[room].level);
    // delays: the parameter is the delay time (20 ms - 1 s)
    const float delayParam = type == 7 || type == 8 || type == 9 || type == 12 || type == 13 ? param : 0.3f;
    delay.len = juce::jlimit (1, (int) delay.l.size() - 1, (int) ((20.0 + 980.0 * delayParam) * 0.001 * sampleRate));
    delay.feedback = type == 8 ? 0.55f : type == 9 ? 0.5f : 0.35f;
    delay.pingPong = type == 9;
    // chorus / flanger: the parameter is the rate
    mod.rate = 0.1 + 4.9 * (type >= 10 ? param : 0.5f);
    if (type == 11)
    {
        mod.baseMs = 1.5f;
        mod.depthMs = 3.0f;
        mod.feedback = 0.6f;
    }
    else
    {
        mod.baseMs = 14.0f;
        mod.depthMs = 6.0f;
        mod.feedback = 0.0f;
    }
}

void Effect::process (const float* in, float* outL, float* outR, int n)
{
    for (int i = 0; i < n; ++i)
    {
        const float x = in[i];
        float l = 0.0f, r = 0.0f, l2 = 0.0f, r2 = 0.0f;
        if (type <= 6)
            reverb.process (x, l, r);
        else if (type <= 9)
        {
            delay.process (x, l, r);
        }
        else if (type <= 11)
        {
            mod.process (sampleRate, x, l, r);
            if (type == 10)
            {
                l = 0.7f * l;
                r = 0.7f * r;
            }
        }
        else if (type <= 13) // DELAY+HALL: the echoes go into the hall
        {
            delay.process (x, l, r);
            reverb.process (x * 0.6f + (l + r) * 0.3f, l2, r2);
            l = l * 0.7f + l2;
            r = r * 0.7f + r2;
        }
        else // CHORUS+HALL
        {
            mod.process (sampleRate, x, l, r);
            reverb.process (x * 0.5f + (l + r) * 0.25f, l2, r2);
            l = l * 0.5f + l2;
            r = r * 0.5f + r2;
        }
        outL[i] = l;
        outR[i] = r;
    }
}

// ---------------------------------------------------------------- the metronome

void ClickVoice::prepare (double sr)
{
    sampleRate = sr;
    juce::Random rng (7);
    auto make = [&] (double seconds, auto&& fn) {
        std::vector<float> v ((size_t) (seconds * sr));
        for (size_t i = 0; i < v.size(); ++i)
            v[i] = fn ((double) i / sr);
        return v;
    };
    sounds[0] = make (0.03, [] (double t) { return (float) (std::sin (2 * kPi * 2000 * t) * std::exp (-t * 180)); });                       // CLICK
    sounds[1] = make (0.05, [&] (double t) { return (float) ((rng.nextFloat() * 2 - 1) * 0.6 * std::exp (-t * 120) + std::sin (2 * kPi * 1300 * t) * std::exp (-t * 90)); }); // STICK
    sounds[2] = make (0.4, [] (double t) { return (float) ((std::sin (2 * kPi * 1760 * t) + 0.5 * std::sin (2 * kPi * 4430 * t)) * 0.6 * std::exp (-t * 9)); }); // BELL
    sounds[3] = make (0.25, [] (double t) {
        const double f = 50 + 120 * std::exp (-t * 30);
        return (float) (std::sin (2 * kPi * f * t) * std::exp (-t * 14));
    }); // KICK
    sounds[4] = make (0.2, [&] (double t) { return (float) (((rng.nextFloat() * 2 - 1) * 0.7 + 0.5 * std::sin (2 * kPi * 190 * t)) * std::exp (-t * 22)); }); // SNARE
    sounds[5] = make (0.06, [&] (double t) { return (float) ((rng.nextFloat() * 2 - 1) * 0.5 * std::exp (-t * 70)); });                                     // PEDAL HIHAT
    sounds[6] = make (0.35, [&] (double t) { return (float) ((rng.nextFloat() * 2 - 1) * 0.45 * std::exp (-t * 9)); });                                     // OPEN HIHAT
    sounds[7] = make (0.05, [&] (double t) { return (float) ((rng.nextFloat() * 2 - 1) * 0.5 * std::exp (-t * 90)); });                                     // CLOSE HIHAT
    // the hats are high-passed (a crude difference), so they sound like metal, not hiss
    for (int s : { 5, 6, 7 })
    {
        float prev = 0.0f;
        for (auto& x : sounds[(size_t) s])
        {
            const float y = x - prev;
            prev = x;
            x = y;
        }
    }
    for (auto& s : sounds)
        current.reserve (std::max (current.capacity(), s.size()));
}

void ClickVoice::trigger (ClickSound s, bool accent, Accent level)
{
    const auto& src = sounds[(size_t) s];
    current.assign (src.begin(), src.end()); // (capacity reserved in prepare: no allocation)
    pos = 0;
    static const float accentGain[4] = { 1.0f, 1.25f, 1.6f, 2.0f };
    gain = 0.5f * (accent ? accentGain[(int) level] : 1.0f);
}

void ClickVoice::render (float* out, int n)
{
    for (int i = 0; i < n && pos < (int) current.size(); ++i, ++pos)
        out[i] += current[(size_t) pos] * gain;
}
} // namespace trackdesk::dsp
