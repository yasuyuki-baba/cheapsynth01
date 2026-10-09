#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>
#include <atomic>

class VCFComponent : public juce::Component,
                     public juce::AudioProcessorParameter::Listener,
                     private juce::Timer {
   public:
    VCFComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCFComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioProcessorValueTreeState::Listener interface
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

   private:
    void updateChoiceState(int parameterIndex, float newValue);
    std::atomic<bool> choiceDirty{true};
    void timerCallback() override;
    juce::AudioProcessorValueTreeState& valueTreeState;

    // Filter Type Selector (Integrated)
    juce::RangedAudioParameter* filterTypeParam = nullptr;
    juce::OwnedArray<juce::ToggleButton> filterTypeButtons;

    juce::Slider cutoffSlider;
    juce::Label cutoffLabel;
    std::unique_ptr<CS01SliderParameterAttachment> cutoffAttachment;

    // Resonance controls (swapped based on mode)
    juce::Slider resonanceSlider;
    juce::Label resonanceLabel;
    std::unique_ptr<CS01SliderParameterAttachment> resonanceAttachment;

    juce::ToggleButton resonanceButton;
    std::unique_ptr<CS01ButtonParameterAttachment> resonanceButtonAttachment;

    juce::Slider egDepthSlider;
    juce::Label egDepthLabel;
    std::unique_ptr<CS01SliderParameterAttachment> egDepthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCFComponent)
};
