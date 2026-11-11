// Iro-x plugin test: runs the real plugin code (PluginProcessor, PluginEditor) without Logic or a display.
// Checks: the plugin is an effect (no MIDI, no instrument), accepts the layouts Logic uses, processes audio
// with the Iroi engine at 44.1, 48 and 96 kHz, reports its delay, keeps its settings when saved and loaded, copes with
// several instances and moving controls, follows the host's tempo, randomises (button and parameter), reports its tail,
// and draws its window.
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

// A pretend host that tells the plugin a tempo and a song position.
struct FakePlayHead : juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0; bool playing = true, hasBpm = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        if (hasBpm) info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
};

// Counts how often a control was grabbed and let go (the randomiser must do both for every control it moves).
struct GestureCounter : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int, bool starting) override { (starting ? begins : ends)++; }
};

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
        // 36 float parameters in the table + 2 lists + input and output level + the Randomise parameter (added in Milestone 2)
        check (proc.getParameters().size() == n + 5, "parameter count is " + juce::String (n + 5));
        check (proc.apvts.getParameter ("randomise") != nullptr && proc.getParameters().getLast() == proc.apvts.getParameter ("randomise"), "Randomise is the last parameter (nothing saved before it moved)");
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
    check (proc.getTailLengthSeconds() == irox_params::kTailSeconds && proc.getTailLengthSeconds() >= 10.0, "tail length reported to the host: " + juce::String (proc.getTailLengthSeconds(), 1) + " s");

    // ---- Logic's tempo ----
    {
        FakePlayHead head;
        auto makeWet = [] (IroxProcessor& p)
        {
            for (const char* id : { "echo_level", "ambience_level", "resonator_level" }) if (auto* prm = p.apvts.getParameter (id)) prm->setValueNotifyingHost (0.8f);
            p.apvts.getParameter ("mod_level")->setValueNotifyingHost (1.0f);
            p.apvts.getParameter ("mod_type")->setValueNotifyingHost (1.0f / 6.0f);
            p.apvts.getParameter ("filter_cutoff")->setValueNotifyingHost (0.5f);
            p.apvts.getParameter ("filter_cutoff_mod")->setValueNotifyingHost (1.0f);
        };
        auto runWith = [&] (double bpm, bool hasBpm, std::vector<float>& out, double& shownBpm)
        {
            IroxProcessor p; makeWet (p);
            head.bpm = bpm; head.hasBpm = hasBpm; head.ppq = 0.0; head.playing = true;
            p.setPlayHead (&head);
            p.setRateAndBufferSizeDetails (48000.0, 512); p.prepareToPlay (48000.0, 512);
            juce::MidiBuffer midi;
            for (int b = 0; b < 60; ++b)
            {
                juce::AudioBuffer<float> buf (2, 512); fillSignal (buf, b * 512);
                p.processBlock (buf, midi);
                head.ppq += head.bpm / 60.0 * 512.0 / 48000.0;
                for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i));
            }
            shownBpm = p.lastHostBpm();
            p.setPlayHead (nullptr);
        };
        std::vector<float> a60, a120, a120b, none; double s1 = 0, s2 = 0, s3 = 0, s4 = 0;
        runWith (60.0, true, a60, s1); runWith (120.0, true, a120, s2); runWith (120.0, true, a120b, s3); runWith (0.0, false, none, s4);
        check (s1 == 60.0 && s2 == 120.0, "the plugin sees the host tempo (" + juce::String (s1, 0) + " and " + juce::String (s2, 0) + " BPM)");
        check (s4 == 0.0, "no tempo from the host: the plugin knows it has none");
        check (a120 == a120b, "the same tempo and song position give identical audio");
        double d = 0; for (size_t i = 0; i < a60.size(); ++i) d += std::abs (a60[i] - a120[i]);
        check (d / (double) a60.size() > 1e-3, "a different host tempo changes the modulation and echo (tempo-synced parts)");
        double d2 = 0; for (size_t i = 0; i < none.size(); ++i) d2 += std::abs (none[i] - a120[i]);
        check (d2 / (double) none.size() > 1e-3, "no host tempo (internal clock) sounds different from 120 BPM");
        bool fin = true; for (float v : a60) fin = fin && std::isfinite (v);
        check (fin, "audio stays finite with host tempo");
    }

    // ---- the randomiser ----
    {
        // the arithmetic of the firmware's randomiser, at the edges
        bool inRange = true, down = false, up = false; juce::Random r (5);
        for (int i = 0; i < 200000; ++i)
        {
            const float v0 = (i % 7 == 0) ? 0.f : (i % 11 == 0) ? 1.f : r.nextFloat();
            const float v = irox_params::randomisedValue (v0, r.nextFloat(), r.nextFloat(), r.nextFloat());
            if (! (v >= 0.f && v <= 1.f)) inRange = false;
            if (v < v0 - 1e-6f) down = true;
            if (v > v0 + 1e-6f) up = true;
        }
        check (inRange && down && up, "randomiser arithmetic: result always 0 to 1, moves up and down");
        check (irox_params::randomisedValue (0.f, 0.5f, 0.4f, 0.9f) == 0.2f && irox_params::randomisedValue (1.f, 0.5f, 0.4f, 0.1f) == 0.8f,
               "randomiser at the ends: a value at 0 can only go up, a value at 1 only down");
        check (irox_params::randomisedValue (0.5f, 0.f, 0.9f, 0.1f) == 0.5f, "Rnd amount 0: the value does not move");

        IroxProcessor p;
        GestureCounter gc;
        for (auto* prm : p.getParameters()) prm->addListener (&gc);
        auto val = [&] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        int n = 0; const auto* targets = irox_params::randomTargets (n);
        check (n == 10, "the randomiser moves ten controls");
        // untouched controls (not in the ten) and the Rnd / Mod amounts must stay where they are
        setParam (p, "filter_level", 0.9f); setParam (p, "echo_level", 0.3f); setParam (p, "filter_mode", 1.0f / 3.0f);
        setParam (p, "filter_cutoff_mod", 0.4f); setParam (p, "output_level", 0.7f); setParam (p, "resonator_dissonance", 0.6f);
        const float level0 = val ("filter_level"), echo0 = val ("echo_level"), mod0 = val ("filter_cutoff_mod"), out0 = val ("output_level"), diss0 = val ("resonator_dissonance");
        const float mode0 = val ("filter_mode");
        for (int i = 0; i < n; ++i) { setParam (p, targets[i].id, 0.5f); setParam (p, targets[i].rndId, 0.2f); }
        gc.begins = gc.ends = 0;
        p.randomise();
        bool within = true, moved = false;
        for (int i = 0; i < n; ++i)
        {
            const float v = val (targets[i].id);
            if (std::abs (v - 0.5f) > 0.2f + 1e-4f) within = false;
            if (std::abs (v - 0.5f) > 1e-4f) moved = true;
        }
        check (within && moved, "Randomise moves the ten controls, each by no more than its Rnd amount (0.2)");
        check (gc.begins == 10 && gc.ends == 10, "every randomised control is grabbed and let go (Logic can record it): " + juce::String (gc.begins) + " / " + juce::String (gc.ends));
        check (val ("filter_level") == level0 && val ("echo_level") == echo0 && val ("filter_cutoff_mod") == mod0 && val ("output_level") == out0
                   && val ("resonator_dissonance") == diss0 && val ("filter_mode") == mode0, "Randomise leaves every other control alone");
        // many presses: values stay in range, spread over the allowed distance, and Rnd 0 freezes a control
        setParam (p, "filter_cutoff_rnd", 0.0f); setParam (p, "filter_cutoff", 0.37f);
        setParam (p, "echo_repeats_rnd", 1.0f);
        float lo = 9.f, hi = -9.f; bool ok = true;
        for (int i = 0; i < 400; ++i)
        {
            p.randomise();
            for (int k = 0; k < n; ++k) { const float v = val (targets[k].id); if (! (v >= 0.f && v <= 1.f)) ok = false; }
            lo = std::min (lo, val ("echo_repeats")); hi = std::max (hi, val ("echo_repeats"));
        }
        check (ok, "400 presses: every randomised control stays between 0 and 1");
        check (hi - lo > 0.7f, "with Rnd at full, Echo Repeats wanders over most of its range");
        check (std::abs (val ("filter_cutoff") - 0.37f) < 1e-4f, "Rnd 0 keeps a control exactly where it is");
        for (auto* prm : p.getParameters()) prm->removeListener (&gc);

        // the Randomise parameter (automation): off to on randomises once, and switches itself off
        IroxProcessor q;
        q.setRateAndBufferSizeDetails (48000.0, 256); q.prepareToPlay (48000.0, 256);
        for (int i = 0; i < n; ++i) { setParam (q, targets[i].id, 0.5f); setParam (q, targets[i].rndId, 0.2f); }
        juce::MidiBuffer midi; juce::AudioBuffer<float> buf (2, 256);
        auto before = [&] { std::vector<float> v; for (int i = 0; i < n; ++i) v.push_back (q.apvts.getRawParameterValue (targets[i].id)->load()); return v; };
        const auto v0 = before();
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRandomise();
        check (before() == v0, "nothing happens while Randomise stays off");
        setParam (q, "randomise", 1.0f);
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRandomise();
        const auto v1 = before();
        check (v1 != v0, "Randomise parameter off to on: the controls move");
        check (q.apvts.getRawParameterValue ("randomise")->load() < 0.5f, "the Randomise parameter switches itself off again");
        fillSignal (buf, 0); q.processBlock (buf, midi); q.runPendingRandomise();
        check (before() == v1, "and it does not randomise again on its own");
        // a project saved with Randomise on must not randomise when opened
        juce::MemoryBlock blk;
        { IroxProcessor src; setParam (src, "randomise", 1.0f); src.getStateInformation (blk); }
        IroxProcessor dst; dst.setRateAndBufferSizeDetails (48000.0, 256); dst.prepareToPlay (48000.0, 256);
        dst.setStateInformation (blk.getData(), (int) blk.getSize());
        std::vector<float> d0; for (int i = 0; i < n; ++i) d0.push_back (dst.apvts.getRawParameterValue (targets[i].id)->load());
        fillSignal (buf, 0); dst.processBlock (buf, midi); dst.runPendingRandomise();
        std::vector<float> d1; for (int i = 0; i < n; ++i) d1.push_back (dst.apvts.getRawParameterValue (targets[i].id)->load());
        check (d0 == d1 && dst.apvts.getRawParameterValue ("randomise")->load() < 0.5f, "opening a project does not randomise");
    }

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
