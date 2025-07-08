#pragma once
#include <JuceHeader.h>

class LFOComponent : public juce::Component {
   public:
    LFOComponent(juce::AudioProcessorValueTreeState& apvts);
    ~LFOComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider lfoSpeedSlider;
    juce::Label lfoSpeedLabel;

    std::unique_ptr<juce::SliderParameterAttachment> lfoSpeedAttachment;
};
