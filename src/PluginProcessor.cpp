#include "PluginProcessor.h"
#include "PluginEditor.h"

IroxProcessor::IroxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void IroxProcessor::prepareToPlay (double, int) {}

// Stereo in, stereo out. A mono Logic track is also accepted (mono in, mono out, or mono in, stereo out).
bool IroxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo()) return true;
    if (in == juce::AudioChannelSet::mono() && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())) return true;
    return false;
}

void IroxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    // Mono in, stereo out: the first input channel is copied to both outputs. Otherwise the buffer already holds
    // the input and is left as it is (pass-through).
    if (numIn == 1 && numOut >= 2)
        for (int ch = 1; ch < numOut; ++ch)
            buffer.copyFrom (ch, 0, buffer, 0, 0, n);
}

juce::AudioProcessorEditor* IroxProcessor::createEditor() { return new IroxEditor (*this); }

// Nothing to save yet; only the version is written so that later versions know what they are reading.
void IroxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree state ("IroxState");
    state.setProperty ("version", versionString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void IroxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        juce::ignoreUnused (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IroxProcessor(); }
