#pragma once

// Trackdesk as an audio processor, for hosts (the Tabletop studio, or a plug-in host): the same engine and panel
// as the app, without an audio device of its own. Its jacks become the processor's channels:
//   inputs   0-9     INPUT 1-6, 7/8 (L, R), 9/10 (L, R)
//   inputs   10, 11  USB L, R (what PC mode hears on channels 1-2)
//   outputs  0-10    MAIN L, R, SUB L, R, AUX 1, AUX 2, PHONES 1 L, R, PHONES 2 L, R, CLICK
//   MIDI out         clock and time code
// The songs live in the same card folder as the app's; the settings travel with the session.

#include "../engine/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace trackdesk
{
class TrackdeskProcessor : public juce::AudioProcessor
{
public:
    TrackdeskProcessor();
    ~TrackdeskProcessor() override;

    static constexpr int kPanelWidth = 1500, kPanelHeight = 1580;
    static constexpr int kNumInputs = 12, kNumOutputs = 11;

    Engine& getEngine() { return engine; }
    /** The host says which input jacks are cabled (an unplugged jack is silent; a stereo channel with only its L jack
        is mono) and whether the AUX 2 output is in use (the built-in effect is then unavailable). */
    void setInputPlugged (int channel, bool plugged);
    void setAux2Plugged (bool plugged);

    const juce::String getName() const override { return "Trackdesk"; }
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    Engine engine { true };
    juce::AudioBuffer<float> inputCopy;
    std::array<std::atomic<bool>, kNumInputs> plugged {};
    std::atomic<bool> aux2 { false };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackdeskProcessor)
};
} // namespace trackdesk
