/*
  ==============================================================================

    CS01OscilloscopeComponent.cpp
    Created: 23 Jun 2025
    Author:  JUCE

  ==============================================================================
*/

#include "CS01OscilloscopeComponent.h"

//==============================================================================
CS01OscilloscopeComponent::CS01OscilloscopeComponent(int initialNumChannels)
    : bufferSize(1024),
      bufferIndex(0),
      numChannels(initialNumChannels),
      waveformColour(juce::Colours::lime),
      backgroundColour(juce::Colours::black),
      gridColour(juce::Colours::darkgrey.withAlpha(0.5f)),
      waveformThickness(1.5f)
{
    // Initialize audio buffer
    audioDataBuffer.setSize(numChannels, bufferSize);
    audioDataBuffer.clear();
    
    // Initialize waveform paths
    waveformPaths.resize(numChannels);
    
    // Start timer for periodic updates
    startTimer(50); // Update every 50ms (20Hz)
}

CS01OscilloscopeComponent::~CS01OscilloscopeComponent()
{
    stopTimer();
}

void CS01OscilloscopeComponent::paint(juce::Graphics& g)
{
    // Draw background
    g.fillAll(backgroundColour);
    
    // Draw grid lines
    drawGrid(g);
    
    // Draw waveforms for all channels
    g.setColour(waveformColour);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        g.strokePath(waveformPaths[ch], juce::PathStrokeType(waveformThickness));
    }
}

void CS01OscilloscopeComponent::resized()
{
    // Update waveform path when component size changes
    updateWaveformPath();
}

void CS01OscilloscopeComponent::setNumChannels(int newNumChannels)
{
    if (numChannels != newNumChannels)
    {
        const juce::ScopedLock lock(mutex);
        numChannels = newNumChannels;
        audioDataBuffer.setSize(numChannels, bufferSize, true, true, true);
        waveformPaths.resize(numChannels);
    }
}

void CS01OscilloscopeComponent::pushBuffer(const juce::AudioBuffer<float>& buffer)
{
    const juce::ScopedLock lock(mutex);
    
    // Use the smaller of buffer channels and configured channels
    const int numChannelsToPush = juce::jmin(buffer.getNumChannels(), numChannels);
    
    // Copy data from new buffer for channels to push
    int numSamples = buffer.getNumSamples();
    
    // Process all channels for each sample before moving to the next sample
    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannelsToPush; ++ch)
        {
            audioDataBuffer.setSample(ch, bufferIndex, buffer.getSample(ch, i));
        }
        // Increment buffer index after processing all channels for this sample
        bufferIndex = (bufferIndex + 1) % bufferSize;
    }
}

void CS01OscilloscopeComponent::setWaveformColour(juce::Colour newColour)
{
    waveformColour = newColour;
    repaint();
}

void CS01OscilloscopeComponent::setBackgroundColour(juce::Colour newColour)
{
    backgroundColour = newColour;
    repaint();
}

void CS01OscilloscopeComponent::setGridColour(juce::Colour newColour)
{
    gridColour = newColour;
    repaint();
}

void CS01OscilloscopeComponent::setWaveformThickness(float newThickness)
{
    waveformThickness = newThickness;
    repaint();
}

void CS01OscilloscopeComponent::setUpdateRate(int rateMs)
{
    stopTimer();
    startTimer(rateMs);
}

void CS01OscilloscopeComponent::setBufferSize(int newBufferSize)
{
    if (bufferSize != newBufferSize)
    {
        const juce::ScopedLock lock(mutex);
        bufferSize = newBufferSize;
        audioDataBuffer.setSize(numChannels, bufferSize, true, true, true);
    }
}

void CS01OscilloscopeComponent::timerCallback()
{
    updateWaveformPath();
    repaint();
}

void CS01OscilloscopeComponent::updateWaveformPath()
{
    const juce::ScopedLock lock(mutex);
    
    // Get component size
    float width = static_cast<float>(getWidth());
    float height = static_cast<float>(getHeight());
    
    // Calculate height per channel
    float channelHeight = height / static_cast<float>(numChannels);
    
    // Create waveform paths for each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Clear this channel's waveform path
        waveformPaths[ch].clear();
        
        // Calculate center Y position for this channel
        float centerY = channelHeight * (ch + 0.5f);
        
        // Waveform amplitude scale (40% of channel height)
        float verticalScale = channelHeight * 0.4f;
        
        bool pathStarted = false;
        
        for (int i = 0; i < bufferSize; ++i)
        {
            // Draw sequentially from current position in buffer
            int index = (bufferIndex + i) % bufferSize;
            float x = (static_cast<float>(i) / static_cast<float>(bufferSize)) * width;
            float y = centerY - (audioDataBuffer.getSample(ch, index) * verticalScale);
            
            if (!pathStarted)
            {
                waveformPaths[ch].startNewSubPath(x, y);
                pathStarted = true;
            }
            else
            {
                waveformPaths[ch].lineTo(x, y);
            }
        }
    }
}

void CS01OscilloscopeComponent::drawGrid(juce::Graphics& g)
{
    g.setColour(gridColour);
    
    float width = static_cast<float>(getWidth());
    float height = static_cast<float>(getHeight());
    float channelHeight = height / static_cast<float>(numChannels);
    
    // Draw grid for each channel
    for (int ch = 0; ch < numChannels; ++ch)
    {
        // Calculate top and bottom positions for this channel
        float top = channelHeight * ch;
        float bottom = channelHeight * (ch + 1);
        float centerY = (top + bottom) * 0.5f;
        
        // Horizontal line (center of this channel)
        g.drawLine(0.0f, centerY, width, centerY, 1.0f);
        
        // Vertical line (center)
        g.drawLine(width / 2.0f, top, width / 2.0f, bottom, 1.0f);
        
        // Additional horizontal lines (quarter divisions)
        float quarterHeight = channelHeight / 4.0f;
        g.drawLine(0.0f, centerY - quarterHeight, width, centerY - quarterHeight, 0.5f);
        g.drawLine(0.0f, centerY + quarterHeight, width, centerY + quarterHeight, 0.5f);
        
        // Additional vertical lines
        for (int i = 1; i <= 4; ++i)
        {
            float x = width / 8.0f * i;
            g.drawLine(x, top, x, bottom, 0.5f);
            g.drawLine(width - x, top, width - x, bottom, 0.5f);
        }
        
        // Draw channel divider lines (except for the last channel)
        if (ch < numChannels - 1)
        {
            g.drawLine(0.0f, bottom, width, bottom, 0.5f);
        }
    }
}
