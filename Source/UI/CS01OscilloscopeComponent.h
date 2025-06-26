/*
  ==============================================================================

    CS01OscilloscopeComponent.h
    Created: 23 Jun 2025
    Author:  JUCE

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

//==============================================================================
/**
 * Component for displaying waveforms in an oscilloscope style
 */
class CS01OscilloscopeComponent : public juce::Component,
                                  private juce::Timer
{
public:
    CS01OscilloscopeComponent(int initialNumChannels = 1);
    ~CS01OscilloscopeComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    /**
     * Add a new audio buffer
     * @param buffer Audio buffer to display
     */
    void pushBuffer(const juce::AudioBuffer<float>& buffer);
    
    /**
     * Set the waveform color
     * @param newColour New waveform color
     */
    void setWaveformColour(juce::Colour newColour);
    
    /**
     * Set the background color
     * @param newColour New background color
     */
    void setBackgroundColour(juce::Colour newColour);
    
    /**
     * Set the grid line color
     * @param newColour New grid line color
     */
    void setGridColour(juce::Colour newColour);
    
    /**
     * Set the waveform thickness
     * @param newThickness New waveform thickness
     */
    void setWaveformThickness(float newThickness);
    
    /**
     * Set the update rate (in milliseconds)
     * @param rateMs Update rate (in milliseconds)
     */
    void setUpdateRate(int rateMs);
    
    /**
     * Set the buffer size
     * @param newBufferSize New buffer size
     */
    void setBufferSize(int newBufferSize);
    
    /**
     * Set the number of channels to display
     * @param newNumChannels New number of channels
     */
    void setNumChannels(int newNumChannels);
    
    /**
     * Get the current number of channels
     * @return Current number of channels
     */
    int getNumChannels() const { return numChannels; }

private:
    void timerCallback() override;
    void updateWaveformPath();
    void drawGrid(juce::Graphics& g);
    
    // Circular buffer to hold waveform data
    juce::AudioBuffer<float> audioDataBuffer;
    int bufferSize;
    int bufferIndex;
    int numChannels;  // Number of channels to display
    
    // Display parameters
    juce::Colour waveformColour;
    juce::Colour backgroundColour;
    juce::Colour gridColour;
    float waveformThickness;
    
    // Paths for waveform display (one per channel)
    std::vector<juce::Path> waveformPaths;
    
    // Synchronization object
    juce::CriticalSection mutex;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CS01OscilloscopeComponent)
};
