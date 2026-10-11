#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>

class BreathControlComponent : public juce::Component {
   public:
    BreathControlComponent(juce::AudioProcessorValueTreeState& apvts);
    ~BreathControlComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider breathVcfSlider;
    juce::Label breathVcfLabel;
    std::unique_ptr<CS01SliderParameterAttachment> breathVcfAttachment;

    juce::Slider breathVcaSlider;
    juce::Label breathVcaLabel;
    std::unique_ptr<CS01SliderParameterAttachment> breathVcaAttachment;
};
