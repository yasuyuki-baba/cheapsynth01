#include "CS01LookAndFeel.h"
#include "VCFComponent.h"
#include "../Parameters.h"

VCFComponent::VCFComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts) {
    // --- Filter Type Selector (Integrated) ---
    filterTypeParam = valueTreeState.getParameter(ParameterIds::filterType);
    jassert(filterTypeParam != nullptr);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(filterTypeParam)) {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i) {
            const juce::String displayName = i == 0   ? "I"
                                             : i == 1 ? "II"
                                                      : choices[i].toUpperCase();
            auto* button = filterTypeButtons.add(new juce::ToggleButton(displayName));
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
    addChildComponent(resonanceSlider);  // Initially hidden, shown in Modern mode
    resonanceLabel.setText("RES", juce::dontSendNotification);
    addAndMakeVisible(resonanceLabel);
    resonanceAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::resonance), resonanceSlider);

    resonanceButton.setButtonText("HIGH");
    addChildComponent(resonanceButton);  // Initially hidden, shown in Original mode
    resonanceButtonAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
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
    cutoffSlider.setDoubleClickReturnValue(
        true, valueTreeState.getParameter(ParameterIds::cutoff)
                  ->convertFrom0to1(
                      valueTreeState.getParameter(ParameterIds::cutoff)->getDefaultValue()));
    resonanceSlider.setPopupDisplayEnabled(true, true, this);
    resonanceSlider.setSliderSnapsToMousePosition(false);
    resonanceSlider.setDoubleClickReturnValue(
        true, valueTreeState.getParameter(ParameterIds::resonance)
                  ->convertFrom0to1(
                      valueTreeState.getParameter(ParameterIds::resonance)->getDefaultValue()));
    egDepthSlider.setPopupDisplayEnabled(true, true, this);
    egDepthSlider.setSliderSnapsToMousePosition(false);
    egDepthSlider.setDoubleClickReturnValue(
        true, valueTreeState.getParameter(ParameterIds::vcfEgDepth)
                  ->convertFrom0to1(
                      valueTreeState.getParameter(ParameterIds::vcfEgDepth)->getDefaultValue()));
}

VCFComponent::~VCFComponent() {
    if (filterTypeParam)
        filterTypeParam->removeListener(this);
}

void VCFComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "VCF");
    // Clear the header rule behind the mode switches, retaining the section divider.
    g.setColour(CS01LookAndFeel::Palette::background);
    g.fillRect(48, 0, 72, 22);
}

void VCFComponent::resized() {
    auto bounds = getLocalBounds().withTrimmedTop(30).withTrimmedBottom(10);
    auto labels = bounds.removeFromBottom(32);
    auto modes = getLocalBounds().withTrimmedLeft(48).withTrimmedRight(10).withHeight(22);
    const int modeWidth = 36;
    for (auto* button : filterTypeButtons)
        button->setBounds(modes.removeFromLeft(modeWidth));
    const int columnWidth = bounds.getWidth() / 3;

    cutoffSlider.setBounds(bounds.removeFromLeft(columnWidth));
    auto resonanceColumn = bounds.removeFromLeft(columnWidth);
    resonanceSlider.setBounds(resonanceColumn);
    resonanceButton.setBounds(
        resonanceColumn.withSizeKeepingCentre(resonanceColumn.getWidth(), 36));
    egDepthSlider.setBounds(bounds);

    for (auto* label : {&cutoffLabel, &resonanceLabel}) {
        label->setBounds(labels.removeFromLeft(columnWidth));
        label->setBorderSize(juce::BorderSize<int>(0));
        label->setJustificationType(juce::Justification::centred);
    }
    egDepthLabel.setBounds(labels);
    egDepthLabel.setJustificationType(juce::Justification::centred);
}

void VCFComponent::parameterValueChanged(int parameterIndex, float newValue) {
    if (parameterIndex == filterTypeParam->getParameterIndex()) {
        // Update UI state
        bool isModern = (newValue >= 0.5f);  // Assuming 0=Original, 1=Modern

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
        resized();  // Re-layout
    }
}

void VCFComponent::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {}
