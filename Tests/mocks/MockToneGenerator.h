#pragma once

#include <JuceHeader.h>
#include "MockOscillator.h"
#include "../../Source/CS01Synth/ToneGenerator.h"
#include "../../Source/CS01Synth/ISoundGenerator.h"
#include "../../Source/Parameters.h"

namespace testing
{
    /**
     * Mock implementation of ToneGenerator for testing
     * This uses MockOscillator internally to avoid timer-related issues in tests
     */
    class MockToneGenerator : public ISoundGenerator
    {
    public:
        MockToneGenerator(juce::AudioProcessorValueTreeState& apvts)
            : parameters(apvts)
        {
            // Try to initialize parameters using the actual parameter IDs
            // If not found, try the test parameter IDs
            
            // Wave type parameter
            waveTypeParam = parameters.getRawParameterValue(ParameterIds::waveType);
            if (waveTypeParam == nullptr || !waveTypeParam->load())
                waveTypeParam = parameters.getRawParameterValue("vco_waveform");
                
            // Feet/octave parameter
            feetParam = parameters.getRawParameterValue(ParameterIds::feet);
            if (feetParam == nullptr || !feetParam->load())
                feetParam = parameters.getRawParameterValue("vco_octave");
                
            // Other parameters
            modDepthParam = parameters.getRawParameterValue(ParameterIds::modDepth);
            glissandoParam = parameters.getRawParameterValue(ParameterIds::glissando);
            releaseParam = parameters.getRawParameterValue(ParameterIds::release);
            pitchBendUpRangeParam = parameters.getRawParameterValue(ParameterIds::pitchBendUpRange);
            pitchBendDownRangeParam = parameters.getRawParameterValue(ParameterIds::pitchBendDownRange);
            pitchBendParam = parameters.getRawParameterValue(ParameterIds::pitchBend);
            pitchParam = parameters.getRawParameterValue(ParameterIds::pitch);
            pwmSpeedParam = parameters.getRawParameterValue(ParameterIds::pwmSpeed);
        }
        
        // Prepare for processing
        void prepare(const juce::dsp::ProcessSpec& spec) override
        {
            oscillator.prepare(spec);
            sampleRate = spec.sampleRate;
        }
        
        // Reset state
        void reset()
        {
            oscillator.reset();
            noteOn = false;
            tailOff = false;
            currentlyPlayingNote = 0;
            currentPitch = 0.0f;
            targetPitch = 0.0f;
            isSliding = false;
            tailOffCounter = 0;
        }
        
        // Start playing a note
        void startNote(int midiNoteNumber, float velocity, int currentPitchWheelPosition) override
        {
            currentlyPlayingNote = midiNoteNumber;
            noteOn = true;
            tailOff = false;
            tailOffCounter = 0;
            
            // Set pitch
            targetPitch = static_cast<float>(midiNoteNumber);
            
            // Handle glissando
            if (isActive() && glissandoParam != nullptr && *glissandoParam > 0.5f)
            {
                isSliding = true;
            }
            else
            {
                currentPitch = targetPitch;
                isSliding = false;
            }
            
            // Process pitch wheel
            pitchWheelMoved(currentPitchWheelPosition);
            
            // Update frequency
            updateFrequency();
        }
        
        // Change to a different note
        void changeNote(int midiNoteNumber) override
        {
            currentlyPlayingNote = midiNoteNumber;
            
            // Set pitch
            targetPitch = static_cast<float>(midiNoteNumber);
            
            // Handle glissando
            if (glissandoParam != nullptr && *glissandoParam > 0.5f)
            {
                isSliding = true;
            }
            else
            {
                currentPitch = targetPitch;
                isSliding = false;
            }
            
            // Update frequency
            updateFrequency();
        }
        
        // Stop playing a note
        void stopNote(bool allowTailOff) override
        {
            if (allowTailOff)
            {
                tailOff = true;
                tailOffCounter = 0;
                
                // Get release time from parameter
                if (releaseParam != nullptr)
                {
                    float releaseSecs = *releaseParam;
                    tailOffDuration = static_cast<int>(releaseSecs * sampleRate);
                    if (tailOffDuration < 1) tailOffDuration = 1;
                }
            }
            else
            {
                noteOn = false;
                tailOff = false;
                currentlyPlayingNote = 0;
            }
        }
        
        // Process pitch bend
        void pitchWheelMoved(int newPitchWheelValue) override
        {
            // Map pitch wheel to semitones using the actual parameters
            float upRange = 2.0f;
            float downRange = 2.0f;
            
            if (pitchBendUpRangeParam != nullptr)
                upRange = pitchBendUpRangeParam->load();
                
            if (pitchBendDownRangeParam != nullptr)
                downRange = pitchBendDownRangeParam->load();
            
            float bendValue = juce::jmap(static_cast<float>(newPitchWheelValue), 0.0f, 16383.0f, -1.0f, 1.0f);
            
            if (bendValue > 0)
                pitchBendSemitones = bendValue * upRange;
            else
                pitchBendSemitones = bendValue * downRange;
                
            updateFrequency();
        }
        
        // Check if the generator is active
        bool isActive() const override
        {
            return noteOn || tailOff;
        }
        
        // Get the currently playing note
        int getCurrentlyPlayingNote() const override
        {
            return currentlyPlayingNote;
        }
        
        // Get the next sample
        float getNextSample()
        {
            // Return 0 if note is off and there's no tail-off
            if (!noteOn && !tailOff)
            {
                return 0.0f;
            }
            
            // Process glissando
            if (isSliding)
            {
                // Get glissando time from parameter
                float glissandoTime = 0.0f;
                if (glissandoParam != nullptr)
                    glissandoTime = glissandoParam->load();
                
                if (glissandoTime < 0.001f) // No slide
                {
                    isSliding = false;
                    currentPitch = targetPitch;
                }
                else
                {
                    // Normal glissando processing
                    const float glissandoRate = 0.001f * sampleRate;
                    if (std::abs(targetPitch - currentPitch) < 0.1f)
                    {
                        currentPitch = targetPitch;
                        isSliding = false;
                    }
                    else if (currentPitch < targetPitch)
                    {
                        currentPitch += glissandoRate;
                    }
                    else
                    {
                        currentPitch -= glissandoRate;
                    }
                }
                
                updateFrequency();
            }
            
            // Process the sample
            float sample = oscillator.processSample(0.0f);
            
            // Apply amplitude
            float amplitude = 1.0f;
            if (tailOff)
            {
                tailOffCounter++;
                // Gradually decrease amplitude
                amplitude = 1.0f - (static_cast<float>(tailOffCounter) / tailOffDuration);
                if (amplitude < 0.0f) amplitude = 0.0f;
                
                // Processing when tail-off is complete
                if (tailOffCounter >= tailOffDuration)
                {
                    tailOff = false;
                    noteOn = false;
                    currentlyPlayingNote = 0;
                }
            }
            
            return sample * amplitude;
        }
        
        // Render a block of samples
        void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override
        {
            if (!isActive())
                return;
                
            for (int sample = 0; sample < numSamples; ++sample)
            {
                float currentSample = getNextSample();
                
                for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
                {
                    outputBuffer.addSample(channel, startSample + sample, currentSample);
                }
            }
            
            // Advance counter if in tail-off
            if (tailOff)
            {
                tailOffCounter += numSamples;
                
                if (tailOffCounter >= tailOffDuration)
                {
                    tailOff = false;
                    noteOn = false;
                    currentlyPlayingNote = 0;
                }
            }
        }
        
        // Set LFO modulation
        void setLfoValue(float modAmount) override
        {
            float modDepth = 0.0f;
            if (modDepthParam != nullptr)
                modDepth = modDepthParam->load();
                
            lfoModulation = modAmount * modDepth;
            updateFrequency();
        }
        
        // For backward compatibility with ToneGeneratorTest
        void setLfoModulation(float modAmount)
        {
            setLfoValue(modAmount);
        }
        
        // For backward compatibility with ToneGeneratorTest
        void setGlissando(bool enabled)
        {
            if (glissandoParam != nullptr)
            {
                glissandoParam->store(enabled ? 1.0f : 0.0f);
            }
        }
        
    private:
        // Update oscillator frequency based on current parameters
        void updateFrequency()
        {
            // Get waveform parameter
            if (waveTypeParam != nullptr)
            {
                int waveformIndex = static_cast<int>(*waveTypeParam);
                MockOscillator::WaveformType waveform;
                
                switch (waveformIndex)
                {
                    case 0: waveform = MockOscillator::WaveformType::triangle; break;
                    case 1: waveform = MockOscillator::WaveformType::saw; break;
                    case 2: waveform = MockOscillator::WaveformType::square; break;
                    case 3: waveform = MockOscillator::WaveformType::square; break; // Pulse
                    case 4: waveform = MockOscillator::WaveformType::square; break; // PWM
                    default: waveform = MockOscillator::WaveformType::saw; break;
                }
                
                oscillator.setWaveform(waveform);
            }
            
            // Calculate octave shift based on feet parameter
            float octaveShift = 0.0f;
            if (feetParam != nullptr)
            {
                int feetIndex = static_cast<int>(*feetParam);
                switch (feetIndex)
                {
                    case 0: octaveShift = -24.0f; break; // 32'
                    case 1: octaveShift = -12.0f; break; // 16'
                    case 2: octaveShift = 0.0f; break;   // 8'
                    case 3: octaveShift = 12.0f; break;  // 4'
                    case 4: octaveShift = 0.0f; break;   // Noise
                    default: octaveShift = 0.0f; break;
                }
            }
            
            // Add pitch bend from parameters
            float pitchBendValue = 0.0f;
            float pitchValue = 0.0f;
            
            if (pitchBendParam != nullptr)
                pitchBendValue = pitchBendParam->load();
                
            if (pitchParam != nullptr)
                pitchValue = pitchParam->load();
            
            float pitchWithModulation = currentPitch + pitchBendSemitones + pitchBendValue + pitchValue + octaveShift + lfoModulation;
            float frequency = 440.0f * std::pow(2.0f, (pitchWithModulation - 69.0f) / 12.0f);
            
            // Set oscillator frequency
            oscillator.setFrequency(frequency);
        }
        
        // Mock oscillator
        MockOscillator oscillator;
        
        // Parameters
        juce::AudioProcessorValueTreeState& parameters;
        std::atomic<float>* waveTypeParam = nullptr;
        std::atomic<float>* feetParam = nullptr;
        std::atomic<float>* modDepthParam = nullptr;
        std::atomic<float>* glissandoParam = nullptr;
        std::atomic<float>* releaseParam = nullptr;
        std::atomic<float>* pitchBendUpRangeParam = nullptr;
        std::atomic<float>* pitchBendDownRangeParam = nullptr;
        std::atomic<float>* pitchBendParam = nullptr;
        std::atomic<float>* pitchParam = nullptr;
        std::atomic<float>* pwmSpeedParam = nullptr;
        
        // Note state
        bool noteOn = false;
        bool tailOff = false;
        int currentlyPlayingNote = 0;
        int tailOffCounter = 0;
        int tailOffDuration = 1000;
        
        // Pitch state
        float currentPitch = 0.0f;
        float targetPitch = 0.0f;
        bool isSliding = false;
        float pitchBendSemitones = 0.0f;
        float lfoModulation = 0.0f;
        
        // Processing state
        double sampleRate = 44100.0;
    };
}
