#pragma once
#include <JuceHeader.h>
#include "../Parameters.h"
#include "../CS01Synth/IFilter.h"

class VCFComponent : public juce::Component {
   public:
    VCFComponent(juce::AudioProcessorValueTreeState& apvts);
    ~VCFComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Update UI when filter processor changes
    void updateFilterControl(IFilter* filterProcessor);

   private:
    // Method to monitor filter type changes and update resonance UI
    void updateResonanceControl(float filterType);

    juce::AudioProcessorValueTreeState& valueTreeState;

    juce::Slider cutoffSlider;
    juce::Label cutoffLabel;
    std::unique_ptr<juce::SliderParameterAttachment> cutoffAttachment;

    // Toggle button for CS01 filter resonance
    juce::ToggleButton resonanceButton;
    juce::Label resonanceLabel;
    std::unique_ptr<juce::ButtonParameterAttachment> resonanceAttachment;

    // Slider for Modern filter resonance
    juce::Slider resonanceSlider;
    juce::Label resonanceSliderLabel;
    std::unique_ptr<juce::SliderParameterAttachment> resonanceSliderAttachment;

    juce::Slider vcfEgDepthSlider;
    juce::Label vcfEgDepthLabel;
    std::unique_ptr<juce::SliderParameterAttachment> vcfEgDepthAttachment;

    // Attachment to monitor filter type parameter changes
    std::unique_ptr<juce::ParameterAttachment> filterTypeAttachment;
};
