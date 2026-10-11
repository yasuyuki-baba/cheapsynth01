#pragma once
#include "cs01synth/VCOProcessor.h"
#include "cs01synth/EGProcessor.h"
#include "cs01synth/LFOProcessor.h"
#include "cs01synth/MidiProcessor.h"
#include "cs01synth/OriginalVCFProcessor.h"
#include "cs01synth/ModernVCFProcessor.h"
#include "cs01synth/VCAProcessor.h"
#include "dsp/Primitives.h"
#include "dsp/NoteOwnership.h"

// Audio-thread owned engine; parameter values are shared through lock-free atomics.
class SynthEngine {
   public:
    explicit SynthEngine(cs01::ParameterState& parameters);
    void prepare(double sampleRate);
    void panic();
    void handleMidi(const cs01::MidiMessage& event);
    void handleUiMidi(const cs01::MidiMessage& event);
    const std::bitset<128>& externalNotes() const {
        return noteOwnership.externalNotes();
    }
    float renderSample();
    int currentNote() const {
        return midi.currentNote();
    }
    static constexpr int latency = cs01::OutputConverter::latency;

   private:
    void applyMidi(const cs01::MidiMessage& event);
    cs01::NoteOwnership noteOwnership;
    cs01::ParameterState& parameters;
    VCOProcessor vco;
    EGProcessor eg;
    LFOProcessor lfo;
    MidiProcessor midi;
    OriginalVCFProcessor original;
    ModernVCFProcessor modern;
    VCAProcessor vca;
    cs01::OutputConverter converter;
};
