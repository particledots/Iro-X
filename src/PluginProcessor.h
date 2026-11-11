#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Iro-x, Milestone 0: an empty audio effect. Audio goes in and comes out unchanged. There is no engine yet.
class IroxProcessor : public juce::AudioProcessor
{
public:
    IroxProcessor();
    ~IroxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Iro-x"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Milestone 0 has no reverb or echo yet, so nothing rings on. Milestone 2 reports the real tail length here.
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::String versionString() { return IROX_VERSION_STRING; }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroxProcessor)
};
