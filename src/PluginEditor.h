#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Milestone 0: a placeholder window. The real window (see the approved mock-up) comes in Milestone 1.
class IroxEditor : public juce::AudioProcessorEditor
{
public:
    explicit IroxEditor (IroxProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    IroxProcessor& proc;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroxEditor)
};
