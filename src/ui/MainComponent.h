#pragma once

// The app's window: the panel scaled to fit, a few buttons of the app's own (audio and MIDI settings, help), the
// file chooser PAIRING asks for, and the testing aids (a snapshot of the panel, the self-test).

#include "Panel.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace trackdesk::ui
{
class MainComponent : public juce::Component, private juce::Timer
{
public:
    explicit MainComponent (Engine& engine);
    ~MainComponent() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void prepareToQuit();

private:
    void timerCallback() override;
    void showAudioSettings();
    void showHelp();
    void loadSettings();
    void saveSettings();
    void runSelfTest (const juce::File& report);

    Engine& engine;
    Panel panel;
    juce::TextButton audioBtn { "AUDIO / MIDI" }, helpBtn { "HELP" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::PropertiesFile> settings;
    juce::String message;
    juce::uint32 messageUntil = 0;
};
} // namespace trackdesk::ui
