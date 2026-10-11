#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>

class VCAComponent : public juce::Component {
   public:
    VCAComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCAComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider vcaEgDepthSlider;
    juce::Label vcaEgDepthLabel;

    std::unique_ptr<CS01SliderParameterAttachment> vcaEgDepthAttachment;
};
