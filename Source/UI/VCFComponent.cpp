#include "CS01LookAndFeel.h"
#include "VCFComponent.h"
#include "../Parameters.h"

VCFComponent::VCFComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts) {
    // --- Filter Type Selector (Integrated) ---
    filterTypeParam = valueTreeState.getParameter(ParameterIds::filterType);
    jassert(filterTypeParam != nullptr);

    addAndMakeVisible(filterTypeLabel);
    filterTypeLabel.setText("TYPE", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(filterTypeParam)) {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i) {
            auto* button = filterTypeButtons.add(new juce::ToggleButton(choices[i].toUpperCase()));
            addAndMakeVisible(button);
            button->setRadioGroupId(100);
            button->setClickingTogglesState(true);
            button->onClick = [choiceParam, i] { *choiceParam = i; };
        }
    }
    filterTypeParam->addListener(this);

    // --- Sliders ---
    cutoffSlider.setSliderStyle(juce::Slider::LinearVertical);
    cutoffSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(cutoffSlider);
    cutoffLabel.setText("CUTOFF", juce::dontSendNotification);
    addAndMakeVisible(cutoffLabel);
    cutoffAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::cutoff), cutoffSlider);

    resonanceSlider.setSliderStyle(juce::Slider::LinearVertical);
    resonanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addChildComponent(resonanceSlider); // Initially hidden, shown in Modern mode
    resonanceLabel.setText("RES", juce::dontSendNotification);
    addAndMakeVisible(resonanceLabel);
    resonanceAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::resonance), resonanceSlider);

    resonanceButton.setButtonText("HIGH");
    addChildComponent(resonanceButton); // Initially hidden, shown in Original mode
    resonanceButtonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        valueTreeState, ParameterIds::resonance, resonanceButton);

    egDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    egDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(egDepthSlider);
    egDepthLabel.setText("EG DEPTH", juce::dontSendNotification);
    addAndMakeVisible(egDepthLabel);
    egDepthAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::vcfEgDepth), egDepthSlider);

    // Initial update
    parameterValueChanged(filterTypeParam->getParameterIndex(), filterTypeParam->getValue());
    // Match the VCO faders without changing parameter ranges or values.
    cutoffSlider.setPopupDisplayEnabled(true, true, this);
    cutoffSlider.setSliderSnapsToMousePosition(false);
    cutoffSlider.setDoubleClickReturnValue(true,
        valueTreeState.getParameter(ParameterIds::cutoff)->convertFrom0to1(
            valueTreeState.getParameter(ParameterIds::cutoff)->getDefaultValue()));
    resonanceSlider.setPopupDisplayEnabled(true, true, this);
    resonanceSlider.setSliderSnapsToMousePosition(false);
    resonanceSlider.setDoubleClickReturnValue(true,
        valueTreeState.getParameter(ParameterIds::resonance)->convertFrom0to1(
            valueTreeState.getParameter(ParameterIds::resonance)->getDefaultValue()));
    egDepthSlider.setPopupDisplayEnabled(true, true, this);
    egDepthSlider.setSliderSnapsToMousePosition(false);
    egDepthSlider.setDoubleClickReturnValue(true,
        valueTreeState.getParameter(ParameterIds::vcfEgDepth)->convertFrom0to1(
            valueTreeState.getParameter(ParameterIds::vcfEgDepth)->getDefaultValue()));

}

VCFComponent::~VCFComponent() {
    if (filterTypeParam)
        filterTypeParam->removeListener(this);
}

void VCFComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "VCF");
}

void VCFComponent::resized() {
    auto bounds = getLocalBounds().reduced(10).withTrimmedTop(20);
    auto labels = bounds.removeFromBottom(32);
    const int columnWidth = bounds.getWidth() / 4;
    auto typeColumn = bounds.removeFromLeft(columnWidth);
    const int rowHeight = juce::jmin(36, typeColumn.getHeight() / filterTypeButtons.size());
    typeColumn.removeFromTop((typeColumn.getHeight() - rowHeight * filterTypeButtons.size()) / 2);
    for (auto* button : filterTypeButtons)
        button->setBounds(typeColumn.removeFromTop(rowHeight).reduced(2));

    cutoffSlider.setBounds(bounds.removeFromLeft(columnWidth));
    auto resonanceColumn = bounds.removeFromLeft(columnWidth);
    resonanceSlider.setBounds(resonanceColumn);
    resonanceButton.setBounds(resonanceColumn.withSizeKeepingCentre(resonanceColumn.getWidth(), 36));
    egDepthSlider.setBounds(bounds);

    for (auto* label : { &filterTypeLabel, &cutoffLabel, &resonanceLabel }) {
        label->setBounds(labels.removeFromLeft(columnWidth));
        label->setJustificationType(juce::Justification::centred);
    }
    egDepthLabel.setBounds(labels);
    egDepthLabel.setJustificationType(juce::Justification::centred);
}

void VCFComponent::parameterValueChanged(int parameterIndex, float newValue) {
    if (parameterIndex == filterTypeParam->getParameterIndex()) {
        // Update UI state
        bool isModern = (newValue >= 0.5f); // Assuming 0=Original, 1=Modern

        // Update Buttons State
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(filterTypeParam)) {
             int index = choiceParam->getIndex();
             if (index >= 0 && index < filterTypeButtons.size()) {
                 filterTypeButtons[index]->setToggleState(true, juce::dontSendNotification);
             }
        }

        // Toggle Resonance Control
        if (isModern) {
            resonanceSlider.setVisible(true);
            resonanceButton.setVisible(false);
        } else {
            resonanceSlider.setVisible(false);
            resonanceButton.setVisible(true);
        }
        resized(); // Re-layout
    }
}

void VCFComponent::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {}
