#pragma once

#include "Parameters.h"

// MIDI writes the standard AudioParameterFloat's atomic value without listeners.
// Read that authoritative value for DSP, rather than APVTS's deferred raw cache.
// All MIDI-mapped parameters in the production layout are AudioParameterFloat.
inline float getMidiParameterValue(juce::AudioProcessorValueTreeState& state,
                                   const juce::String& id) {
    return static_cast<juce::AudioParameterFloat*>(state.getParameter(id))->get();
}

// State capture may precede the next notification tick. Snapshot current float
// values into the copy so MIDI-edited patches/sessions remain immediately saveable.
// Like APVTS::copyState, this is a non-realtime operation.
inline juce::ValueTree copyCurrentMidiParameterState(juce::AudioProcessorValueTreeState& state) {
    auto snapshot = state.copyState();
    for (auto child : snapshot) {
        const auto id = child.getProperty("id").toString();
        if (auto* parameter = dynamic_cast<juce::AudioParameterFloat*>(state.getParameter(id)))
            child.setProperty("value", parameter->get(), nullptr);
    }
    return snapshot;
}
