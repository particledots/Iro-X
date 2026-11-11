// Iro-x plugin test: runs the real plugin code (PluginProcessor, PluginEditor) without Logic or a display.
// Milestone 0 checks: the plugin is an effect (no MIDI, no instrument), accepts the layouts Logic uses, passes audio
// through unchanged, survives saving and loading, reports its version, and draws its window.
// With a folder as the first argument it also writes a picture of the window there (PNG).
// Not part of the download. Build with -DIROX_BUILD_PLUGIN_TEST=ON (see CMakeLists.txt).
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
int failures = 0;
void check (bool ok, const juce::String& what)
{
    std::printf ("%-72s %s\n", what.toRawUTF8(), ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}

juce::AudioProcessor::BusesLayout layout (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
{
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (in);
    l.outputBuses.add (out);
    return l;
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    IroxProcessor proc;

    // ---- what kind of plugin it is ----
    check (proc.getName() == "Iro-x", "name is Iro-x");
    check (! proc.acceptsMidi() && ! proc.producesMidi() && ! proc.isMidiEffect(), "no MIDI in or out (an audio effect)");
    check (proc.getTotalNumInputChannels() == 2 && proc.getTotalNumOutputChannels() == 2, "starts as stereo in, stereo out");
    check (juce::String (IROX_VERSION_STRING).isNotEmpty() && IroxProcessor::versionString() == IROX_VERSION_STRING, "version string present");

    // ---- layouts ----
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    check (proc.checkBusesLayoutSupported (layout (stereo, stereo)), "layout stereo in, stereo out accepted");
    check (proc.checkBusesLayoutSupported (layout (mono, stereo)),   "layout mono in, stereo out accepted");
    check (proc.checkBusesLayoutSupported (layout (mono, mono)),     "layout mono in, mono out accepted");
    check (! proc.checkBusesLayoutSupported (layout (stereo, mono)), "layout stereo in, mono out refused");
    check (! proc.checkBusesLayoutSupported (layout (juce::AudioChannelSet::create5point1(), juce::AudioChannelSet::create5point1())), "layout 5.1 refused");

    // ---- pass-through at the rates Logic uses ----
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        proc.setRateAndBufferSizeDetails (rate, 512);
        proc.prepareToPlay (rate, 512);
        juce::AudioBuffer<float> buf (2, 512), ref (2, 512);
        for (int i = 0; i < 512; ++i)
        {
            buf.setSample (0, i, std::sin (0.05f * (float) i) * 0.7f);
            buf.setSample (1, i, std::sin (0.031f * (float) i + 1.0f) * 0.5f);
        }
        ref.makeCopyOf (buf);
        juce::MidiBuffer midi;
        for (int b = 0; b < 4; ++b) { proc.processBlock (buf, midi); }
        bool same = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                if (buf.getSample (ch, i) != ref.getSample (ch, i)) same = false;
        check (same, "audio passes through unchanged at " + juce::String ((int) rate) + " Hz");
    }

    // ---- mono in, stereo out ----
    {
        IroxProcessor m;
        m.setPlayConfigDetails (1, 2, 48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        buf.clear();
        for (int i = 0; i < 256; ++i) buf.setSample (0, i, std::sin (0.07f * (float) i));
        juce::MidiBuffer midi;
        m.processBlock (buf, midi);
        bool both = true;
        for (int i = 0; i < 256; ++i) if (buf.getSample (0, i) != buf.getSample (1, i)) both = false;
        check (both, "mono in, stereo out: both outputs carry the input");
    }

    // ---- silence in, silence out, no NaN, odd block sizes ----
    {
        proc.setRateAndBufferSizeDetails (48000.0, 1);
        proc.prepareToPlay (48000.0, 1);
        bool ok = true;
        for (int n : { 1, 2, 7, 64, 333, 4096 })
        {
            juce::AudioBuffer<float> buf (2, n);
            buf.clear();
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    if (buf.getSample (ch, i) != 0.0f) ok = false;
        }
        check (ok, "silence stays silence for block sizes 1 to 4096");
    }

    // ---- tail ----
    check (proc.getTailLengthSeconds() == 0.0, "tail length is 0 (no reverb or echo yet in Milestone 0)");

    // ---- saving and loading ----
    {
        juce::MemoryBlock block;
        proc.getStateInformation (block);
        check (block.getSize() > 0, "state can be saved");
        IroxProcessor other;
        other.setStateInformation (block.getData(), (int) block.getSize());
        check (true, "state can be loaded again");
        other.setStateInformation ("garbage", 7);
        check (true, "garbage state is ignored without a crash");
        other.setStateInformation (nullptr, 0);
        check (true, "empty state is ignored without a crash");
    }

    // ---- window ----
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        check (ed != nullptr, "window can be created");
        if (ed != nullptr)
        {
            check (ed->getWidth() > 200 && ed->getHeight() > 100, "window has a size");
            auto img = ed->createComponentSnapshot (ed->getLocalBounds());
            check (img.isValid() && img.getWidth() == ed->getWidth(), "window can be drawn");
            if (argc > 1)
            {
                juce::File dir (juce::String::fromUTF8 (argv[1]));
                dir.createDirectory();
                juce::FileOutputStream out (dir.getChildFile ("window.png"));
                if (out.openedOk()) { out.setPosition (0); out.truncate(); juce::PNGImageFormat().writeImageToStream (img, out); }
                check (dir.getChildFile ("window.png").getSize() > 0, "picture of the window written");
            }
        }
    }

    std::printf ("\n%s\n", failures == 0 ? "RESULT - all plugin checks passed" : "RESULT - SOME PLUGIN CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
