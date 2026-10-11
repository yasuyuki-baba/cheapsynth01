#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>

class VCOComponent : public juce::Component {
   public:
    VCOComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCOComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Slider glissandoSlider;
    juce::Label glissandoLabel;
    std::unique_ptr<CS01SliderParameterAttachment> glissandoAttachment;

    juce::Slider pitchSlider;
    juce::Label pitchLabel;
    std::unique_ptr<CS01SliderParameterAttachment> pitchAttachment;

    juce::OwnedArray<juce::ToggleButton> waveTypeButtons;
    juce::Label waveTypeLabel;
    std::unique_ptr<CS01ChoiceButtonParameterAttachment> waveTypeAttachment;

    juce::OwnedArray<juce::ToggleButton> feetButtons;
    juce::Label feetLabel;
    std::unique_ptr<CS01ChoiceButtonParameterAttachment> feetAttachment;

    juce::Slider pwmSpeedSlider;
    juce::Label pwmSpeedLabel;
    std::unique_ptr<CS01SliderParameterAttachment> pwmSpeedAttachment;
};
