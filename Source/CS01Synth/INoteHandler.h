#pragma once

/**
 * INoteHandler - Interface for handling MIDI note events
 * 
 * This interface defines methods for handling note on/off events,
 * pitch wheel movements, and other MIDI-related functionality.
 */
class INoteHandler
{
public:
    virtual ~INoteHandler() = default;
    
    // Note handling methods
    virtual void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) = 0;
    virtual void stopNote(bool allowTailOff) = 0;
    virtual void changeNote(int midiNoteNumber) = 0;
    virtual void pitchWheelMoved(int newPitchWheelValue) = 0;
    virtual bool isActive() const = 0;
    virtual int getCurrentlyPlayingNote() const = 0;
};
