// Trackdesk's side of the GPUI bridge: its panel's parts, and what makes each look different.

#include "PanelBridge.h"
#include "../src/engine/Engine.h"
#include "../src/ui/MainComponent.h"

namespace trackdesk::ui
{
using panelbridge::mix;
using panelbridge::mixFloat;

struct GpuiAdapter : panelbridge::Adapter
{
    std::unique_ptr<Engine> engine = std::make_unique<Engine>();
    std::unique_ptr<MainComponent> mc = std::make_unique<MainComponent> (*engine);
    juce::File tone;

    GpuiAdapter() { mc->setSize (Panel::kWidth, Panel::kHeight); } // (the panel at 1:1 inside the window component)
    ~GpuiAdapter() override
    {
        Panel::keyHook = nullptr;
        mc.reset();
        engine.reset();
        tone.deleteFile();
    }

    juce::String title() override { return "Trackdesk TD-12"; }
    juce::Component& panel() override { return *mc; }
    juce::Component& keyTarget() override { return mc->panel; }
    void prepareToQuit() override { mc->prepareToQuit(); }
    void setKeyHook (std::function<bool (int)> f) override { Panel::keyHook = std::move (f); }
    float outputLevel() override { return engine->getOutputLevel(); }

    void testSound (bool start) override
    {
        // a tone through the paired device, assigned to MAIN (as the app's self-test does)
        if (start)
        {
            tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("trackdesk-gpui-tone.wav");
            tone.deleteFile();
            juce::WavAudioFormat wav;
            std::unique_ptr<juce::OutputStream> os (tone.createOutputStream().release());
            auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (16));
            juce::AudioBuffer<float> b (2, 48000 * 3);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (0, i, 0.3f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f)), b.setSample (1, i, b.getSample (0, i));
            if (w != nullptr)
                w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
            w.reset();
            engine->pairWithFile (tone);
        }
        engine->withDesk ([start] (Desk& d) {
            d.setAssign (start ? Assign::ToMain : Assign::Off);
            if (start)
            {
                d.setMasterKnob (MasterKnob::MainFader, law::kFaderUnity);
                d.setMasterSwitch (MasterSwitch::MainMute, false);
            }
        });
    }

    std::vector<std::pair<juce::String, juce::Component*>> parts() override
    {
        std::vector<std::pair<juce::String, juce::Component*>> p;
        for (auto& part : mc->panel.parts)
        {
            auto* c = part.get();
            juce::String kind = "static";
            if (dynamic_cast<PushKey*> (c) != nullptr)
                kind = "key";
            else if (dynamic_cast<Knob*> (c) != nullptr || dynamic_cast<Fader*> (c) != nullptr || dynamic_cast<JogDial*> (c) != nullptr)
                kind = "drag";
            else if (dynamic_cast<LcdView*> (c) != nullptr || dynamic_cast<MeterView*> (c) != nullptr)
                kind = "display";
            else if (dynamic_cast<Slide3*> (c) != nullptr)
                kind = "slide";
            p.push_back ({ kind, c });
        }
        // (the app's own buttons sit on the panel too)
        for (auto* c : mc->panel.getChildren())
            if (dynamic_cast<juce::TextButton*> (c) != nullptr)
                p.push_back ({ "button", c });
        return p;
    }

    uint64_t custom (juce::Component& c) override
    {
        uint64_t h = 11;
        if (auto* k = dynamic_cast<Knob*> (&c))
            return mixFloat (h, k->shown);
        if (auto* f = dynamic_cast<Fader*> (&c))
            return mixFloat (h, f->shown);
        if (auto* k = dynamic_cast<PushKey*> (&c))
            return mix (mix (mix (h, k->held), k->shownLatched), (uint64_t) (k->shownLamp + 2));
        if (auto* l = dynamic_cast<Led*> (&c))
            return mix (h, l->shown.getARGB());
        if (auto* s = dynamic_cast<Slide3*> (&c))
            return mix (h, (uint64_t) (s->shown + 2));
        if (auto* l = dynamic_cast<LcdView*> (&c))
        {
            h = mixFloat (mix (h, l->dim), l->contrast);
            for (auto v : l->last)
                h = mix (h, v);
            return h;
        }
        if (auto* m = dynamic_cast<MeterView*> (&c))
            return mixFloat (mixFloat (mix (mix (h, (uint64_t) (m->lit[0] + 2)), (uint64_t) (m->lit[1] + 2)), m->held[0], 0.01f), m->held[1], 0.01f);
        if (auto* j = dynamic_cast<JogDial*> (&c))
            return mix (mixFloat (h, j->angle), j->pressed);
        return 0;
    }
};
} // namespace trackdesk::ui

std::unique_ptr<panelbridge::Adapter> panelbridge::makeAdapter() { return std::make_unique<trackdesk::ui::GpuiAdapter>(); }
