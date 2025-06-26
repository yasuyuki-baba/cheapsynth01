#pragma once
#include <JuceHeader.h>

class CS01EGComponent : public juce::Component
{
public:
    CS01EGComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01EGComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider attackSlider;
    juce::Label attackLabel;
    std::unique_ptr<juce::SliderParameterAttachment> attackAttachment;

    juce::Slider decaySlider;
    juce::Label decayLabel;
    std::unique_ptr<juce::SliderParameterAttachment> decayAttachment;

    juce::Slider sustainSlider;
    juce::Label sustainLabel;
    std::unique_ptr<juce::SliderParameterAttachment> sustainAttachment;

    juce::Slider releaseSlider;
    juce::Label releaseLabel;
    std::unique_ptr<juce::SliderParameterAttachment> releaseAttachment;
};
