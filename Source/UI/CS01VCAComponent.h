#pragma once
#include <JuceHeader.h>

class CS01VCAComponent : public juce::Component
{
public:
    CS01VCAComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01VCAComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider vcaEgDepthSlider;
    juce::Label vcaEgDepthLabel;

    std::unique_ptr<juce::SliderParameterAttachment> vcaEgDepthAttachment;
};
