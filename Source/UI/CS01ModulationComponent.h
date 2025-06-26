#pragma once

#include <JuceHeader.h>

class CS01AudioProcessor;

class CS01ModulationComponent : public juce::Component,
                                public juce::AudioProcessorParameter::Listener,
                                public juce::Slider::Listener
{
public:
    CS01ModulationComponent(CS01AudioProcessor& p);
    ~CS01ModulationComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

private:
    CS01AudioProcessor& processor;

    juce::Slider pitchBendSlider;
    juce::Label pitchBendLabel;

    juce::Slider modDepthSlider;
    juce::Label modDepthLabel;

    juce::OwnedArray<juce::ToggleButton> lfoTargetButtons;
    juce::Label lfoTargetLabel;
    juce::AudioProcessorParameter* lfoTargetParam = nullptr;
};
