#include "PluginProcessor.h"
#include "PluginEditor.h"

IroxProcessor::IroxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "IroxState", irox_params::createLayout())
{
}

void IroxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock_ = std::max (samplesPerBlock, 512);
    for (auto& s : scratch_) s.assign ((size_t) maxBlock_, 0.f);
    stream_.prepare (sampleRate, maxBlock_);
    stream_.setParams (irox_params::toEngine (apvts), apvts.getRawParameterValue ("input_level")->load(), apvts.getRawParameterValue ("output_level")->load());
    setLatencySamples (stream_.latencySamples());
}

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
    const int total = buffer.getNumSamples();
    if (maxBlock_ == 0) return;     // not prepared

    stream_.setParams (irox_params::toEngine (apvts), apvts.getRawParameterValue ("input_level")->load(), apvts.getRawParameterValue ("output_level")->load());

    for (int pos = 0; pos < total; pos += maxBlock_)
    {
        const int n = std::min (maxBlock_, total - pos);
        const float* inL = buffer.getReadPointer (0) + pos;
        const float* inR = numIn > 1 ? buffer.getReadPointer (1) + pos : inL;
        // copy the input first: the output goes into the same buffer
        std::copy (inL, inL + n, scratch_[0].begin());
        std::copy (inR, inR + n, scratch_[1].begin());
        stream_.process (scratch_[0].data(), scratch_[1].data(), scratch_[2].data(), scratch_[3].data(), n);
        if (numOut >= 2)
        {
            std::copy (scratch_[2].begin(), scratch_[2].begin() + n, buffer.getWritePointer (0) + pos);
            std::copy (scratch_[3].begin(), scratch_[3].begin() + n, buffer.getWritePointer (1) + pos);
        }
        else
        {
            float* o = buffer.getWritePointer (0) + pos;
            for (int i = 0; i < n; ++i) o[i] = 0.5f * (scratch_[2][(size_t) i] + scratch_[3][(size_t) i]);
        }
    }
}

juce::AudioProcessorEditor* IroxProcessor::createEditor() { return new IroxEditor (*this); }

void IroxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("version", versionString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void IroxProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IroxProcessor(); }
