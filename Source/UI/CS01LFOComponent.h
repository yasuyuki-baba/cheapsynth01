#pragma once
#include <JuceHeader.h>

class CS01LFOComponent : public juce::Component
{
public:
    CS01LFOComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01LFOComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider lfoSpeedSlider;
    juce::Label lfoSpeedLabel;

    std::unique_ptr<juce::SliderParameterAttachment> lfoSpeedAttachment;
};
