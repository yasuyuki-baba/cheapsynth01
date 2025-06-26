#pragma once
#include <JuceHeader.h>

class FilterTypeComponent : public juce::Component
{
public:
    FilterTypeComponent(juce::AudioProcessorValueTreeState& apvts);
    ~FilterTypeComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::ComboBox filterTypeComboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
};
