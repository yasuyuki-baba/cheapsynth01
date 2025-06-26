#pragma once
#include <JuceHeader.h>

class VolumeComponent : public juce::Component
{
public:
    VolumeComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VolumeComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider volumeSlider;
    juce::Label volumeLabel;
    
    std::unique_ptr<juce::SliderParameterAttachment> volumeAttachment;
};
