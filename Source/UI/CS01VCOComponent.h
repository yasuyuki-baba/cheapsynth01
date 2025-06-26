#pragma once
#include <JuceHeader.h>

class CS01VCOComponent : public juce::Component,
                           public juce::AudioProcessorParameter::Listener
{
public:
    CS01VCOComponent(juce::AudioProcessorValueTreeState& apvts);
    ~CS01VCOComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider glissandoSlider;
    juce::Label glissandoLabel;
    std::unique_ptr<juce::SliderParameterAttachment> glissandoAttachment;

    juce::Slider pitchSlider;
    juce::Label pitchLabel;
    std::unique_ptr<juce::SliderParameterAttachment> pitchAttachment;

    juce::OwnedArray<juce::ToggleButton> waveTypeButtons;
    juce::Label waveTypeLabel;
    juce::AudioProcessorParameter* waveTypeParam = nullptr;
    
    juce::OwnedArray<juce::ToggleButton> feetButtons;
    juce::Label feetLabel;
    juce::AudioProcessorParameter* feetParam = nullptr;

    juce::Slider pwmSpeedSlider;
    juce::Label pwmSpeedLabel;
    std::unique_ptr<juce::SliderParameterAttachment> pwmSpeedAttachment;
};
