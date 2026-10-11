#pragma once

#include "CS01Synth/VCOProcessor.h"

#include <memory>

/**
 * Example showing how to use the enhanced VCOProcessor with different sound generators
 */
class SoundProcessorExample {
   public:
    SoundProcessorExample(juce::AudioProcessorValueTreeState& apvts) {
        // Create VCO processor with ToneGenerator (default)
        vcoProcessor = std::make_unique<VCOProcessor>(apvts);

        // Example: Switch to NoiseGenerator
        // vcoProcessor->setGeneratorType(GeneratorType::Noise);

        // Get the sound generator (regardless of its type)
        auto* generator = vcoProcessor->getSoundGenerator();

        // Example: Check if active and render
        if (generator->isActive()) {
            juce::AudioBuffer<float> buffer(1, 512);
            generator->renderNextBlock(buffer, 0, buffer.getNumSamples());
        }

        // Example: Handle notes through the unified sound generator interface
        generator->startNote(60, 0.8f, 8192);

        // Example: Switch between generator types at runtime
        // vcoProcessor->setGeneratorType(GeneratorType::Tone);   // Switch to ToneGenerator
        // vcoProcessor->setGeneratorType(GeneratorType::Noise);  // Switch to NoiseGenerator
    }

   private:
    std::unique_ptr<VCOProcessor> vcoProcessor;
};
