#pragma once

// The mixer's signal flow, as the manual and its block diagram describe it:
//   input:   jack -> GAIN (the analog preamp) -> LOW CUT -> PHASE                     (SIG light, PRE COMP send)
//   channel: MODE (LIVE: the input, PC: the computer, MTR: the input and/or the track) -> COMP -> EQ
//            -> [PFL] -> MUTE -> [AUX 1 pre] -> fader -> PAN/BAL -> MAIN / SUB ; AUX 1 post, AUX 2/FX post-fader
//   effect:  AUX 2/FX bus -> effect -> FX fader -> MAIN / SUB / SOLO (AFL) ; TO AUX 1 -> AUX 1
//   master:  SUB -> SUB OUT (and with SUB > MAIN into the MAIN MIX) ; MAIN MIX -> master EQ -> (tracks 11/12)
//            -> MAIN fader -> (SD MAIN MIX RETURN) -> output delay -> pad -> MAIN OUT ;
//            AUX 1/2 -> level, mute, master EQ -> AUX OUT 1/2 ; SOLO bus (PFL, AFL) -> PFL/AFL MASTER -> phones, meters
// The click goes to the CLICK jack, the phones and the buses after the recording taps: it's never recorded (p.51).

#include "Dsp.h"

namespace trackdesk
{
class Mixer
{
public:
    /** What a strip hears (the MODE switch and, in MTR mode, the recorder's state: p.39). */
    enum class Source : uint8_t
    {
        Input,
        Playback,
        InputAndPlayback,
        Muted
    };

    struct IO
    {
        std::array<const float*, kInputs> live {}; // the input jacks
        std::array<const float*, kInputs> pc {};   // the computer (USB)
        std::array<const float*, kTracks> mtr {};  // the recorder's playback
        const float* btL = nullptr;                // the paired (Bluetooth) device
        const float* btR = nullptr;
        const float* click = nullptr;              // the metronome
        float *mainL = nullptr, *mainR = nullptr, *subL = nullptr, *subR = nullptr, *aux1 = nullptr, *aux2 = nullptr;
        float *phones1L = nullptr, *phones1R = nullptr, *phones2L = nullptr, *phones2R = nullptr, *clickOut = nullptr;
        std::array<float*, kTracks> rec {};          // what each track records (and the USB sends 1-12)
        float *usbMixL = nullptr, *usbMixR = nullptr; // the MAIN MIX to the computer (after the OUTPUT DELAY)
        std::array<Source, kStrips> source {};
        bool fxAvailable = true; // (not when the AUX 2 jack is in use)
    };

    /** Peak levels over a block (0..1+). */
    struct Meters
    {
        std::array<float, kStrips * 2> channel {}; // L, R per strip: what it hears (post-EQ, pre-fader)
        std::array<float, kInputs> live {}, pc {}; // the inputs (after GAIN) and the computer
        std::array<float, kTracks> mtr {};         // the tracks' playback
        std::array<bool, kStrips> sigGreen {}, sigRed {}, compActive {};
        float mainL = 0, mainR = 0;   // the MAIN meters: MAIN OUT, or the SOLO bus while PFL/AFL is lit
        float mixL = 0, mixR = 0;     // the MAIN MIX bus
        float aux1 = 0, aux2 = 0, fx = 0;
        void mergeFrom (const Meters& o);
    };

    void prepare (double sampleRate, int maxBlock);
    void process (int n, const IO& io, const std::array<Strip, kStrips>& strips, const Master& master, const Settings& settings,
                  const FxSettings& fx, const Metronome& metronome, Meters& meters);
    /** The SOLO bus is what the phones and meters hear: a PFL solo or an AFL is on. */
    static bool soloBusActive (const std::array<Strip, kStrips>& strips, const Master& master, const Settings& settings);
    static bool anySolo (const std::array<Strip, kStrips>& strips);
    void resetEffect() { effect.reset(); }

private:
    struct StripDsp
    {
        std::array<dsp::Biquad, 2> lowCut;
        std::array<dsp::ThreeBand, 2> eq;
        dsp::Compressor comp;
        dsp::Ramp fader, aux1, aux2;
    };
    struct DelayLine
    {
        std::vector<float> l, r;
        int pos = 0;
        void prepare (int size);
        void process (float* L, float* R, int n, int delay);
    };
    double sampleRate = 48000.0;
    int maxBlock = 0;
    std::array<StripDsp, kStrips> strip;
    dsp::Effect effect;
    std::array<dsp::ThreeBand, 4> masterEq; // MAIN L, R, AUX 1, AUX 2
    dsp::Ramp mainRamp, subRamp, fxRamp;
    std::vector<float> jackL, jackR, pcL, pcR, pbL, pbR, bufL, bufR, preL, preR;
    std::vector<float> mainBusL, mainBusR, subBusL, subBusR, soloL, soloR, aux1Bus, aux2Bus, fxL, fxR, aux1Out, aux2Out;
    DelayLine mainDelay, usbDelay;
};
} // namespace trackdesk
