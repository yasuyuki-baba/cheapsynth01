#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>

class VCFComponent : public juce::Component {
   public:
    VCFComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCFComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;

    // Filter Type Selector (Integrated)
    juce::OwnedArray<juce::ToggleButton> filterTypeButtons;
    std::unique_ptr<CS01ChoiceButtonParameterAttachment> filterTypeAttachment;

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
