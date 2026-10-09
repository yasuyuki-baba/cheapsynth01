#include "UI/FilterTypeComponent.h"

#include "Parameters.h"
#include "UI/CS01LookAndFeel.h"

FilterTypeComponent::FilterTypeComponent(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts) {
    filterTypeComboBox.addItem("Original", 1);
    filterTypeComboBox.addItem("Modern", 2);
    addAndMakeVisible(filterTypeComboBox);

    filterTypeAttachment = std::make_unique<CS01ComboBoxParameterAttachment>(
        valueTreeState, ParameterIds::filterType, filterTypeComboBox);
}

FilterTypeComponent::~FilterTypeComponent() {}

void FilterTypeComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "FILTER TYPE");
}

void FilterTypeComponent::resized() {
    auto bounds = getLocalBounds();
    // Adjust the position of the combo box to align its height with sliders in other components
    filterTypeComboBox.setBounds(bounds.withTrimmedTop(bounds.getHeight() * 0.6f).reduced(5, 5));
}
