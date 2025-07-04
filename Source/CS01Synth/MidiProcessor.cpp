#include "MidiProcessor.h"
#include "../Parameters.h"

MidiProcessor::MidiProcessor(juce::AudioProcessorValueTreeState& apvts)
    : AudioProcessor(BusesProperties()), // No audio buses
      apvts(apvts)
{
}

MidiProcessor::~MidiProcessor() = default;

void MidiProcessor::prepareToPlay(double, int) {}
void MidiProcessor::releaseResources() {}

void MidiProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // This processor does not process audio, so we must clear the buffer
    // to prevent any leftover data from passing through.
    buffer.clear();

    // Process MIDI messages but don't generate output buffer
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        handleMidiEvent(message, midiMessages);
    }
    
    // Clear MIDI buffer as we don't generate output MIDI messages
    midiMessages.clear();
}

void MidiProcessor::handleMidiEvent(const juce::MidiMessage& midiMessage, juce::MidiBuffer&)
{
    if (midiMessage.isNoteOn())
    {
        handleNoteOn(midiMessage);
    }
    else if (midiMessage.isNoteOff())
    {
        handleNoteOff(midiMessage);
    }
    else if (midiMessage.isPitchWheel())
    {
        handlePitchWheel(midiMessage);
    }
    else if (midiMessage.isController())
    {
        handleControllerMessage(midiMessage);
    }
    // Ignore other MIDI messages
}

void MidiProcessor::handleNoteOn(const juce::MidiMessage& midiMessage)
{
    bool wasEmpty = activeNotes.isEmpty();
    activeNotes.addIfNotAlreadyThere(midiMessage.getNoteNumber());
    activeNotes.sort();
    
    int highestNote = activeNotes.getLast();
    float velocity = midiMessage.getVelocity() / 127.0f;

    if (soundGenerator != nullptr)
    {
        if (wasEmpty)
        {
            soundGenerator->startNote(highestNote, velocity, lastPitchWheelValue);
            
            // Call EG note-on only for the first note
            if (egProcessor != nullptr)
            {
                egProcessor->startEnvelope();
            }
        }
        else
        {
            soundGenerator->changeNote(highestNote);
        }
    }
}

void MidiProcessor::handleNoteOff(const juce::MidiMessage& midiMessage)
{
    activeNotes.removeFirstMatchingValue(midiMessage.getNoteNumber());
    
    if (soundGenerator != nullptr)
    {
        if (activeNotes.isEmpty())
        {
            // Always set allowTailOff = true to make sound fade gradually
            soundGenerator->stopNote(true);
            
            // Start EG release only when all notes are off
            if (egProcessor != nullptr)
            {
                egProcessor->releaseEnvelope();
            }
        }
        else
        {
            activeNotes.sort();
            int highestNote = activeNotes.getLast();
            soundGenerator->changeNote(highestNote);
        }
    }
}

void MidiProcessor::handlePitchWheel(const juce::MidiMessage& midiMessage)
{
    lastPitchWheelValue = midiMessage.getPitchWheelValue();
    
    // Set pitch wheel value to sound generator
    if (soundGenerator != nullptr)
    {
        soundGenerator->pitchWheelMoved(lastPitchWheelValue);
    }
    
    // Also set pitch bend value to parameter
    const float bend = (lastPitchWheelValue - 8192) / 8192.0f;
    if (auto* param = apvts.getParameter(ParameterIds::pitchBend))
        param->setValueNotifyingHost(bend);
}

void MidiProcessor::handleControllerMessage(const juce::MidiMessage& midiMessage)
{
    const int controller = midiMessage.getControllerNumber();
    const float value = midiMessage.getControllerValue() / 127.0f;

    if (controller == 1) // CC #1: Modulation Wheel
    {
        if (auto* param = apvts.getParameter(ParameterIds::modDepth))
            param->setValueNotifyingHost(value);
    }
    else if (controller == 2) // CC #2: Breath Controller
    {
        if (auto* param = apvts.getParameter(ParameterIds::breathInput))
            param->setValueNotifyingHost(value);
    }
}
