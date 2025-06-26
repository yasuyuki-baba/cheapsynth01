#pragma once
#include <JuceHeader.h>

class CS01VCFComponent : public juce::Component
{
public:
    CS01VCFComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01VCFComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider cutoffSlider;
    juce::Label cutoffLabel;
    std::unique_ptr<juce::SliderParameterAttachment> cutoffAttachment;

    juce::ToggleButton resonanceButton;
    juce::Label resonanceLabel;
    std::unique_ptr<juce::ButtonParameterAttachment> resonanceAttachment;

    juce::Slider vcfEgDepthSlider;
    juce::Label vcfEgDepthLabel;
    std::unique_ptr<juce::SliderParameterAttachment> vcfEgDepthAttachment;
};
