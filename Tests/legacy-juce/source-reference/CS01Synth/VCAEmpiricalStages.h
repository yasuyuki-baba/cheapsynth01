#pragma once

#include <cmath>

// Stateful external coupling approximation, not a component/load solution.
// The caller supplies the empirical pole defined at 44.1 kHz.
class EmpiricalVCACoupling {
   public:
    explicit EmpiricalVCACoupling(float initialPole) : pole(initialPole) {}
    void prepare(double sampleRate, float referencePole) {
        pole =
            static_cast<float>(std::pow(static_cast<double>(referencePole), 44100.0 / sampleRate));
    }
    void reset() {
        capacitorState = 0.0f;
    }
    float processSample(float input) {
        capacitorState = capacitorState * pole + input * (1.0f - pole);
        return input - capacitorState;
    }

   private:
    float capacitorState = 0.0f;
    float pole;
};

// Empirical Tr7 coloration only; external coupling is a separate stage.
// Neither the asymmetry nor the treble emphasis is hardware-calibrated.
class Tr7EmpiricalBuffer {
   public:
    struct EmpiricalParameters {
        static constexpr float positiveGain = 0.95f;
        static constexpr float negativeGain = 0.92f;
        // This difference term deliberately retains its original rate dependence.
        static constexpr float trebleDifferenceReference = 0.998f;
        static constexpr float trebleGain = 2.0f;
    };
    void reset() {
        previousOutput = 0.0f;
    }
    float processSample(float input) {
        const float transistorOutput = input > 0 ? input * EmpiricalParameters::positiveGain
                                                 : input * EmpiricalParameters::negativeGain;
        const float highFreqComponent = (transistorOutput - previousOutput) *
                                        (1.0f - EmpiricalParameters::trebleDifferenceReference) *
                                        EmpiricalParameters::trebleGain;
        previousOutput = transistorOutput;
        return transistorOutput + highFreqComponent;
    }

   private:
    float previousOutput = 0.0f;
};
