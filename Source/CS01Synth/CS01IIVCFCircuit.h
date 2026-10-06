#pragma once

#include "CS01Synth/IG05630.h"
#include "CS01Synth/ExperimentalIG05630.h"

#include <atomic>

// Circuit boundary for Modern, analogous to CS01VCFCircuit in Original.
// External coupling is currently unity: no uncalibrated stages are added.
class CS01IIVCFCircuit {
   public:
    enum class Model { Legacy, Experimental };
    void setModel(Model newModel) { selectedModel.store(newModel, std::memory_order_relaxed); }
    Model getModel() const { return selectedModel.load(std::memory_order_relaxed); }

    void prepare(double sampleRate);
    void reset();
    void setCutoffFrequency(float frequency);
    void setResonance(float resonance);
    float processSample(int channel, float sample);

   private:
    IG05630 model;
    ExperimentalIG05630 experimentalModel;
    std::atomic<Model> selectedModel{Model::Legacy};
    float maximumCutoff = 20000.0f;
};
