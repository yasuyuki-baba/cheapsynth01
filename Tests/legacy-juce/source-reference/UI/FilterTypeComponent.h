#pragma once

#include <JuceHeader.h>

#include "UI/PollingParameterAttachments.h"

#include <memory>

class FilterTypeComponent : public juce::Component {
   public:
    FilterTypeComponent(juce::AudioProcessorValueTreeState& apvts);
    ~FilterTypeComponent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

   private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::ComboBox filterTypeComboBox;
    std::unique_ptr<CS01ComboBoxParameterAttachment> filterTypeAttachment;
};
