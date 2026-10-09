#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>
#include <atomic>

class CS01AudioProcessor;

class ModulationComponent : public juce::Component,
                            public juce::AudioProcessorParameter::Listener,
                            public juce::Slider::Listener,
                            private juce::Timer {
   public:
    ModulationComponent(CS01AudioProcessor& p);
    ~ModulationComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider) override;
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

   private:
    void updateChoiceState(int parameterIndex, float newValue);
    std::atomic<bool> choiceDirty{true};
    void timerCallback() override;
    CS01AudioProcessor& processor;

    juce::Slider pitchBendSlider;
    juce::Label pitchBendLabel;
    juce::Slider bendUpSlider, bendDownSlider;
    juce::Label bendUpLabel, bendDownLabel;
    std::unique_ptr<CS01SliderParameterAttachment> bendUpAttachment;
    std::unique_ptr<CS01SliderParameterAttachment> bendDownAttachment;
    bool draggingBend = false;
    bool returningBend = false;
    double returnStarted = 0.0, returnPosition = 0.0;
    unsigned bendRevision = 0;

    juce::Slider modDepthSlider;
    juce::Label modDepthLabel;
    bool draggingMod = false;

    juce::OwnedArray<juce::ToggleButton> lfoTargetButtons;
    juce::Label lfoTargetLabel;
    juce::AudioProcessorParameter* lfoTargetParam = nullptr;
};
