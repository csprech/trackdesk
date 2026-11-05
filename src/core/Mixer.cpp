#include "Mixer.h"

namespace trackdesk
{
namespace
{
    float peakOf (const float* x, int n)
    {
        float p = 0.0f;
        if (x != nullptr)
            for (int i = 0; i < n; ++i)
                p = std::max (p, std::abs (x[i]));
        return p;
    }
    void addScaled (float* dst, const float* src, float g, int n)
    {
        if (src == nullptr || dst == nullptr || g == 0.0f)
            return;
        for (int i = 0; i < n; ++i)
            dst[i] += src[i] * g;
    }
    void copyOrClear (float* dst, const float* src, int n)
    {
        if (dst == nullptr)
            return;
        if (src != nullptr)
            std::copy (src, src + n, dst);
        else
            std::fill (dst, dst + n, 0.0f);
    }
    void clear (std::vector<float>& v, int n) { std::fill (v.begin(), v.begin() + n, 0.0f); }

    /** A gain ramped from its last value over the block. */
    struct RampedGain
    {
        float from, to, step;
        RampedGain (dsp::Ramp& r, float target, int n)
        {
            from = r.started ? r.value : target;
            to = target;
            step = (to - from) / (float) std::max (1, n);
            r.started = true;
            r.value = target;
        }
        float at (int i) const { return from + step * (float) (i + 1); }
    };
} // namespace

void Mixer::Meters::mergeFrom (const Meters& o)
{
    for (size_t i = 0; i < channel.size(); ++i)
        channel[i] = std::max (channel[i], o.channel[i]);
    for (size_t i = 0; i < live.size(); ++i)
    {
        live[i] = std::max (live[i], o.live[i]);
        pc[i] = std::max (pc[i], o.pc[i]);
    }
    for (size_t i = 0; i < mtr.size(); ++i)
        mtr[i] = std::max (mtr[i], o.mtr[i]);
    for (size_t i = 0; i < sigGreen.size(); ++i)
    {
        sigGreen[i] = sigGreen[i] || o.sigGreen[i];
        sigRed[i] = sigRed[i] || o.sigRed[i];
        compActive[i] = compActive[i] || o.compActive[i];
    }
    mainL = std::max (mainL, o.mainL);
    mainR = std::max (mainR, o.mainR);
    mixL = std::max (mixL, o.mixL);
    mixR = std::max (mixR, o.mixR);
    aux1 = std::max (aux1, o.aux1);
    aux2 = std::max (aux2, o.aux2);
    fx = std::max (fx, o.fx);
}

void Mixer::DelayLine::prepare (int size)
{
    l.assign ((size_t) size, 0.0f);
    r.assign ((size_t) size, 0.0f);
    pos = 0;
}

void Mixer::DelayLine::process (float* L, float* R, int n, int delay)
{
    const int size = (int) l.size();
    delay = juce::jlimit (0, size - 1, delay);
    for (int i = 0; i < n; ++i)
    {
        l[(size_t) pos] = L[i];
        r[(size_t) pos] = R[i];
        int rp = pos - delay;
        if (rp < 0)
            rp += size;
        L[i] = l[(size_t) rp];
        R[i] = r[(size_t) rp];
        if (++pos >= size)
            pos = 0;
    }
}

void Mixer::prepare (double sr, int block)
{
    sampleRate = sr;
    maxBlock = std::max (1, block);
    for (auto* v : { &jackL, &jackR, &pcL, &pcR, &pbL, &pbR, &bufL, &bufR, &preL, &preR, &mainBusL, &mainBusR, &subBusL, &subBusR, &soloL, &soloR,
                     &aux1Bus, &aux2Bus, &fxL, &fxR, &aux1Out, &aux2Out })
        v->assign ((size_t) maxBlock, 0.0f);
    for (auto& s : strip)
    {
        s.comp.prepare (sr);
        for (auto& f : s.lowCut)
        {
            f.setHighPass (sr, 100.0, 0.707);
            f.reset();
        }
        for (auto& e : s.eq)
            e = dsp::ThreeBand();
    }
    for (auto& e : masterEq)
        e = dsp::ThreeBand();
    effect.prepare (sr);
    mainDelay.prepare ((int) (sr * 2.01) + 1);
    usbDelay.prepare ((int) (sr * 2.01) + 1);
}

bool Mixer::anySolo (const std::array<Strip, kStrips>& strips)
{
    for (auto& s : strips)
        if (s.solo)
            return true;
    return false;
}

bool Mixer::soloBusActive (const std::array<Strip, kStrips>& strips, const Master& m, const Settings& settings)
{
    return (settings.soloMode == SoloMode::Pfl && anySolo (strips)) || m.aux1Afl || m.aux2Afl || m.fxAfl;
}

void Mixer::process (int n, const IO& io, const std::array<Strip, kStrips>& strips, const Master& m, const Settings& settings, const FxSettings& fxs,
                     const Metronome& metro, Meters& meters)
{
    jassert (n <= maxBlock);
    n = std::min (n, maxBlock);
    meters = Meters();
    for (auto* v : { &mainBusL, &mainBusR, &subBusL, &subBusR, &soloL, &soloR, &aux1Bus, &aux2Bus })
        clear (*v, n);
    const bool solos = anySolo (strips);
    const bool inPlace = settings.soloMode == SoloMode::InPlace;
    for (int t = 0; t < kTracks; ++t)
        meters.mtr[(size_t) t] = peakOf (io.mtr[(size_t) t], n);
    for (int i = 0; i < kInputs; ++i)
        meters.pc[(size_t) i] = peakOf (io.pc[(size_t) i], n);

    // ---------------------------------------------------------------- the channel strips
    for (int s = 0; s < kStrips; ++s)
    {
        const auto& c = strips[(size_t) s];
        auto& d = strip[(size_t) s];
        const bool stereo = isStereo (s);
        const int in0 = firstInputOf (s);
        const int channels = stereo ? 2 : 1;

        // the input jack through the preamp: GAIN, LOW CUT, PHASE
        const float g = juce::Decibels::decibelsToGain (law::gainDb (c.gain, s)) * (c.phaseInvert ? -1.0f : 1.0f);
        float sigPeak = 0.0f;
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& x = ch == 0 ? jackL : jackR;
            const float* raw = io.live[(size_t) (in0 + ch)];
            if (raw == nullptr && ch == 1)
                raw = io.live[(size_t) in0]; // (a stereo channel with only its L jack: mono on both sides)
            float peak = 0.0f;
            for (int i = 0; i < n; ++i)
            {
                float v = raw != nullptr ? raw[i] * g : 0.0f;
                if (c.lowCut)
                    v = d.lowCut[(size_t) ch].process (v);
                x[(size_t) i] = v;
                peak = std::max (peak, std::abs (v));
            }
            meters.live[(size_t) (in0 + ch)] = peak;
            sigPeak = std::max (sigPeak, peak);
        }
        meters.sigGreen[(size_t) s] = sigPeak > 0.01f; // -40 dB
        meters.sigRed[(size_t) s] = sigPeak > 0.89f;   // about -1 dBFS: close to clipping
        // the computer (digital: no preamp)
        copyOrClear (pcL.data(), io.pc[(size_t) in0], n);
        if (stereo)
            copyOrClear (pcR.data(), io.pc[(size_t) in0 + 1] != nullptr ? io.pc[(size_t) in0 + 1] : io.pc[(size_t) in0], n);
        // the tracks' playback
        copyOrClear (pbL.data(), io.mtr[(size_t) in0], n);
        if (stereo)
            copyOrClear (pbR.data(), io.mtr[(size_t) in0 + 1], n);

        // what the strip hears (MODE)
        float* L = bufL.data();
        float* R = bufR.data();
        const auto src = io.source[(size_t) s];
        for (int i = 0; i < n; ++i)
        {
            const size_t k = (size_t) i;
            const float jl = jackL[k], jr = stereo ? jackR[k] : 0.0f;
            switch (c.mode)
            {
                case Mode::Pc:
                    L[i] = pcL[k];
                    R[i] = stereo ? pcR[k] : 0.0f;
                    break;
                case Mode::Live:
                    L[i] = jl;
                    R[i] = jr;
                    break;
                case Mode::Mtr:
                {
                    const float pl = pbL[k], pr = stereo ? pbR[k] : 0.0f;
                    switch (src)
                    {
                        case Source::Input: L[i] = jl; R[i] = jr; break;
                        case Source::Playback: L[i] = pl; R[i] = pr; break;
                        case Source::InputAndPlayback: L[i] = jl + pl; R[i] = jr + pr; break;
                        case Source::Muted: L[i] = R[i] = 0.0f; break;
                    }
                    break;
                }
            }
        }
        if (s == 7 && m.assign == Assign::To910) // the paired device on 9/10
        {
            addScaled (L, io.btL, 1.0f, n);
            addScaled (R, io.btR, 1.0f, n);
        }

        // what's recorded (and sent to the computer): PRE COMP is always the input jack (p.54)
        float* recL = io.rec[(size_t) in0];
        float* recR = stereo ? io.rec[(size_t) in0 + 1] : nullptr;
        auto tap = [&] (const float* a, const float* b) {
            copyOrClear (recL, a, n);
            if (stereo)
                copyOrClear (recR, b, n);
        };
        if (c.sendPoint == SendPoint::PreComp)
            tap (jackL.data(), jackR.data());

        // COMP (one detector for a stereo pair)
        d.comp.setAmount (c.comp);
        float maxReduction = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float level = stereo ? std::max (std::abs (L[i]), std::abs (R[i])) : std::abs (L[i]);
            const float cg = d.comp.gainFor (level);
            maxReduction = std::max (maxReduction, d.comp.reductionDb());
            L[i] *= cg;
            R[i] *= cg;
        }
        meters.compActive[(size_t) s] = maxReduction > 1.0f;
        if (c.sendPoint == SendPoint::PostComp)
            tap (L, R);

        // EQ: HIGH 10 kHz shelf, MID 100 Hz - 8 kHz, LOW 80 Hz shelf, +-15 dB
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& eq = d.eq[(size_t) ch];
            eq.set (sampleRate, law::eqDb (c.eqLow), 80.0f, law::eqDb (c.eqMid), law::midHz (c.eqMidFreq), 1.0f, law::eqDb (c.eqHigh), 10000.0f);
            if (! eq.flat())
            {
                float* x = ch == 0 ? L : R;
                for (int i = 0; i < n; ++i)
                    x[i] = eq.process (x[i]);
            }
        }
        if (c.sendPoint == SendPoint::PostEq)
            tap (L, R);
        meters.channel[(size_t) s * 2] = peakOf (L, n);
        meters.channel[(size_t) s * 2 + 1] = stereo ? peakOf (R, n) : meters.channel[(size_t) s * 2];

        // PFL: before MUTE and the fader (a muted channel can be cued)
        if (c.solo && ! inPlace)
        {
            const float lg = stereo ? 1.0f : 0.7071f;
            addScaled (soloL.data(), L, lg, n);
            addScaled (soloR.data(), stereo ? R : L, lg, n);
        }

        // MUTE, the sends, the fader, PAN / BAL; in-place solo silences the channels that aren't soloed
        const bool on = ! c.mute;
        float pl = 0, pr = 0;
        if (stereo)
            law::balance (c.pan, pl, pr);
        else
            law::pan (c.pan, pl, pr);
        const bool silenced = inPlace && solos && ! c.solo;
        const RampedGain fg (d.fader, on ? law::faderGain (c.fader) : 0.0f, n);
        const RampedGain a1 (d.aux1, on ? law::sendGain (c.aux1) : 0.0f, n);
        const RampedGain a2 (d.aux2, on ? law::sendGain (c.aux2) : 0.0f, n);
        for (int i = 0; i < n; ++i)
        {
            const float f = fg.at (i);
            const float mono = stereo ? 0.5f * (L[i] + R[i]) : L[i];
            aux1Bus[(size_t) i] += mono * a1.at (i) * (settings.aux1PostFader ? f : 1.0f);
            aux2Bus[(size_t) i] += mono * a2.at (i) * f; // (always post-fader)
            if (silenced)
                continue;
            const float l = L[i] * f * pl, r = (stereo ? R[i] : L[i]) * f * pr;
            if (c.main)
            {
                mainBusL[(size_t) i] += l;
                mainBusR[(size_t) i] += r;
            }
            if (c.sub)
            {
                subBusL[(size_t) i] += l;
                subBusR[(size_t) i] += r;
            }
        }
    }

    // ---------------------------------------------------------------- the effect
    clear (fxL, n);
    clear (fxR, n);
    if (io.fxAvailable && fxs.on)
    {
        effect.setType (fxs.type, fxs.param[(size_t) fxs.type]);
        effect.process (aux2Bus.data(), fxL.data(), fxR.data(), n);
    }
    meters.fx = std::max (peakOf (fxL.data(), n), peakOf (fxR.data(), n));
    {
        const RampedGain ret (fxRamp, m.fxMute ? 0.0f : law::faderGain (m.fxFader), n);
        const float toAux1 = m.fxMute || ! m.fxAux1 ? 0.0f : law::sendGain (m.fxToAux1);
        for (int i = 0; i < n; ++i)
        {
            const size_t k = (size_t) i;
            aux1Bus[k] += 0.5f * (fxL[k] + fxR[k]) * toAux1; // TO AUX 1 has its own knob
            const float gi = ret.at (i);
            const float l = fxL[k] * gi, r = fxR[k] * gi;
            if (m.fxMain)
            {
                mainBusL[k] += l;
                mainBusR[k] += r;
            }
            if (m.fxSub)
            {
                subBusL[k] += l;
                subBusR[k] += r;
            }
            if (m.fxAfl)
            {
                soloL[k] += l;
                soloR[k] += r;
            }
        }
    }

    // the paired device straight into the MAIN MIX
    if (m.assign == Assign::ToMain)
    {
        addScaled (mainBusL.data(), io.btL, 1.0f, n);
        addScaled (mainBusR.data(), io.btR, 1.0f, n);
    }

    // ---------------------------------------------------------------- the click: the CLICK jack, and its level elsewhere
    auto clickGain = [&] (int route) { return metro.routeOn[(size_t) route] ? (float) metro.routeVolume[(size_t) route] / 50.0f : 0.0f; };
    if (io.clickOut != nullptr)
    {
        std::fill (io.clickOut, io.clickOut + n, 0.0f);
        addScaled (io.clickOut, io.click, (float) metro.clickVolume / 50.0f, n);
    }

    // ---------------------------------------------------------------- SUB
    const float subTarget = m.subMute ? 0.0f : law::faderGain (m.subFader);
    {
        const RampedGain sg (subRamp, subTarget, n);
        for (int i = 0; i < n; ++i)
        {
            subBusL[(size_t) i] *= sg.at (i);
            subBusR[(size_t) i] *= sg.at (i);
        }
    }
    addScaled (subBusL.data(), io.click, clickGain (2) * subTarget, n);
    addScaled (subBusR.data(), io.click, clickGain (2) * subTarget, n);
    if (m.subMain) // the SUB output into the MAIN MIX
    {
        addScaled (mainBusL.data(), subBusL.data(), 1.0f, n);
        addScaled (mainBusR.data(), subBusR.data(), 1.0f, n);
    }
    const float subPad = juce::Decibels::decibelsToGain ((float) settings.padSubDb);
    for (int i = 0; i < n; ++i)
    {
        if (io.subL != nullptr)
            io.subL[i] = subBusL[(size_t) i] * subPad;
        if (io.subR != nullptr)
            io.subR[i] = subBusR[(size_t) i] * subPad;
    }

    // ---------------------------------------------------------------- the master EQ on the MAIN MIX
    const bool eqMain = m.eqIn && ! m.eqOnAux, eqAux = m.eqIn && m.eqOnAux;
    for (auto& eq : masterEq)
        eq.set (sampleRate, law::eqDb (m.eqLow), 60.0f, law::eqDb (m.eqMid), law::midHz (m.eqMidFreq), m.eqNarrow ? 3.0f : 0.7f, law::eqDb (m.eqHigh),
                10000.0f);
    if (eqMain && ! masterEq[0].flat())
        for (int i = 0; i < n; ++i)
        {
            mainBusL[(size_t) i] = masterEq[0].process (mainBusL[(size_t) i]);
            mainBusR[(size_t) i] = masterEq[1].process (mainBusR[(size_t) i]);
        }
    meters.mixL = peakOf (mainBusL.data(), n);
    meters.mixR = peakOf (mainBusR.data(), n);

    // ---------------------------------------------------------------- MAIN, tracks 11/12 and the USB mix
    if (! m.postRec)
    {
        copyOrClear (io.rec[(size_t) kMixTrack], mainBusL.data(), n);
        copyOrClear (io.rec[(size_t) kMixTrack + 1], mainBusR.data(), n);
    }
    const float mainTarget = m.mainMute ? 0.0f : law::faderGain (m.mainFader);
    // the MAIN MIX before the fader: for the computer (fxL/R are free now) and the phones' MAIN MIX PRE (with the click)
    std::copy (mainBusL.begin(), mainBusL.begin() + n, fxL.begin());
    std::copy (mainBusR.begin(), mainBusR.begin() + n, fxR.begin());
    std::copy (mainBusL.begin(), mainBusL.begin() + n, preL.begin());
    std::copy (mainBusR.begin(), mainBusR.begin() + n, preR.begin());
    addScaled (preL.data(), io.click, clickGain (3), n);
    addScaled (preR.data(), io.click, clickGain (3), n);
    {
        const RampedGain mg (mainRamp, mainTarget, n);
        for (int i = 0; i < n; ++i)
        {
            mainBusL[(size_t) i] *= mg.at (i);
            mainBusR[(size_t) i] *= mg.at (i);
        }
    }
    if (m.postRec) // POST REC: tracks 11/12 (and the computer's mix) after the MAIN fader
    {
        copyOrClear (io.rec[(size_t) kMixTrack], mainBusL.data(), n);
        copyOrClear (io.rec[(size_t) kMixTrack + 1], mainBusR.data(), n);
    }
    // the MAIN MIX to the computer, delayed for streaming when OUTPUT DELAY is enabled (p.71)
    const int delaySamples = (int) std::lround (settings.outputDelayMs * 0.001 * sampleRate);
    if (io.usbMixL != nullptr && io.usbMixR != nullptr)
    {
        std::copy_n (m.postRec ? mainBusL.data() : fxL.data(), n, io.usbMixL);
        std::copy_n (m.postRec ? mainBusR.data() : fxR.data(), n, io.usbMixR);
        if (settings.outputDelayOn)
            usbDelay.process (io.usbMixL, io.usbMixR, n, delaySamples);
    }
    if (m.sdReturn) // the stereo master on the card instead of the MAIN MIX (p.23)
        for (int i = 0; i < n; ++i)
        {
            mainBusL[(size_t) i] = (io.mtr[(size_t) kMixTrack] != nullptr ? io.mtr[(size_t) kMixTrack][i] : 0.0f) * mainTarget;
            mainBusR[(size_t) i] = (io.mtr[(size_t) kMixTrack + 1] != nullptr ? io.mtr[(size_t) kMixTrack + 1][i] : 0.0f) * mainTarget;
        }
    addScaled (mainBusL.data(), io.click, clickGain (3) * mainTarget, n);
    addScaled (mainBusR.data(), io.click, clickGain (3) * mainTarget, n);
    if (settings.outputDelayOn && settings.outputDelayOnMain)
        mainDelay.process (mainBusL.data(), mainBusR.data(), n, delaySamples);
    const float mainPad = juce::Decibels::decibelsToGain ((float) settings.padMainDb);
    for (int i = 0; i < n; ++i)
    {
        if (io.mainL != nullptr)
            io.mainL[i] = mainBusL[(size_t) i] * mainPad;
        if (io.mainR != nullptr)
            io.mainR[i] = mainBusR[(size_t) i] * mainPad;
    }

    // ---------------------------------------------------------------- AUX 1 / AUX 2 OUT
    addScaled (aux1Bus.data(), io.click, clickGain (4), n);
    addScaled (aux2Bus.data(), io.click, clickGain (5), n);
    const float a1Out = m.aux1Mute ? 0.0f : law::levelGain (m.aux1Out);
    const float a2Out = m.aux2Mute ? 0.0f : law::levelGain (m.aux2Out);
    for (int i = 0; i < n; ++i)
    {
        float x1 = aux1Bus[(size_t) i] * a1Out, x2 = aux2Bus[(size_t) i] * a2Out;
        if (eqAux)
        {
            x1 = masterEq[2].process (x1);
            x2 = masterEq[3].process (x2);
        }
        aux1Out[(size_t) i] = x1;
        aux2Out[(size_t) i] = x2;
        if (io.aux1 != nullptr)
            io.aux1[i] = x1;
        if (io.aux2 != nullptr)
            io.aux2[i] = x2;
    }
    meters.aux1 = peakOf (aux1Out.data(), n);
    meters.aux2 = peakOf (aux2Out.data(), n);
    if (m.aux1Afl) // AFL: the AUX outputs onto the SOLO bus
    {
        addScaled (soloL.data(), aux1Out.data(), 1.0f, n);
        addScaled (soloR.data(), aux1Out.data(), 1.0f, n);
    }
    if (m.aux2Afl)
    {
        addScaled (soloL.data(), aux2Out.data(), 1.0f, n);
        addScaled (soloR.data(), aux2Out.data(), 1.0f, n);
    }

    // ---------------------------------------------------------------- SOLO, the phones, the meters
    const float soloGain = law::levelGain (m.pflAfl);
    for (int i = 0; i < n; ++i)
    {
        soloL[(size_t) i] *= soloGain;
        soloR[(size_t) i] *= soloGain;
    }
    const bool soloBus = soloBusActive (strips, m, settings);
    auto phones = [&] (float* outL, float* outR, bool auxMode, float level, int clickRoute) {
        if (outL == nullptr || outR == nullptr)
            return;
        const float g = law::levelGain (level);
        for (int i = 0; i < n; ++i)
        {
            const size_t k = (size_t) i;
            float l, r;
            if (soloBus) // anything soloed: both phones hear the SOLO bus
            {
                l = soloL[k];
                r = soloR[k];
            }
            else if (auxMode) // AUX 1 on the left, AUX 2 on the right
            {
                l = aux1Out[k];
                r = aux2Out[k];
            }
            else if (settings.phonesMainPost)
            {
                l = mainBusL[k];
                r = mainBusR[k];
            }
            else
            {
                l = preL[k];
                r = preR[k];
            }
            outL[i] = l * g;
            outR[i] = r * g;
        }
        addScaled (outL, io.click, clickGain (clickRoute) * g, n);
        addScaled (outR, io.click, clickGain (clickRoute) * g, n);
    };
    phones (io.phones1L, io.phones1R, m.phones1Aux, m.phones1, 0);
    phones (io.phones2L, io.phones2R, m.phones2Aux, m.phones2, 1);
    meters.mainL = soloBus ? peakOf (soloL.data(), n) : peakOf (mainBusL.data(), n) * mainPad;
    meters.mainR = soloBus ? peakOf (soloR.data(), n) : peakOf (mainBusR.data(), n) * mainPad;
}
} // namespace trackdesk
