#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "IroxEngine.h"

// Parameter ids. They are fixed from Milestone 1 on: saved Logic projects and automation refer to them.
namespace irox_params
{
struct Def { const char* id; const char* name; float def; const char* section; };

// All 0..1 parameters, in the order they appear in the host's list. "Mod" = how far the internal modulation moves the
// parameter, "Rnd" = how far the randomiser (Milestone 2) moves it.
inline const Def* floatParams (int& count)
{
    static const Def defs[] = {
        { "filter_level", "Filter Level", 1.0f, "Filter" },
        { "filter_cutoff", "Filter Cutoff", 1.0f, "Filter" },
        { "filter_cutoff_mod", "Filter Cutoff Mod", 0.5f, "Filter" },
        { "filter_cutoff_rnd", "Filter Cutoff Rnd", 0.5f, "Filter" },
        { "filter_resonance", "Filter Resonance", 0.0f, "Filter" },
        { "filter_resonance_mod", "Filter Resonance Mod", 0.0f, "Filter" },
        { "filter_resonance_rnd", "Filter Resonance Rnd", 0.5f, "Filter" },
        { "resonator_level", "Resonator Level", 0.0f, "Resonator" },
        { "resonator_tune", "Resonator Tune", 0.5f, "Resonator" },
        { "resonator_tune_mod", "Resonator Tune Mod", 0.0f, "Resonator" },
        { "resonator_tune_rnd", "Resonator Tune Rnd", 0.5f, "Resonator" },
        { "resonator_feedback", "Resonator Feedback", 0.5f, "Resonator" },
        { "resonator_feedback_mod", "Resonator Feedback Mod", 0.0f, "Resonator" },
        { "resonator_feedback_rnd", "Resonator Feedback Rnd", 0.5f, "Resonator" },
        { "resonator_dissonance", "Resonator Dissonance", 0.0f, "Resonator" },
        { "echo_level", "Echo Level", 0.0f, "Echo" },
        { "echo_repeats", "Echo Repeats", 0.5f, "Echo" },
        { "echo_repeats_mod", "Echo Repeats Mod", 0.0f, "Echo" },
        { "echo_repeats_rnd", "Echo Repeats Rnd", 0.5f, "Echo" },
        { "echo_density", "Echo Density", 0.5f, "Echo" },
        { "echo_density_mod", "Echo Density Mod", 0.0f, "Echo" },
        { "echo_density_rnd", "Echo Density Rnd", 0.5f, "Echo" },
        { "echo_filter", "Echo Filter", 0.5f, "Echo" },
        { "ambience_level", "Ambience Level", 0.0f, "Ambience" },
        { "ambience_decay", "Ambience Decay", 0.5f, "Ambience" },
        { "ambience_decay_mod", "Ambience Decay Mod", 0.0f, "Ambience" },
        { "ambience_decay_rnd", "Ambience Decay Rnd", 0.5f, "Ambience" },
        { "ambience_spacetime", "Ambience Spacetime", 0.5f, "Ambience" },
        { "ambience_spacetime_mod", "Ambience Spacetime Mod", 0.0f, "Ambience" },
        { "ambience_spacetime_rnd", "Ambience Spacetime Rnd", 0.5f, "Ambience" },
        { "ambience_autopan", "Ambience Auto-pan", 0.0f, "Ambience" },
        { "mod_level", "Modulation Level", 0.0f, "Modulation" },
        { "mod_level_rnd", "Modulation Level Rnd", 0.5f, "Modulation" },
        { "mod_speed", "Modulation Speed", 0.5f, "Modulation" },
        { "mod_speed_rnd", "Modulation Speed Rnd", 0.5f, "Modulation" },
        { "mod_type", "Modulation Type", 0.0f, "Modulation" },
    };
    count = (int) (sizeof (defs) / sizeof (defs[0]));
    return defs;
}
inline const char* const* modeNames() { static const char* n[] = { "Low pass", "Band pass", "High pass", "Comb" }; return n; }
inline const char* const* modTypeNames() { static const char* n[] = { "Chaos", "Sine", "Ramp", "Saw", "Square", "Random", "Envelope" }; return n; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    int n = 0; const Def* d = floatParams (n);
    // the choice parameters sit with their section; ids are what matter
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "filter_mode", 1 }, "Filter Mode", StringArray { "Low pass", "Band pass", "High pass", "Comb" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "filter_position", 1 }, "Filter Position", StringArray { "1", "2", "3", "4" }, 0));
    for (int i = 0; i < n; ++i)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { d[i].id, 1 }, d[i].name, NormalisableRange<float> (0.f, 1.f), d[i].def));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "input_level", 1 }, "Input Level", NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output_level", 1 }, "Output Level", NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

inline irox::EngineParams toEngine (const juce::AudioProcessorValueTreeState& s)
{
    auto v = [&] (const char* id) { return s.getRawParameterValue (id)->load(); };
    auto choice = [&] (const char* id) { return (v (id) + 0.5f) / 4.f; };   // 4 choices -> the middle of each quarter
    irox::EngineParams p;
    p.filterLevel = v ("filter_level"); p.filterMode = choice ("filter_mode"); p.filterCutoff = v ("filter_cutoff");
    p.filterCutoffMod = v ("filter_cutoff_mod"); p.filterResonance = v ("filter_resonance"); p.filterResonanceMod = v ("filter_resonance_mod");
    p.filterPosition = choice ("filter_position");
    p.resonatorLevel = v ("resonator_level"); p.resonatorTune = v ("resonator_tune"); p.resonatorTuneMod = v ("resonator_tune_mod");
    p.resonatorFeedback = v ("resonator_feedback"); p.resonatorFeedbackMod = v ("resonator_feedback_mod"); p.resonatorDissonance = v ("resonator_dissonance");
    p.echoLevel = v ("echo_level"); p.echoRepeats = v ("echo_repeats"); p.echoRepeatsMod = v ("echo_repeats_mod");
    p.echoDensity = v ("echo_density"); p.echoDensityMod = v ("echo_density_mod"); p.echoFilter = v ("echo_filter");
    p.ambienceLevel = v ("ambience_level"); p.ambienceDecay = v ("ambience_decay"); p.ambienceDecayMod = v ("ambience_decay_mod");
    p.ambienceSpacetime = v ("ambience_spacetime"); p.ambienceSpacetimeMod = v ("ambience_spacetime_mod"); p.ambienceAutoPan = v ("ambience_autopan");
    p.modLevel = v ("mod_level"); p.modSpeed = v ("mod_speed"); p.modType = v ("mod_type");
    p.filterCutoffRnd = v ("filter_cutoff_rnd"); p.filterResonanceRnd = v ("filter_resonance_rnd"); p.resonatorTuneRnd = v ("resonator_tune_rnd");
    p.resonatorFeedbackRnd = v ("resonator_feedback_rnd"); p.echoRepeatsRnd = v ("echo_repeats_rnd"); p.echoDensityRnd = v ("echo_density_rnd");
    p.ambienceDecayRnd = v ("ambience_decay_rnd"); p.ambienceSpacetimeRnd = v ("ambience_spacetime_rnd");
    p.modLevelRnd = v ("mod_level_rnd"); p.modSpeedRnd = v ("mod_speed_rnd");
    return p;
}
}
