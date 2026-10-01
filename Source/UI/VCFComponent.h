#pragma once
#include <JuceHeader.h>
#include "../Parameters.h"

class VCFComponent : public juce::Component,
                     public juce::AudioProcessorParameter::Listener {
   public:
    VCFComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCFComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioProcessorValueTreeState::Listener interface
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;

    // Filter Type Selector (Integrated)
    juce::RangedAudioParameter* filterTypeParam = nullptr;
    juce::OwnedArray<juce::ToggleButton> filterTypeButtons;
    juce::Label filterTypeLabel;

    juce::Slider cutoffSlider;
    juce::Label cutoffLabel;
    std::unique_ptr<juce::SliderParameterAttachment> cutoffAttachment;

    // Resonance controls (swapped based on mode)
    juce::Slider resonanceSlider;
    juce::Label resonanceLabel;
    std::unique_ptr<juce::SliderParameterAttachment> resonanceAttachment;

    juce::ToggleButton resonanceButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> resonanceButtonAttachment;

    juce::Slider egDepthSlider;
    juce::Label egDepthLabel;
    std::unique_ptr<juce::SliderParameterAttachment> egDepthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VCFComponent)
};
