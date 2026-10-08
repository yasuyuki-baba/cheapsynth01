#pragma once

#include "Parameters.h"

// MIDI writes the standard AudioParameterFloat's atomic value without listeners.
// Read that authoritative value for DSP, rather than APVTS's deferred raw cache.
// All MIDI-mapped parameters in the production layout are AudioParameterFloat.
inline float getMidiParameterValue(juce::AudioProcessorValueTreeState& state,
                                   const juce::String& id) {
    return static_cast<juce::AudioParameterFloat*>(state.getParameter(id))->get();
}

// Standard parameter atomic storage is authoritative before the UI notification tick.
inline float getCurrentParameterValue(juce::AudioProcessorValueTreeState& state,
                                      const juce::String& id) {
    auto* parameter = state.getParameter(id);
    if (auto* value = dynamic_cast<juce::AudioParameterFloat*>(parameter))
        return value->get();
    if (auto* value = dynamic_cast<juce::AudioParameterInt*>(parameter))
        return static_cast<float>(value->get());
    if (auto* value = dynamic_cast<juce::AudioParameterChoice*>(parameter))
        return static_cast<float>(value->getIndex());
    return parameter->convertFrom0to1(parameter->getValue());
}

// State capture may precede the next notification tick. Snapshot current values
// into the copy so MIDI/program-edited patches/sessions remain immediately saveable.
// Like APVTS::copyState, this is a non-realtime operation.
inline juce::ValueTree copyCurrentMidiParameterState(juce::AudioProcessorValueTreeState& state) {
    auto snapshot = state.copyState();
    for (auto child : snapshot) {
        const auto id = child.getProperty("id").toString();
        if (auto* parameter = state.getParameter(id))
            child.setProperty("value", parameter->convertFrom0to1(parameter->getValue()), nullptr);
    }
    return snapshot;
}
