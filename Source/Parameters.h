#pragma once
#include <JuceHeader.h>

namespace ParameterIds
{
    const juce::String feet { "FEET" };
    const juce::String waveType { "WAVE_TYPE" };
    const juce::String lfoTarget { "LFO_TARGET" };
    const juce::String pitch { "PITCH" };
    const juce::String glissando { "GLISSANDO" };
    const juce::String pwmSpeed { "PWM_SPEED" };
    const juce::String pitchBend { "PITCH_BEND" };
    const juce::String cutoff { "CUTOFF" };
    const juce::String resonance { "RESONANCE" };
    const juce::String vcfEgDepth { "VCF_EG_DEPTH" };
    const juce::String vcaEgDepth { "VCA_EG_DEPTH" };
    const juce::String lfoSpeed { "LFO_SPEED" };
    const juce::String modDepth { "MOD_DEPTH" };
    const juce::String attack { "ATTACK" };
    const juce::String decay { "DECAY" };
    const juce::String sustain { "SUSTAIN" };
    const juce::String release { "RELEASE" };
    const juce::String breathVcf { "BREATH_VCF" };
    const juce::String breathVca { "BREATH_VCA" };
    const juce::String volume { "VOLUME" };
    const juce::String breathInput { "BREATH_INPUT" };
    const juce::String pitchBendUpRange { "PITCH_BEND_UP_RANGE" };
    const juce::String pitchBendDownRange { "PITCH_BEND_DOWN_RANGE" };
    const juce::String filterType { "FILTER_TYPE" }; // Filter type (MODERN/CS01)
}
