# Research: the 12-track recording console (TASCAM Model 12)

This collects how the TASCAM Model 12 works, as the reference for **Trackdesk TD-12**, an original desktop mixer and recorder built for research. It reproduces the Model 12's architecture, controls and behaviour. Trackdesk doesn't use TASCAM's names, logos or artwork. "TASCAM" and "Model 12" are trademarks of TEAC Corporation and appear here only to identify the console being studied.

The source is the Model 12 Owner's Manual (D01341220C, 2020–2023 editions, 212 pages; page numbers below are the English section's). The look comes from TASCAM's product photographs and the manual's panel drawings.

## 1. What it is

- A **10-input analog-style digital mixer** with a 12-track SD recorder, a USB audio interface and a DAW controller (p.17).
- **8 channel strips:**
  - channels **1–6** are mono;
  - **7/8** and **9/10** are stereo.
  - Each channel input is a MIC/LINE/INST XLR/TRS combo jack. The stereo channels have a second R jack.
- **Buses:** MAIN MIX L/R, SUB L/R, AUX 1, AUX 2/FX and SOLO L/R (p.20).
- **Processing:**
  - a one-knob digital compressor on every channel;
  - a 3-band EQ with a semi-parametric mid;
  - 16 built-in effects;
  - a 3-band master EQ for the outputs.
- **Recorder:** 12 tracks (channels 1–10 plus the MAIN MIX L/R bus as tracks 11/12), WAV 44.1/48 kHz, 16/24-bit (p.76).
- **Size and weight:** 343 × 98.8 × 360 mm with its wooden side panels (315 mm without), 4.3 kg (p.78).

## 2. The look

- **Panel:**
  - matt black, framed by **walnut side cheeks**;
  - a light grey band across the top for the LOW CUT/INST switches;
  - a black band holding the MODE switches;
  - the fader area is a **slate blue-grey**;
  - the master section (FX, SUB, MAIN faders) is **gold**.
- **Knob caps by function** (black skirts with a white pointer):
  - **red** GAIN;
  - **green** COMP;
  - **blue** EQ (HIGH, MID frequency, MID gain, LOW) and the master EQ;
  - **orange** AUX 1 and AUX 2/FX;
  - **white** PAN and the monitoring knobs.
- **Faders:** 60 mm long, with white caps on the channels. The FX fader has an **orange** cap, SUB **white**, MAIN **red**.
- **Switches:** light grey square push switches that latch, beside small round LEDs. The REC keys sit above the faders. MUTE, MAIN, SUB and SOLO are beside each fader.
- **Display and meters:**
  - a monochrome LCD with four function buttons under it;
  - a MULTI JOG dial, MENU, and the transport keys (◀◀ ▶▶ ■ ▶/II ●);
  - a two-column MAIN level meter (OL, 15, 10, 6, 3, 0, −2, −4, −7, −10, −20, −30) beside the display.
- **Other panels:** a blue Bluetooth panel (PAIRING, ASSIGN 9/10–OFF–MAIN) and a CLICK block (TEMPO light, TAP/START/STOP).
- **Rear panel:**
  - MAIN OUTPUT L/R (XLR), SUB OUTPUT L/R, AUX OUTPUT 1/2, CLICK, FOOTSWITCH, MIDI OUT/IN;
  - INSERT 1–2, the combo inputs 1–6, 7/8 and 9/10 with their R jacks, MUSIC/TALK, USB-C, DC IN and STANDBY.

## 3. The channel strip (p.20–21)

| Control | Function |
|---|---|
| GAIN + SIG | Input level. MIC 0 to +50 dB; LINE −10 to +40 dB on 1–6 and −20 to +30 dB on 7–10 (p.76). SIG lights green at −40 dB and red near clipping |
| LOW CUT | Low cut filter (100 Hz) |
| INST | High-impedance input for guitars (L jack only on stereo channels) |
| MODE | Input source: **LIVE** (input jack), **PC** (computer over USB), **MTR** (the recorder's track) (p.39) |
| COMP + light | One-knob compressor: turning it up lowers the threshold. The light shows gain reduction |
| EQ | HIGH (10 kHz shelf), MID gain with frequency 100 Hz–8 kHz (default 600 Hz), LOW (80 Hz shelf), each ±15 dB |
| AUX 1 | Send to AUX 1. Pre-fader by default; the MIXER > AUX1 SEND menu makes it post-fader, and a POST light shows it (p.55) |
| AUX 2/FX | Send to the AUX 2/FX bus (post-fader); feeds the built-in effect |
| PAN / BAL | Pan; centre is −3 dB to each side (p.21). Balance on the stereo channels |
| REC | Arms the channel's track (blinks when armed, lights while recording) |
| MUTE + light | Mutes the channel |
| Fader | Channel level (+10 at the top, then 5, 0, 5, 10, 15, 20, 30, 40, ∞) |
| MAIN / SUB / SOLO | Send to the MAIN MIX bus, the SUB bus, the SOLO bus |

**MTR mode monitoring (p.39):**

| Transport | REC off | REC on |
|---|---|---|
| Stopped | muted | input |
| Playing | playback | playback + input |
| Recording | playback | input |

## 4. The master section (p.22–24)

- **Master EQ:** HIGH (10 kHz), MID (100 Hz–8 kHz) with a narrow/wide switch, LOW (60 Hz), ±15 dB.
  - An AUX 1/2 / MAIN MIX switch picks which outputs it acts on.
  - EQ IN/BYPASS switches it in or out.
- **PHONES 1, 2:** each has a level knob and an AUX 1/2 / MAIN switch.
  - With the switch in, that phones output hears AUX 1 (left) and AUX 2 (right).
  - Whenever anything is soloed, both phones outputs hear the SOLO bus.
  - MIXER > PHONES MAIN SEL picks pre or post the MAIN fader.
- **PHANTOM +48V:** for all mic inputs, switchable only while the recorder is stopped, after a confirmation on the display (p.39).
- **AUX 1 / AUX 2 outputs:** a level knob, MUTE and AFL (to the SOLO bus) for each. When the AUX 2 jack is in use, the built-in effect is unavailable (p.25).
- **PFL/AFL MASTER:** the SOLO bus level.
- **SD MAIN MIX RETURN:** the MAIN outputs play the stereo master tracks 11/12 instead of the MAIN MIX.
- **Bluetooth:** PAIRING. ASSIGN sends the paired device to channel 9/10, off, or straight to the MAIN MIX.
- **CLICK:** TAP/START/STOP and the TEMPO light (red on the first beat, green on the others).
- **POST REC:** records tracks 11/12 (and sends the USB mix) after the MAIN fader instead of before.
- **FX section:**
  - SELECT opens the EFFECT screen;
  - the TO AUX 1 knob sets the effect's return to AUX 1;
  - the FX fader sets the return level;
  - switches: MUTE, AUX 1, MAIN, SUB, AFL.
  - The effect's return goes to the MAIN MIX, SUB, SOLO and AUX 1 buses.
- **SUB:** fader, MUTE, and MAIN (sends the SUB output into the MAIN MIX).
- **MAIN:** fader and MUTE.

**The 16 effects (p.41):**
1. HALL 1
2. HALL 2
3. ROOM 1
4. ROOM 2
5. PLATE
6. STUDIO
7. LIVE
8. DELAY 1
9. DELAY 2
10. PING PONG
11. CHORUS
12. FLANGER
13. DELAY+HALL 1
14. DELAY+HALL 2
15. CHORUS+HALL 1
16. CHORUS+HALL 2

Each has one parameter. The delays' TIME can be tapped with the CLICK button. F2 turns the effect on or off, and a library holds 10 settings.

## 5. The recorder (p.39–49)

- **Songs:** 44.1/48 kHz, 16/24-bit. You can create, load, save, delete, protect, unprotect and rename them, and clear all their marks (p.35–37).
- **Recording:**
  - REC keys arm tracks 1–10. Tracks 11/12 (the MAIN MIX) always record when you press ● (p.40).
  - ● during playback punches in; ■ punches out.
  - With no track armed, ● records only the mix: a mixdown (p.47).
- **Auto punch in/out:** points at least 1 s apart, a pre-roll of OFF or 1–10 s (default 2 s), rehearsal with ▶ and recording with ●. Playback stops 2 s after the out point. Recording isn't possible with eight or more tracks armed (p.44–45).
- **Locating:** ◀◀ / ▶▶ held searches. Pressed, they go to the start, the end, the punch points and the marks. ■ + ◀◀ / ▶▶ go to where recording last started or stopped. MULTI JOG moves the position, and pressed it edits the counter directly (p.23, p.43).
- **Repeat:** off, play then stop, or loop (F2).
- **Undo/redo:** the last recording, punch or track clear (F3).
- **Marks:** F4 adds one while playing or recording; ◀◀ / ▶▶ move between them (p.49).
- **Track edit:** clear one track or all, import a WAV into an empty track, swap tracks (including 7/8, 9/10, 11/12 as pairs), and export the stereo mix (tracks 11/12 to one stereo WAV) (p.46–48).
- **VAMP:** up to 10 loop ranges (in/out at least 2 s apart), played automatically or manually (p.57–59).
- **SD PLAY:** plays WAV files from the card's MUSIC folder.

## 6. Other functions (p.50–60)

- **Metronome:**
  - TEMPO 20–250 BPM (tap with CLICK), SIGNATURE 1/1–12/8;
  - SOUND: CLICK, STICK, BELL, KICK, SNARE, PEDAL HIHAT, OPEN HIHAT, CLOSE HIHAT;
  - ACCENT: OFF, LOW, MID, HIGH;
  - CLICK OUT mode: OFF, COUNT IN+REC, REC, REC&PLAY, with VOLUME 0–50;
  - click to PHONES 1/2 and to the SUB, MAIN, AUX 1 and AUX 2 buses, each with its own volume;
  - COUNT IN: 1, 2 or 4 bars of the signature, 2+4 BEAT, 2, 3 or 4 BEAT.
- **MIDI:** MIDI time code (30 fps non-drop) and MIDI clock with song position pointers, sent while playing or recording.
- **Mixer menu:**
  - MTR/USB SEND POINT per channel: PRE COMP, POST COMP or POST EQ;
  - PHASE per channel;
  - AUX1 SEND: pre or post fader;
  - SOLO MODE: PFL, or INPLACE SOLO (mutes the channels that aren't soloed);
  - PHONES MAIN SEL: pre or post.
- **Footswitch:** OFF, PLAY/PAUSE, PUNCH I/O, FX MUTE, REC/STOP, MARKER, MARK SKIP, SONG INCREMENT, TAP, VAMP, VAMP I/O SET, with polarity, for two switches.
- **Output settings:** OUTPUT PAD for MAIN and SUB, 0 to −40 dB. OUTPUT DELAY 0–2000 ms (for streaming), optionally on the MAIN outputs too.
- **System menu:** KEY FUNCTION (F4: MARK or CLICK), DISPLAY (contrast, brightness), SONG NAME format, INITIALIZE, MEDIA FORMAT.

## 7. Decisions for the reproduction

1. **Headless console** (core/Desk): every knob, fader, switch, key, menu and screen, with LEDs and the display as outputs. Tests play it like a person would.
2. **Signal flow as documented:** channel → GAIN → LOW CUT → PHASE → COMP → EQ → MUTE → fader → PAN → MAIN/SUB, with the AUX 1 pre/post send, the post-fader AUX 2/FX send and the PFL/in-place SOLO. The effect returns to MAIN/SUB/SOLO/AUX 1, then come the master EQ, the outputs and the phones logic.
3. **The SD card becomes a folder:** songs are folders of WAV takes. A track is a list of recorded passes, later passes playing over earlier ones, which is what makes punch-in, undo and swap work.
4. **USB:**
   - Standalone, LIVE channels hear the audio device's inputs 1–10 and PC channels hear inputs 11–20, for example a loopback device carrying your computer's audio.
   - In Tabletop, LIVE is the input jacks and PC channels 1–2 hear the computer (the manual: "output from the computer is sent to channels 1–2").
5. **Bluetooth becomes an audio file:** PAIRING opens an audio file, which plays as if from a phone, into 9/10 or the MAIN MIX as ASSIGN says.
6. **The footswitch is a computer key** (\\), with all its functions.
7. **Not reproduced:**
   - DAW control (HUI/MCU), the MUSIC/TALK jack and the inserts on channels 1–2;
   - phantom power and INST have their switches, lights and confirmation but no audible effect on line sources;
   - date/time and USB audio modes are shown but don't apply.

## Sources

- TASCAM Model 12 Owner's Manual, [Full Compass copy](https://www.fullcompass.com/common/files/87272-Model12OwnersManual.pdf); also on [ManualsLib](https://www.manualslib.com/manual/1797080/Tascam-12.html).
- [TASCAM Model 12 product page](https://tascam.com/us/product/model_12/top) and its photographs.
