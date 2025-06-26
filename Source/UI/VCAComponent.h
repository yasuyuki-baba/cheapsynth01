#pragma once
#include <JuceHeader.h>

class VCAComponent : public juce::Component
{
public:
    VCAComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCAComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider vcaEgDepthSlider;
    juce::Label vcaEgDepthLabel;

    std::unique_ptr<juce::SliderParameterAttachment> vcaEgDepthAttachment;
};
