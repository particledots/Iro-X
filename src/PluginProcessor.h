#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "IroxStream.h"

// Iro-x: the Iroi effects engine as an audio effect. Stereo (or mono) in, stereo (or mono) out.
class IroxProcessor : public juce::AudioProcessor,
                      private juce::AsyncUpdater
{
public:
    IroxProcessor();
    ~IroxProcessor() override { cancelPendingUpdate(); }

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
    // How long the reverb, echo and resonator ring on after the audio stops; Logic keeps the plugin running that long
    // at the end of a region. The number is measured by the engine test (kTailSeconds, Parameters.h).
    double getTailLengthSeconds() const override { return irox_params::kTailSeconds; }

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
    // The randomiser (the button in the window, and the "Randomise" parameter going from off to on). Message thread.
    void randomise();
    // For the plugin test: run a randomise request that is waiting (normally the message loop does it).
    void runPendingRandomise() { handleUpdateNowIfNeeded(); }
    // What Logic told us about tempo and transport in the last block, for the window.
    double lastHostBpm() const { return lastBpm_.load(); }
    int underruns() const { return stream_.underruns(); }

private:
    void handleAsyncUpdate() override;
    irox::Stream stream_;
    juce::Random random_;
    bool lastRandomise_ = false;
    std::atomic<double> lastBpm_ { 0.0 };
    std::vector<float> scratch_[4];
    int maxBlock_ = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroxProcessor)
};
