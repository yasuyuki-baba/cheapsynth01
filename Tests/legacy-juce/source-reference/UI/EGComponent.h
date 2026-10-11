#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>

class EGComponent : public juce::Component {
   public:
    EGComponent(juce::AudioProcessorValueTreeState& apvts);
    ~EGComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider attackSlider;
    juce::Label attackLabel;
    std::unique_ptr<CS01SliderParameterAttachment> attackAttachment;

    juce::Slider decaySlider;
    juce::Label decayLabel;
    std::unique_ptr<CS01SliderParameterAttachment> decayAttachment;

    juce::Slider sustainSlider;
    juce::Label sustainLabel;
    std::unique_ptr<CS01SliderParameterAttachment> sustainAttachment;

    juce::Slider releaseSlider;
    juce::Label releaseLabel;
    std::unique_ptr<CS01SliderParameterAttachment> releaseAttachment;
};
