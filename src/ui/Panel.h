#pragma once

// The console's top panel on a fixed 1500 x 1580 canvas, laid out on the original's measured drawing (343 x 360 mm
// with its wooden cheeks, about 4.27 px a millimetre). Its owner scales it to the window.
//
// The panel reads the console once a frame (a snapshot under the engine's lock) and writes to it when a control
// moves, so the audio thread rarely waits.

#include "../engine/Engine.h"
#include "Widgets.h"

namespace trackdesk::ui
{
class Panel : public juce::Component, private juce::Timer
{
    friend struct GpuiAdapter; // (the GPUI front end drives this panel off-screen: bridge/TrackdeskAdapter.cpp)

public:
    /** Whether a key is held (the GPUI front end answers this; otherwise the computer's keyboard). */
    static inline std::function<bool (int)> keyHook;
    static bool keyIsDown (int c) { return keyHook ? keyHook (c) : juce::KeyPress::isKeyCurrentlyDown (c); }

    static constexpr int kWidth = 1500, kHeight = 1580;
    explicit Panel (Engine& engine);
    ~Panel() override;

    void paint (juce::Graphics&) override;
    bool keyPressed (const juce::KeyPress&) override;
    bool keyStateChanged (bool isKeyDown) override;
    /** Reads the console now (the timer does it 30 times a second). */
    void refreshNow();
    /** The self-test: works every control through its widget and checks the console follows (passes, failures). */
    std::pair<juce::StringArray, juce::StringArray> selfTestControls();
    /** The rectangle (canvas) left free at the top left of the panel for the app's own buttons. */
    static juce::Rectangle<int> utilityArea() { return { 96, 44, 520, 70 }; }

private:
    struct Snapshot
    {
        std::array<Strip, kStrips> strips;
        Master master;
        std::array<bool, kStrips> rec {};
        Desk::Lights lights;
        Mixer::Meters meters;
        float contrast = 0.6f;
        bool dim = false;
    };

    void timerCallback() override;
    void build();
    void paintBackground (juce::Graphics& g);
    void pressKey (Key k);
    void releaseKey (Key k);
    template <typename Fn>
    void edit (Fn&& fn)
    {
        engine.withDesk ([&] (Desk& d) {
            fn (d);
            snap.strips = d.strips;
            snap.master = d.master;
            snap.rec = d.recKey;
        });
    }
    Knob& addKnob (juce::Point<float> centre, float diameter, juce::Colour cap, float dflt, std::function<float()> get, std::function<void (float)> set);
    PushKey& addKey (juce::Point<float> centre, juce::Point<float> size, PushKey::Style style, const juce::String& glyph = {});
    Led& addLed (juce::Point<float> centre, float diameter, juce::Colour base, std::function<juce::Colour()> colour);
    Fader& addFader (float x, juce::Colour cap, std::function<float()> get, std::function<void (float)> set);

    Engine& engine;
    Snapshot snap;
    juce::Image background;
    float backgroundScale = 0.0f;
    std::vector<std::unique_ptr<juce::Component>> parts;
    std::vector<Knob*> knobs;
    std::vector<PushKey*> keys;
    std::vector<Led*> leds;
    std::vector<Fader*> faders;
    std::vector<Slide3*> slides;
    std::vector<std::pair<PushKey*, Key>> momentaries;
    LcdView* lcd = nullptr;
    MeterView* meter = nullptr;
    JogDial* jog = nullptr;
    bool footHeld = false;
};

/** Where the channel strips sit: the centre x of strip i on the canvas. */
float stripX (int strip);
} // namespace trackdesk::ui
