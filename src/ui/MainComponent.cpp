#include "MainComponent.h"

namespace trackdesk::ui
{
namespace
{
    bool testing() { return std::getenv ("TRACKDESK_SELFTEST") != nullptr || std::getenv ("TRACKDESK_SNAPSHOT") != nullptr; }

    struct SmallButtonLook : juce::LookAndFeel_V4
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
        {
            auto r = b.getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colour (0xff26272b).brighter (down ? 0.25f : over ? 0.12f : 0.0f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
        }
        juce::Font getTextButtonFont (juce::TextButton&, int h) override { return panelFont ((float) h * 0.42f); }
    };
    SmallButtonLook& buttonLook()
    {
        static SmallButtonLook l;
        return l;
    }
} // namespace

MainComponent::MainComponent (Engine& e) : engine (e), panel (e)
{
    addAndMakeVisible (panel);
    for (auto* b : { &audioBtn, &helpBtn })
    {
        b->setLookAndFeel (&buttonLook());
        b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd8d8d4));
        b->setWantsKeyboardFocus (false);
        panel.addAndMakeVisible (b); // (on the panel's empty top left, scaling with it)
    }
    const auto u = Panel::utilityArea();
    audioBtn.setBounds (u.getX(), u.getY() + 14, 150, 40);
    helpBtn.setBounds (u.getX() + 160, u.getY() + 14, 90, 40);
    audioBtn.onClick = [this] { showAudioSettings(); };
    helpBtn.onClick = [this] { showHelp(); };
    engine.onMessage = [this] (const juce::String& s) {
        message = s;
        messageUntil = juce::Time::getMillisecondCounter() + 3000;
        repaint();
    };
    engine.onPairingRequest = [this] {
        chooser = std::make_unique<juce::FileChooser> ("Pair with an audio file (it plays like a phone)",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
            if (fc.getResult().existsAsFile())
                engine.pairWithFile (fc.getResult());
        });
    };
    loadSettings();
    setSize (900, 948);
    if (auto* forced = std::getenv ("TRACKDESK_WINDOW")) // testing aid: a window size ("1000x1050")
    {
        const auto wh = juce::StringArray::fromTokens (forced, "x", {});
        if (wh.size() == 2)
            setSize (wh[0].getIntValue(), wh[1].getIntValue());
    }
    startTimerHz (4);
    if (auto* snap = std::getenv ("TRACKDESK_SNAPSHOT")) // testing aid: the panel at 1x to a PNG, then quit
    {
        const juce::File f (snap);
        const float scale = std::getenv ("TRACKDESK_SNAPSHOT_SCALE") != nullptr ? (float) std::atof (std::getenv ("TRACKDESK_SNAPSHOT_SCALE")) : 1.0f;
        if (std::getenv ("TRACKDESK_SNAPSHOT_HOME") != nullptr) // (the README's picture: the Home Screen, a mix set up)
            engine.withDesk ([] (Desk& d) {
                d.press (Key::Menu);
                d.release (Key::Menu);
                const float gains[] = { 0.42f, 0.35f, 0.5f, 0.3f, 0.28f, 0.45f, 0.4f, 0.38f };
                const float faders[] = { 0.70f, 0.66f, 0.73f, 0.62f, 0.58f, 0.68f, 0.72f, 0.64f };
                const float pans[] = { 0.5f, 0.3f, 0.7f, 0.5f, 0.2f, 0.8f, 0.5f, 0.5f };
                for (int s = 0; s < kStrips; ++s)
                {
                    d.setKnob (s, StripKnob::Gain, gains[s]);
                    d.setKnob (s, StripKnob::Fader, faders[s]);
                    d.setKnob (s, StripKnob::Pan, pans[s]);
                    d.setKnob (s, StripKnob::Comp, s % 3 == 0 ? 0.35f : 0.0f);
                    d.setKnob (s, StripKnob::Aux2, s < 4 ? 0.45f : 0.2f);
                }
                d.setSwitch (0, StripSwitch::Rec, true);
                d.setSwitch (1, StripSwitch::Rec, true);
                d.setSwitch (4, StripSwitch::Mute, true);
                d.setMode (6, Mode::Mtr);
                d.setMode (7, Mode::Pc);
                d.setMasterKnob (MasterKnob::MainFader, law::kFaderUnity);
                d.setMasterKnob (MasterKnob::SubFader, 0.6f);
                d.setMasterKnob (MasterKnob::Phones1, 0.62f);
            });
        juce::Timer::callAfterDelay (1200, [safe = juce::Component::SafePointer<MainComponent> (this), f, scale] {
            if (safe == nullptr)
                return;
            safe->panel.refreshNow();
            const auto img = safe->panel.createComponentSnapshot (safe->panel.getLocalBounds(), true, scale);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            os.flush();
            juce::JUCEApplicationBase::quit();
        });
    }
    if (auto* report = std::getenv ("TRACKDESK_SELFTEST"))
    {
        const juce::File f (report);
        juce::Timer::callAfterDelay (1500, [safe = juce::Component::SafePointer<MainComponent> (this), f] {
            if (safe != nullptr)
                safe->runSelfTest (f);
        });
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    engine.onMessage = nullptr;
    engine.onPairingRequest = nullptr;
    audioBtn.setLookAndFeel (nullptr);
    helpBtn.setLookAndFeel (nullptr);
}

void MainComponent::prepareToQuit() { saveSettings(); }

void MainComponent::resized()
{
    // the panel keeps its proportions, centred in the window
    const float sc = std::min (getWidth() / (float) Panel::kWidth, getHeight() / (float) Panel::kHeight);
    const float ox = (getWidth() - Panel::kWidth * sc) * 0.5f, oy = (getHeight() - Panel::kHeight * sc) * 0.5f;
    panel.setTransform (juce::AffineTransform::scale (sc).translated (ox, oy));
}

void MainComponent::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff0d0d0e)); }

void MainComponent::paintOverChildren (juce::Graphics& g)
{
    if (message.isEmpty() || juce::Time::getMillisecondCounter() > messageUntil)
        return;
    auto r = getLocalBounds().removeFromBottom (36).reduced (getWidth() / 4, 4).toFloat();
    g.setColour (juce::Colour (0xe0202125));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (juce::Colours::white);
    g.setFont (panelFont (14.0f));
    g.drawText (message, r, juce::Justification::centred);
}

void MainComponent::mouseDown (const juce::MouseEvent&) { panel.grabKeyboardFocus(); }

void MainComponent::timerCallback()
{
    if (message.isNotEmpty() && juce::Time::getMillisecondCounter() > messageUntil)
    {
        message = {};
        repaint();
    }
}

void MainComponent::showAudioSettings()
{
    auto* content = new juce::Component();
    auto* selector = new juce::AudioDeviceSelectorComponent (engine.getDeviceManager(), 0, 20, 2, 23, false, false, true, false);
    selector->setSize (580, 470);
    auto* outLabel = new juce::Label ({}, "MIDI out:");
    auto* outBox = new juce::ComboBox();
    outBox->addItem ("(the Trackdesk port only)", 1);
    const auto outs = engine.getMidiOutputs();
    for (int i = 0; i < outs.size(); ++i)
    {
        outBox->addItem (outs[i].name, i + 2);
        if (outs[i].identifier == engine.getMidiOutputIdentifier())
            outBox->setSelectedId (i + 2, juce::dontSendNotification);
    }
    if (outBox->getSelectedId() == 0)
        outBox->setSelectedId (1, juce::dontSendNotification);
    outBox->onChange = [this, outBox, outs] {
        const int id = outBox->getSelectedId();
        engine.setMidiOutput (id >= 2 ? outs[id - 2].identifier : juce::String());
    };
    auto* phones = new juce::ToggleButton ("Outputs 1-2 play PHONES 1 (for a computer's own two outputs)");
    phones->setToggleState (engine.getPhonesOnFirstOutputs(), juce::dontSendNotification);
    phones->onClick = [this, phones] { engine.setPhonesOnFirstOutputs (phones->getToggleState()); };
    auto* note = new juce::Label ({}, "Inputs 1-10: the input jacks. Inputs 11-20: the computer (PC mode). Outputs: MAIN L/R, SUB L/R, AUX 1, AUX 2, "
                                      "PHONES 1 L/R, PHONES 2 L/R, CLICK, then USB 1-12.");
    note->setFont (juce::FontOptions (12.0f));
    note->setColour (juce::Label::textColourId, juce::Colours::grey);
    content->addAndMakeVisible (selector);
    content->addAndMakeVisible (outLabel);
    content->addAndMakeVisible (outBox);
    content->addAndMakeVisible (phones);
    content->addAndMakeVisible (note);
    selector->setTopLeftPosition (0, 0);
    outLabel->setBounds (16, 476, 80, 24);
    outBox->setBounds (100, 476, 460, 24);
    phones->setBounds (16, 506, 560, 24);
    note->setBounds (16, 532, 560, 36);
    content->setSize (580, 576);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (content);
    o.dialogTitle = "Audio / MIDI settings";
    o.dialogBackgroundColour = juce::Colour (0xff1d1f24);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

void MainComponent::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::NoIcon, "Trackdesk TD-12",
        "A 12-track recording console: 10 inputs on 8 channel strips, a 12-track recorder (the 10 inputs and the MAIN MIX), "
        "16 effects and a master section, as on the original.\n\n"
        "Knobs and faders: drag (shift for fine), double-click for the default, or scroll. Switches latch.\n"
        "The display: MENU, F1-F4 and the MULTI JOG (drag round it, scroll, click to push). The arrow keys turn it, Return pushes it, "
        "Space is PLAY, and \\ is the footswitch.\n\n"
        "The card is the folder Music/Trackdesk Card: songs in MTR, WAV files to import or play in MUSIC. "
        "PAIRING opens an audio file that plays like a phone paired to the console.\n\n"
        "Audio: inputs 1-10 are the input jacks, inputs 11-20 what PC mode hears; outputs MAIN, SUB, AUX 1/2, PHONES 1/2, CLICK, then USB 1-12. "
        "MIDI clock and time code go to the \"Trackdesk\" MIDI port.");
}

void MainComponent::loadSettings()
{
    if (testing())
        return; // the self-test and snapshots leave the user's settings alone
    juce::PropertiesFile::Options o;
    o.applicationName = "Trackdesk";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.folderName = "Trackdesk";
    settings = std::make_unique<juce::PropertiesFile> (o);
    if (auto xml = settings->getXmlValue ("audioDevice"))
        engine.getDeviceManager().initialise (20, 23, xml.get(), true);
}

void MainComponent::saveSettings()
{
    if (settings == nullptr)
        return;
    if (auto xml = engine.getDeviceManager().createStateXml())
        settings->setValue ("audioDevice", xml.get());
    settings->saveIfNeeded();
}

// ======================================================================== self-test

void MainComponent::runSelfTest (const juce::File& reportFile)
{
    auto lines = std::make_shared<juce::StringArray>();
    auto failures = std::make_shared<int> (0);
    auto log = [lines] (const juce::String& s) { lines->add (s); };
    auto fail = [lines, failures] (const juce::String& s) {
        lines->add ("FAIL  " + s);
        ++*failures;
    };
    const auto results = panel.selfTestControls();
    for (auto& r : results.first)
        log (r);
    for (auto& r : results.second)
        fail (r);

    // sound: pair with a generated tone, record a mixdown, play it back from the card
    auto tone = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("trackdesk-selftest-tone.wav");
    {
        tone.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (tone.createOutputStream().release());
        auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (48000.0).withNumChannels (2).withBitsPerSample (16));
        juce::AudioBuffer<float> b (2, 48000 * 3);
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (0, i, 0.3f * std::sin (6.2831853f * 440.0f * (float) i / 48000.0f)), b.setSample (1, i, b.getSample (0, i));
        if (w != nullptr)
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    engine.pairWithFile (tone);
    engine.withDesk ([] (Desk& d) {
        d.setAssign (Assign::ToMain);
        d.setMasterKnob (MasterKnob::MainFader, law::kFaderUnity);
        d.setMasterSwitch (MasterSwitch::MainMute, false);
    });
    struct Step
    {
        int ms;
        std::function<void()> fn;
    };
    auto steps = std::make_shared<std::vector<Step>>();
    steps->push_back ({ 700, [=, this] {
                           const float l = engine.getOutputLevel();
                           if (l < 0.05f)
                               fail ("the paired device isn't heard on MAIN (" + juce::String (l, 4) + ")");
                           else
                               log ("ok    paired device on MAIN: peak " + juce::String (l, 3));
                           engine.withDesk ([] (Desk& d) {
                               d.press (Key::Record); // (no track armed: a mixdown onto 11/12)
                               d.release (Key::Record);
                           });
                       } });
    steps->push_back ({ 1500, [=, this] {
                           bool rec = false;
                           engine.withDesk ([&] (Desk& d) {
                               rec = d.recorder.isRecording();
                               d.press (Key::Stop);
                               d.release (Key::Stop);
                           });
                           if (! rec)
                               fail ("RECORD didn't record");
                       } });
    steps->push_back ({ 600, [=, this] {
                           bool has = false;
                           juce::String song;
                           engine.withDesk ([&] (Desk& d) {
                               has = d.recorder.hasAudio (kMixTrack);
                               song = d.currentSongName();
                               d.setAssign (Assign::Off);
                               d.press (Key::Stop); // STOP + REW: where recording last started
                               d.press (Key::Rew);
                               d.release (Key::Rew);
                               d.release (Key::Stop);
                               d.setMasterSwitch (MasterSwitch::SdReturn, true);
                           });
                           const auto files = engine.getCard().songFolder (song).findChildFiles (juce::File::findFiles, false, "*.wav");
                           if (! has || files.isEmpty())
                               fail ("the MAIN MIX take wasn't saved (" + juce::String (files.size()) + " files)");
                           else
                               log ("ok    recorded " + juce::String (files.size()) + " take file(s) in " + song);
                       } });
    steps->push_back ({ 400, [=, this] {
                           const float quiet = engine.getOutputLevel();
                           if (quiet > 0.02f)
                               fail ("still sounding with ASSIGN OFF and the recorder stopped (" + juce::String (quiet, 4) + ")");
                           engine.withDesk ([] (Desk& d) {
                               d.press (Key::Play);
                               d.release (Key::Play);
                           });
                       } });
    steps->push_back ({ 700, [=, this] {
                           const float l = engine.getOutputLevel();
                           if (l < 0.05f)
                               fail ("the take doesn't play back through SD MAIN MIX RETURN (" + juce::String (l, 4) + ")");
                           else
                               log ("ok    playback from the card: peak " + juce::String (l, 3));
                           engine.withDesk ([] (Desk& d) {
                               d.press (Key::Stop);
                               d.release (Key::Stop);
                               d.setMasterSwitch (MasterSwitch::SdReturn, false);
                           });
                           tone.deleteFile();
                           lines->add ("");
                           lines->add (*failures == 0 ? juce::String ("ALL PASSED") : juce::String (*failures) + " FAILED");
                           reportFile.replaceWithText (lines->joinIntoString ("\n") + "\n");
                           juce::JUCEApplicationBase::quit();
                       } });
    struct Runner
    {
        static void next (std::shared_ptr<std::vector<Step>> s, size_t i, juce::Component::SafePointer<MainComponent> safe)
        {
            if (safe == nullptr || i >= s->size())
                return;
            juce::Timer::callAfterDelay ((*s)[i].ms, [s, i, safe] {
                if (safe == nullptr)
                    return;
                (*s)[i].fn();
                next (s, i + 1, safe);
            });
        }
    };
    Runner::next (steps, 0, this);
}
} // namespace trackdesk::ui
