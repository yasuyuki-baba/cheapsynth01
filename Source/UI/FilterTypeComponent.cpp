#include "FilterTypeComponent.h"
#include "../Parameters.h"

FilterTypeComponent::FilterTypeComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    filterTypeComboBox.addItem("Original", 1);
    filterTypeComboBox.addItem("Modern", 2);
    addAndMakeVisible(filterTypeComboBox);
    
    filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        valueTreeState, ParameterIds::filterType, filterTypeComboBox);
}

FilterTypeComponent::~FilterTypeComponent() {}

void FilterTypeComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("FILTER TYPE", getLocalBounds().withTrimmedBottom(getHeight() / 2), juce::Justification::centred, 1);
}

void FilterTypeComponent::resized()
{
    auto bounds = getLocalBounds();
    // Adjust the position of the combo box to align its height with sliders in other components
    filterTypeComboBox.setBounds(bounds.withTrimmedTop(bounds.getHeight() * 0.6f).reduced(5, 5));
}
