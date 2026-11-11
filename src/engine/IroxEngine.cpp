// The only file that includes the Iroi firmware. The firmware's hardware layer (Ui.h, ParamController.h, Midi.h,
// Led.h, InputDetector.h) is NOT used: on the module it turns knob and fader readings into the PatchCtrls values
// below. Here the plugin parameters are written into PatchCtrls directly, with the same arithmetic the firmware
// uses for the parts that matter to the sound (fader snapping to full, the filter cutoff note and its smoothing).
// The CV inputs are not copied (scope item 3), so every CV amount is 0 and every CV value is 0.
#include "IroxEngine.h"
#include "owl_runtime.h"
#include "Iroi.h"
#include "Clock.h"

namespace irox
{
struct Engine::Impl
{
    PatchCtrls ctrls {};
    PatchCvs cvs {};
    PatchState state {};
    EngineParams params;
    Iroi* iroi = nullptr;
    Clock* clock = nullptr;
    AudioBuffer* buffer = nullptr;
    float cutoffPot = 0.f;
    bool firstBlock = true;

    Impl()
    {
        irox_init_owl_runtime();
        irox_ZeroedAllocations zeroed;      // the firmware expects cleared memory, like OpenWare's allocator gives

        state.sampleRate = (float) kEngineSampleRate;
        state.blockSize = kEngineBlockSize;
        state.blockRate = (float) (kEngineSampleRate / kEngineBlockSize);   // 1500
        state.inputLevel = FloatArray::create (kEngineBlockSize);
        state.outputLevel = FloatArray::create (kEngineBlockSize);
        state.efModLevel = FloatArray::create (kEngineBlockSize);
        state.outLevel = 1.f;
        state.randomSlew = kRandomSlewSamples;
        state.randomHasSlew = false;
        state.modAttenuverters = false;
        state.cvAttenuverters = false;
        state.funcMode = FUNC_MODE_NONE;
        state.clockSource = CLOCK_SOURCE_INTERNAL;
        state.syncIn = false;

        // Same order as the firmware's patch: Iroi, then Clock (the Clock creates the tempo object).
        iroi = Iroi::create (&ctrls, &cvs, &state);
        clock = Clock::create (&ctrls, &state);
        buffer = AudioBuffer::create (2, kEngineBlockSize);

        applyParams();
    }

    ~Impl()
    {
        AudioBuffer::destroy (buffer);
        Iroi::destroy (iroi);
        Clock::destroy (clock);
        TapTempo::destroy (state.tempo);
        FloatArray::destroy (state.inputLevel);
        FloatArray::destroy (state.outputLevel);
        FloatArray::destroy (state.efModLevel);
    }

    static float fader (float v) { return v >= kOne ? 1.f : v; }   // as in Ui::Poll

    void applyParams()
    {
        const EngineParams& p = params;
        ctrls.filterVol = fader (p.filterLevel);
        ctrls.resonatorVol = fader (p.resonatorLevel);
        ctrls.echoVol = fader (p.echoLevel);
        ctrls.ambienceVol = fader (p.ambienceLevel);

        ctrls.filterMode = p.filterMode;
        ctrls.filterPosition = p.filterPosition;
        ctrls.filterResonance = p.filterResonance;
        ctrls.filterResonanceModAmount = p.filterResonanceMod;
        ctrls.filterNoiseLevel = 0.f;                    // not used by the firmware's engine

        ctrls.resonatorTune = p.resonatorTune;
        ctrls.resonatorTuneModAmount = p.resonatorTuneMod;
        ctrls.resonatorFeedback = p.resonatorFeedback;
        ctrls.resonatorFeedbackModAmount = p.resonatorFeedbackMod;
        ctrls.resonatorDissonance = p.resonatorDissonance;

        ctrls.echoRepeats = p.echoRepeats;
        ctrls.echoRepeatsModAmount = p.echoRepeatsMod;
        ctrls.echoDensity = p.echoDensity;
        ctrls.echoDensityModAmount = p.echoDensityMod;
        ctrls.echoFilter = p.echoFilter;

        ctrls.ambienceDecay = p.ambienceDecay;
        ctrls.ambienceDecayModAmount = p.ambienceDecayMod;
        ctrls.ambienceSpacetime = p.ambienceSpacetime;
        ctrls.ambienceSpacetimeModAmount = p.ambienceSpacetimeMod;
        ctrls.ambienceAutoPan = p.ambienceAutoPan;

        ctrls.modLevel = p.modLevel;
        ctrls.modSpeed = p.modSpeed;
        ctrls.modType = p.modType;

        ctrls.filterCutoffRndAmount = p.filterCutoffRnd;
        ctrls.filterResonanceRndAmount = p.filterResonanceRnd;
        ctrls.resonatorTuneRndAmount = p.resonatorTuneRnd;
        ctrls.resonatorFeedbackRndAmount = p.resonatorFeedbackRnd;
        ctrls.echoRepeatsRndAmount = p.echoRepeatsRnd;
        ctrls.echoDensityRndAmount = p.echoDensityRnd;
        ctrls.ambienceDecayRndAmount = p.ambienceDecayRnd;
        ctrls.ambienceSpacetimeRndAmount = p.ambienceSpacetimeRnd;
        ctrls.modLevelRndAmount = p.modLevelRnd;
        ctrls.modSpeedRndAmount = p.modSpeedRnd;

        // CV amounts stay 0 (CV jacks are replaced by host automation).
        ctrls.filterCutoffCvAmount = ctrls.filterResonanceCvAmount = 0.f;
        ctrls.resonatorTuneCvAmount = ctrls.resonatorFeedbackCvAmount = 0.f;
        ctrls.echoRepeatsCvAmount = ctrls.echoDensityCvAmount = 0.f;
        ctrls.ambienceDecayCvAmount = ctrls.ambienceSpacetimeCvAmount = 0.f;

        state.modActive = ctrls.modLevel > 0.1f;

        // Cutoff: the firmware turns the knob (plus the modulation) into a note number and smooths it per block
        // (Ui::Poll). The first block jumps straight to the value so the filter does not sweep in from note 0.
        float cutoff = Modulate (p.filterCutoff, p.filterCutoffMod, state.modValue, 0, 0, -1.f, 1.f, state.modAttenuverters);
        const float note = 12.f + 127.f * cutoff;
        if (firstBlock) cutoffPot = note;
        else cutoffPot += 0.1f * (note - cutoffPot);
        ctrls.filterCutoff = cutoffPot;
    }

    void process (float* left, float* right)
    {
        applyParams();
        firstBlock = false;
        clock->Process();

        FloatArray l = buffer->getSamples (LEFT_CHANNEL);
        FloatArray r = buffer->getSamples (RIGHT_CHANNEL);
        for (int i = 0; i < kEngineBlockSize; ++i) { l[i] = left[i]; r[i] = right[i]; }
        iroi->Process (*buffer);
        for (int i = 0; i < kEngineBlockSize; ++i) { left[i] = l[i]; right[i] = r[i]; }
    }
};

Engine::Engine() : impl (new Impl) {}
Engine::~Engine() = default;
void Engine::setParams (const EngineParams& p) { impl->params = p; }
void Engine::processBlock (float* left, float* right) { impl->process (left, right); }
float Engine::modulationValue() const { return impl->state.modValue; }
}
