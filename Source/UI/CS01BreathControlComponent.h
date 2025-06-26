#pragma once
#include <JuceHeader.h>

class CS01BreathControlComponent : public juce::Component
{
public:
    CS01BreathControlComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01BreathControlComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider breathVcfSlider;
    juce::Label breathVcfLabel;
    std::unique_ptr<juce::SliderParameterAttachment> breathVcfAttachment;

    juce::Slider breathVcaSlider;
    juce::Label breathVcaLabel;
    std::unique_ptr<juce::SliderParameterAttachment> breathVcaAttachment;
};
