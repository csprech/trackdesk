#include "TrackdeskProcessor.h"
#include "../ui/Panel.h"

namespace trackdesk
{
namespace
{
    class Editor : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        explicit Editor (TrackdeskProcessor& p) : juce::AudioProcessorEditor (p), proc (p), panel (p.getEngine())
        {
            addAndMakeVisible (panel);
            setSize (TrackdeskProcessor::kPanelWidth, TrackdeskProcessor::kPanelHeight);
            // PAIRING asks for an audio file to play as the paired device
            p.getEngine().onPairingRequest = [this] {
                chooser = std::make_unique<juce::FileChooser> ("Pair with an audio file", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                               "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
                    if (fc.getResult().existsAsFile())
                        proc.getEngine().pairWithFile (fc.getResult());
                });
            };
        }
        ~Editor() override { proc.getEngine().onPairingRequest = nullptr; }
        void resized() override
        {
            const float sc = std::min (getWidth() / (float) TrackdeskProcessor::kPanelWidth, getHeight() / (float) TrackdeskProcessor::kPanelHeight);
            const float ox = (getWidth() - TrackdeskProcessor::kPanelWidth * sc) * 0.5f, oy = (getHeight() - TrackdeskProcessor::kPanelHeight * sc) * 0.5f;
            panel.setTransform (juce::AffineTransform::scale (sc).translated (ox, oy));
        }
        void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0d0d0e)); }

    private:
        void timerCallback() override {}
        TrackdeskProcessor& proc;
        ui::Panel panel;
        std::unique_ptr<juce::FileChooser> chooser;
    };
} // namespace

TrackdeskProcessor::TrackdeskProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput ("Inputs", juce::AudioChannelSet::discreteChannels (kNumInputs), true)
                                .withOutput ("Outputs", juce::AudioChannelSet::discreteChannels (kNumOutputs), true))
{
    for (auto& p : plugged)
        p = false;
}

TrackdeskProcessor::~TrackdeskProcessor() = default;

void TrackdeskProcessor::setInputPlugged (int channel, bool p)
{
    if (channel >= 0 && channel < kNumInputs)
        plugged[(size_t) channel] = p;
    bool usb = plugged[10] || plugged[11];
    engine.setJacks (aux2.load(), usb);
}

void TrackdeskProcessor::setAux2Plugged (bool p)
{
    aux2 = p;
    engine.setJacks (p, plugged[10] || plugged[11]);
}

bool TrackdeskProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    return layout.getMainOutputChannelSet().size() == kNumOutputs
           && (layout.getMainInputChannelSet().isDisabled() || layout.getMainInputChannelSet().size() == kNumInputs);
}

void TrackdeskProcessor::prepareToPlay (double sampleRate, int maxBlock)
{
    inputCopy.setSize (kNumInputs, juce::jmax (1, maxBlock));
    engine.prepareHosted (sampleRate, maxBlock);
}

void TrackdeskProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const int n = buffer.getNumSamples();
    const int numIn = juce::jmin (kNumInputs, getTotalNumInputChannels(), buffer.getNumChannels());
    if (n > inputCopy.getNumSamples())
        inputCopy.setSize (kNumInputs, n, false, false, true);
    // the inputs share the buffer with the outputs: copy them first
    for (int c = 0; c < numIn; ++c)
        inputCopy.copyFrom (c, 0, buffer, c, 0, n);
    const float* live[kInputs] {};
    const float* pc[2] {};
    for (int c = 0; c < kInputs; ++c)
        live[c] = c < numIn && plugged[(size_t) c] ? inputCopy.getReadPointer (c) : nullptr;
    for (int c = 0; c < 2; ++c)
        pc[c] = kInputs + c < numIn && plugged[(size_t) (kInputs + c)] ? inputCopy.getReadPointer (kInputs + c) : nullptr;
    float* outs[kNumOutputs] {};
    const int numOut = juce::jmin (kNumOutputs, getTotalNumOutputChannels(), buffer.getNumChannels());
    for (int c = 0; c < numOut; ++c)
        outs[c] = buffer.getWritePointer (c);
    for (int c = numOut; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);
    engine.processHosted (live, pc, outs, numOut, n, midi);
}

juce::AudioProcessorEditor* TrackdeskProcessor::createEditor() { return new Editor (*this); }

void TrackdeskProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = engine.getState();
    state.getDynamicObject()->setProperty ("format", "trackdesk-state");
    juce::MemoryOutputStream mos (dest, false);
    mos.writeString (juce::JSON::toString (state, true));
}

void TrackdeskProcessor::setStateInformation (const void* data, int size)
{
    juce::MemoryInputStream mis (data, (size_t) size, false);
    auto json = juce::JSON::parse (mis.readEntireStreamAsString());
    if (json.getProperty ("format", {}).toString() != "trackdesk-state")
        return;
    json.getDynamicObject()->setProperty ("format", "trackdesk-settings");
    engine.setState (json);
}
} // namespace trackdesk
