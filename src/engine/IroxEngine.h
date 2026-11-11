// Iro-x engine wrapper. No JUCE and no firmware headers here, so the plugin code stays clean and the engine can be
// tested on its own (tools/engine_test).
//
// The Iroi firmware was written for a module that runs at 48 kHz in blocks of 32 samples (1500 blocks per second).
// Many of its numbers are tied to that (delay line lengths, the modulation clock, the filter smoothing), so the
// engine always runs exactly like that. IroxStream (IroxStream.h) puts a resampler and a block FIFO around it so the
// plugin works at any host sample rate and block size.
#pragma once
#include <memory>

namespace irox
{
constexpr int kEngineBlockSize = 32;
constexpr double kEngineSampleRate = 48000.0;

// Every value is what a knob or fader of the module would report: 0 to 1.
struct EngineParams
{
    // Filter + VCA
    float filterLevel = 1.0f;            // fader (the VCA after the filter)
    float filterMode = 0.125f;           // 0..0.25 low pass, to 0.5 band pass, to 0.75 high pass, above comb
    float filterCutoff = 1.0f;
    float filterCutoffMod = 0.5f;
    float filterResonance = 0.0f;
    float filterResonanceMod = 0.0f;
    float filterPosition = 0.0f;         // 4 positions in the chain: 0..0.25, 0.25..0.5, 0.5..0.75, 0.75..1

    // Resonator
    float resonatorLevel = 0.0f;         // dry / wet
    float resonatorTune = 0.5f;
    float resonatorTuneMod = 0.0f;
    float resonatorFeedback = 0.5f;
    float resonatorFeedbackMod = 0.0f;
    float resonatorDissonance = 0.0f;

    // Echo
    float echoLevel = 0.0f;              // dry / wet
    float echoRepeats = 0.5f;
    float echoRepeatsMod = 0.0f;
    float echoDensity = 0.5f;
    float echoDensityMod = 0.0f;
    float echoFilter = 0.5f;             // 0.5 = off, below low pass, above high pass

    // Ambience
    float ambienceLevel = 0.0f;          // dry / wet
    float ambienceDecay = 0.5f;
    float ambienceDecayMod = 0.0f;
    float ambienceSpacetime = 0.5f;
    float ambienceSpacetimeMod = 0.0f;
    float ambienceAutoPan = 0.0f;

    // Modulation
    float modLevel = 0.0f;
    float modSpeed = 0.5f;
    float modType = 0.0f;                // 0 chaos, 1/6 sine, 2/6 ramp, 3/6 saw, 4/6 square, 5/6 random, 1 envelope

    // Random amounts (how far the randomiser button moves each control; used by randomiseParameters in Parameters.h,
    // they do not change the sound on their own)
    float filterCutoffRnd = 0.5f, filterResonanceRnd = 0.5f, resonatorTuneRnd = 0.5f, resonatorFeedbackRnd = 0.5f;
    float echoRepeatsRnd = 0.5f, echoDensityRnd = 0.5f, ambienceDecayRnd = 0.5f, ambienceSpacetimeRnd = 0.5f;
    float modLevelRnd = 0.5f, modSpeedRnd = 0.5f;
};

// What the host (Logic) says about tempo and transport. bpm <= 0 means "no tempo known": the engine then runs on the
// firmware's own internal clock (60 BPM), as the module does with nothing patched into its sync input.
struct Transport
{
    double bpm = 0.0;
    bool hasPpq = false;       // ppq = position in quarter notes at the start of the host block
    double ppq = 0.0;
    bool playing = false;
};

// One Iroi effects engine. Not thread safe: one thread at a time per object. Every object has its own state, so
// any number of them can run at once (different Logic tracks).
class Engine
{
public:
    Engine();
    ~Engine();
    Engine (const Engine&) = delete;
    Engine& operator= (const Engine&) = delete;

    void setParams (const EngineParams& p);
    // Logic's tempo and transport. Replaces the module's sync jack: one clock pulse per quarter note (see IroxEngine.cpp).
    void setTransport (const Transport& t);
    // Processes exactly kEngineBlockSize samples in place (48 kHz).
    void processBlock (float* left, float* right);

    // The current modulation value (about -1..1), for the window.
    float modulationValue() const;
    // Position inside the current quarter note (0..1) of the clock that follows Logic's tempo; for tests.
    double beatPhase() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
