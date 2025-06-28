#pragma once

#include <JuceHeader.h>

//==============================================================================
// IG02610 2-pole lowpass filter implementation
class IG02610LPF
{
public:
    IG02610LPF();
    ~IG02610LPF() = default;
    
    void reset();
    void prepare(double sampleRate);
    void setCutoffFrequency(float newCutoff);
    void setResonance(float newResonance);
    
    // Process a single sample (legacy method)
    float processSample(int channel, float sample);
    
    // Process a block of samples (more efficient)
    void processBlock(float* samples, int numSamples);
    
    // Process a block of samples with multiple channels
    void processBlock(float** channelData, int numChannels, int numSamples);
    
    // Process a block with per-sample cutoff modulation
    void processBlock(float* samples, int numSamples, 
                     const float* cutoffModulation, float baseResonance);
    
private:
    float cutoff, resonance, sampleRate;
    float a1, a2, b0, b1, b2;
    float z1, z2;
    
    // WaveShaper parameters - defined as static constants
    static constexpr float RESONANCE_DRIVE = 1.2f;
    static constexpr float RESONANCE_SHAPE = 0.8f;
    static constexpr float OUTPUT_DRIVE = 1.1f;
    
    // Input stage model
    struct InputStage {
        float prevSample = 0.0f;
        juce::dsp::IIR::Filter<float> dcBlocker;
        
        void prepare(double sampleRate) {
            dcBlocker.reset();
            dcBlocker.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f);
        }
        
        void reset() {
            prevSample = 0.0f;
            dcBlocker.reset();
        }
    };
    
    // Output stage model
    struct OutputStage {
        float prevInput = 0.0f;
        float prevOutput = 0.0f;
        double sampleRate = 44100.0;
        
        void prepare(double newSampleRate) {
            sampleRate = newSampleRate;
        }
        
        void reset() {
            prevInput = 0.0f;
            prevOutput = 0.0f;
        }
    };
    
    InputStage inputStage;
    OutputStage outputStage;
    
    // More accurate tanh approximation
    float accurateTanh(float x);
    
    // Input stage processing
    float processInputStage(float sample);
    
    // Output stage processing
    float processOutputStage(float sample);
    
    void updateCoefficients();
};
