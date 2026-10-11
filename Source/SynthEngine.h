#pragma once
#include "CS01Synth/VCOProcessor.h"
#include "CS01Synth/EGProcessor.h"
#include "CS01Synth/LFOProcessor.h"
#include "CS01Synth/MidiProcessor.h"
#include "CS01Synth/OriginalVCFProcessor.h"
#include "CS01Synth/ModernVCFProcessor.h"
#include "CS01Synth/VCAProcessor.h"
#include "DSP/Primitives.h"
#include "DSP/NoteOwnership.h"

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
        return midi.getCurrentlyPlayingNote();
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
