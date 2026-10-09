#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>
#include <atomic>

class VCOComponent : public juce::Component,
                     public juce::AudioProcessorParameter::Listener,
                     private juce::Timer {
   public:
    VCOComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCOComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

   private:
    void updateChoiceState(int parameterIndex, float newValue);
    std::atomic<bool> choiceDirty{true};
    void timerCallback() override;
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider glissandoSlider;
    juce::Label glissandoLabel;
    std::unique_ptr<CS01SliderParameterAttachment> glissandoAttachment;

    juce::Slider pitchSlider;
    juce::Label pitchLabel;
    std::unique_ptr<CS01SliderParameterAttachment> pitchAttachment;

    juce::OwnedArray<juce::ToggleButton> waveTypeButtons;
    juce::Label waveTypeLabel;
    juce::AudioProcessorParameter* waveTypeParam = nullptr;

    juce::OwnedArray<juce::ToggleButton> feetButtons;
    juce::Label feetLabel;
    juce::AudioProcessorParameter* feetParam = nullptr;

    juce::Slider pwmSpeedSlider;
    juce::Label pwmSpeedLabel;
    std::unique_ptr<CS01SliderParameterAttachment> pwmSpeedAttachment;
};
