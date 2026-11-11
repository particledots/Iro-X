#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "IroxStream.h"

// Iro-x, Milestone 1: the Iroi effects engine as an audio effect. Stereo (or mono) in, stereo (or mono) out.
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
    // Milestone 1 reports no tail; Milestone 2 measures the reverb and echo tail and reports it here.
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::String versionString() { return IROX_VERSION_STRING; }

    juce::AudioProcessorValueTreeState apvts;
    irox::Stream& stream() { return stream_; }
    int underruns() const { return stream_.underruns(); }

private:
    irox::Stream stream_;
    std::vector<float> scratch_[4];
    int maxBlock_ = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroxProcessor)
};
