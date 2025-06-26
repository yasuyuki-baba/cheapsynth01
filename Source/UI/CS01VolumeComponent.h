#pragma once
#include <JuceHeader.h>

class CS01VolumeComponent : public juce::Component
{
public:
    CS01VolumeComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01VolumeComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    
    std::unique_ptr<juce::SliderParameterAttachment> volumeAttachment;
};
