// Iro-x plugin test: runs the real plugin code (PluginProcessor, PluginEditor) without Logic or a display.
// Milestone 1 checks: the plugin is an effect (no MIDI, no instrument), accepts the layouts Logic uses, processes audio
// with the Iroi engine at 44.1, 48 and 96 kHz, reports its delay, keeps its settings when saved and loaded, copes with
// several instances and moving controls, and draws its window.
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

    // ---- helpers ----
    auto fillSignal = [] (juce::AudioBuffer<float>& b, int offset)
    {
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float t = (float) (i + offset);
            b.setSample (0, i, std::sin (0.05f * t) * 0.5f + 0.2f * std::sin (0.9f * t));
            if (b.getNumChannels() > 1) b.setSample (1, i, std::sin (0.031f * t + 1.0f) * 0.4f);
        }
    };
    auto setParam = [] (IroxProcessor& p, const char* id, float normalised)
    {
        if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (normalised);
    };
    // runs about 'blocks' blocks of the test signal; returns the output peak and whether all samples were finite
    auto run = [&] (IroxProcessor& p, double rate, int blockSize, int blocks, float& peak, bool& finite, std::vector<float>* keepL = nullptr)
    {
        p.setRateAndBufferSizeDetails (rate, blockSize);
        p.prepareToPlay (rate, blockSize);
        peak = 0.f; finite = true;
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buf (2, blockSize);
            fillSignal (buf, b * blockSize);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                {
                    const float v = buf.getSample (ch, i);
                    if (! std::isfinite (v)) finite = false;
                    peak = std::max (peak, std::abs (v));
                }
            if (keepL != nullptr) for (int i = 0; i < blockSize; ++i) keepL->push_back (buf.getSample (0, i));
        }
    };

    // ---- parameters ----
    {
        int n = 0; irox_params::floatParams (n);
        // 36 float parameters in the table + 2 lists + input and output level
        check (proc.getParameters().size() == n + 4, "parameter count is " + juce::String (n + 4));
        bool allThere = true, allAutomatable = true;
        for (auto* p : proc.getParameters())
        {
            auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p);
            if (withId == nullptr || proc.apvts.getParameter (withId->paramID) == nullptr) allThere = false;
            if (! p->isAutomatable()) allAutomatable = false;
        }
        check (allThere, "every parameter has an id");
        check (allAutomatable, "every parameter can be automated");
        auto* in = proc.apvts.getParameter ("input_level");
        check (in != nullptr && std::abs (in->convertFrom0to1 (in->getDefaultValue())) < 0.001f, "input level starts at 0 dB");
        check (in != nullptr && std::abs (in->convertFrom0to1 (0.f) + 24.f) < 0.01f && std::abs (in->convertFrom0to1 (1.f) - 12.f) < 0.01f, "input level range is -24 to +12 dB");
    }

    // ---- the effect really processes the audio, at the rates Logic uses ----
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        IroxProcessor p;
        float peak = 0.f; bool finite = true;
        std::vector<float> dry, wet;
        run (p, rate, 512, 24, peak, finite, &dry);
        check (finite && peak > 0.05f && peak < 4.f, "default settings give sound, finite, at " + juce::String ((int) rate) + " Hz");
        check (p.underruns() == 0, "no buffer shortage at " + juce::String ((int) rate) + " Hz");
        IroxProcessor q;
        setParam (q, "resonator_level", 1.0f); setParam (q, "resonator_feedback", 0.9f);
        setParam (q, "echo_level", 1.0f); setParam (q, "ambience_level", 1.0f);
        run (q, rate, 512, 24, peak, finite, &wet);
        check (finite && peak < 16.f, "all effects on: finite, sensible level at " + juce::String ((int) rate) + " Hz");
        double diff = 0; for (size_t i = 0; i < dry.size(); ++i) diff += std::abs (dry[i] - wet[i]);
        check (diff / (double) dry.size() > 1e-3, "turning the effects up changes the sound at " + juce::String ((int) rate) + " Hz");
    }

    // ---- reported delay (latency) ----
    {
        IroxProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 512); p.prepareToPlay (48000.0, 512);
        check (p.getLatencySamples() == 32, "reported delay at 48 kHz is 32 samples (" + juce::String (p.getLatencySamples()) + ")");
        p.setRateAndBufferSizeDetails (44100.0, 512); p.prepareToPlay (44100.0, 512);
        check (p.getLatencySamples() > 32 && p.getLatencySamples() < 100, "reported delay at 44.1 kHz is " + juce::String (p.getLatencySamples()) + " samples");
        p.setRateAndBufferSizeDetails (96000.0, 512); p.prepareToPlay (96000.0, 512);
        check (p.getLatencySamples() > 64 && p.getLatencySamples() < 200, "reported delay at 96 kHz is " + juce::String (p.getLatencySamples()) + " samples");
    }

    // ---- mono in, stereo out and mono in, mono out ----
    {
        IroxProcessor m;
        m.setPlayConfigDetails (1, 2, 48000.0, 256);
        m.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> buf (2, 256);
        juce::MidiBuffer midi;
        float peak = 0; bool fin = true;
        for (int b = 0; b < 20; ++b)
        {
            buf.clear();
            for (int i = 0; i < 256; ++i) buf.setSample (0, i, std::sin (0.07f * (float) (i + b * 256)) * 0.5f);
            m.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) { fin = fin && std::isfinite (buf.getSample (ch, i)); peak = std::max (peak, std::abs (buf.getSample (ch, i))); }
        }
        check (fin && peak > 0.05f, "mono in, stereo out gives sound");
        IroxProcessor m2;
        m2.setPlayConfigDetails (1, 1, 48000.0, 256);
        m2.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> mb (1, 256);
        peak = 0; fin = true;
        for (int b = 0; b < 20; ++b)
        {
            for (int i = 0; i < 256; ++i) mb.setSample (0, i, std::sin (0.07f * (float) (i + b * 256)) * 0.5f);
            m2.processBlock (mb, midi);
            for (int i = 0; i < 256; ++i) { fin = fin && std::isfinite (mb.getSample (0, i)); peak = std::max (peak, std::abs (mb.getSample (0, i))); }
        }
        check (fin && peak > 0.05f, "mono in, mono out gives sound");
    }

    // ---- silence in, silence out, odd block sizes ----
    {
        IroxProcessor p;
        p.setRateAndBufferSizeDetails (48000.0, 1); p.prepareToPlay (48000.0, 1);
        bool ok = true;
        for (int n : { 1, 2, 7, 64, 333, 4096 })
        {
            juce::AudioBuffer<float> buf (2, n);
            buf.clear();
            juce::MidiBuffer midi;
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < n; ++i)
                    if (! (std::abs (buf.getSample (ch, i)) < 1e-3f)) ok = false;
        }
        check (ok, "silence stays (nearly) silent for block sizes 1 to 4096");
    }

    // ---- block size does not change the sound ----
    {
        IroxProcessor a, b;
        for (auto* p : { &a, &b }) { setParam (*p, "echo_level", 0.7f); setParam (*p, "ambience_level", 0.6f); setParam (*p, "resonator_level", 0.5f); }
        std::vector<float> ra, rb; float pk; bool f;
        run (a, 48000.0, 128, 40, pk, f, &ra);
        run (b, 48000.0, 128, 40, pk, f, &rb);
        check (ra == rb, "two instances with the same settings give identical audio");
    }

    // ---- several instances do not disturb each other ----
    {
        IroxProcessor a, b, c;
        setParam (b, "echo_level", 1.0f); setParam (b, "echo_repeats", 0.9f);
        setParam (c, "ambience_level", 1.0f);
        std::vector<float> alone, together; float pk; bool f;
        {
            IroxProcessor solo;
            run (solo, 48000.0, 256, 30, pk, f, &alone);
        }
        a.setRateAndBufferSizeDetails (48000.0, 256); a.prepareToPlay (48000.0, 256);
        b.setRateAndBufferSizeDetails (48000.0, 256); b.prepareToPlay (48000.0, 256);
        c.setRateAndBufferSizeDetails (48000.0, 256); c.prepareToPlay (48000.0, 256);
        juce::MidiBuffer midi;
        for (int blk = 0; blk < 30; ++blk)
            for (auto* p : { &a, &b, &c })
            {
                juce::AudioBuffer<float> buf (2, 256);
                fillSignal (buf, blk * 256);
                p->processBlock (buf, midi);
                if (p == &a) for (int i = 0; i < 256; ++i) together.push_back (buf.getSample (0, i));
            }
        check (alone == together, "three instances running at once do not disturb each other");
    }

    // ---- tail ----
    check (proc.getTailLengthSeconds() == 0.0, "tail length is 0 in Milestone 1 (measured and reported in Milestone 2)");

    // ---- saving and loading: settings come back ----
    {
        IroxProcessor src;
        setParam (src, "filter_cutoff", 0.37f);
        setParam (src, "echo_level", 0.8f);
        setParam (src, "filter_mode", 2.0f / 3.0f);       // third entry
        setParam (src, "output_level", 0.75f);
        juce::MemoryBlock block;
        src.getStateInformation (block);
        check (block.getSize() > 0, "state can be saved");
        IroxProcessor dst;
        dst.setStateInformation (block.getData(), (int) block.getSize());
        auto get = [] (IroxProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        check (std::abs (get (dst, "filter_cutoff") - get (src, "filter_cutoff")) < 1e-5f
               && std::abs (get (dst, "echo_level") - 0.8f) < 1e-4f
               && (int) get (dst, "filter_mode") == 2
               && std::abs (get (dst, "output_level") - get (src, "output_level")) < 1e-4f, "saved settings come back after loading");
        dst.setStateInformation ("garbage", 7);
        check (true, "garbage state is ignored without a crash");
        dst.setStateInformation (nullptr, 0);
        check (true, "empty state is ignored without a crash");
        float pk; bool f;
        run (dst, 48000.0, 256, 10, pk, f);
        check (f, "plugin still works after loading bad state");
    }

    // ---- automation while playing (parameters change every block) ----
    {
        IroxProcessor p;
        p.setRateAndBufferSizeDetails (44100.0, 300); p.prepareToPlay (44100.0, 300);
        juce::MidiBuffer midi; bool fin = true;
        juce::Random rnd (7);
        for (int b = 0; b < 200; ++b)
        {
            for (auto* prm : p.getParameters()) prm->setValueNotifyingHost (rnd.nextFloat());
            juce::AudioBuffer<float> buf (2, 300);
            fillSignal (buf, b * 300);
            p.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 300; ++i) fin = fin && std::isfinite (buf.getSample (ch, i));
        }
        check (fin, "200 blocks with every control moving: no bad numbers");
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
            for (int w : { 885, 1475, 1770 })
            {
                ed->setSize (w, w * IroxEditor::baseHeight / IroxEditor::baseWidth);
                auto im = ed->createComponentSnapshot (ed->getLocalBounds());
                check (im.isValid() && im.getWidth() == w, "window can be drawn at width " + juce::String (w));
            }
            ed->setSize (IroxEditor::baseWidth, IroxEditor::baseHeight);
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
