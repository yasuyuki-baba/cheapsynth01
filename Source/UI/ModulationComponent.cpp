#include "CS01LookAndFeel.h"
#include "ModulationComponent.h"
#include "../CS01AudioProcessor.h"
#include "../Parameters.h"

ModulationComponent::ModulationComponent(CS01AudioProcessor& p) : processor(p) {
    // Pitch Bend Slider
    pitchBendSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchBendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchBendSlider.setRange(0.0, 1.0, 0.001);
    pitchBendSlider.setValue(0.0);
    pitchBendSlider.getProperties().set("performanceWheel", true);
    pitchBendSlider.setSliderSnapsToMousePosition(false);
    pitchBendSlider.setPopupDisplayEnabled(true, true, this);
    pitchBendSlider.setDoubleClickReturnValue(true, 0.0);
    pitchBendSlider.addListener(this);
    addAndMakeVisible(pitchBendSlider);
    pitchBendLabel.setText("BEND", juce::dontSendNotification);
    pitchBendLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchBendLabel);

    // Mod Depth Slider
    modDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    modDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    modDepthSlider.setRange(0.0, 1.0, 0.001);
    modDepthSlider.setValue(0.0);
    modDepthSlider.getProperties().set("performanceWheel", true);
    modDepthSlider.setSliderSnapsToMousePosition(false);
    modDepthSlider.setPopupDisplayEnabled(true, true, this);
    modDepthSlider.setDoubleClickReturnValue(true, 0.0);
    modDepthSlider.addListener(this);
    addAndMakeVisible(modDepthSlider);
    modDepthLabel.setText("MOD", juce::dontSendNotification);
    modDepthLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modDepthLabel);

    // LFO Target Buttons
    lfoTargetParam = processor.getValueTreeState().getParameter(ParameterIds::lfoTarget);
    jassert(lfoTargetParam != nullptr);
    addAndMakeVisible(lfoTargetLabel);
    lfoTargetLabel.setText("TARGET", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(lfoTargetParam)) {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i) {
            auto* button = lfoTargetButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(3);  // Group ID for LFO Target
            button->setClickingTogglesState(true);
            button->onClick = [choiceParam, i] { *choiceParam = i; };
        }
    }
    lfoTargetParam->addListener(this);

    // Initial update
    parameterValueChanged(lfoTargetParam->getParameterIndex(), lfoTargetParam->getValue());
}

ModulationComponent::~ModulationComponent() {
    pitchBendSlider.removeListener(this);
    modDepthSlider.removeListener(this);
    if (lfoTargetParam)
        lfoTargetParam->removeListener(this);
}

void ModulationComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "CONTROL");
}

void ModulationComponent::sliderValueChanged(juce::Slider* slider) {
    if (slider == &pitchBendSlider) {
        // Pitch bend value is from 0.0 to 1.0. MIDI pitch wheel is 14-bit (0-16383).
        // We map our range to the upper half of the MIDI pitch wheel range (8192-16383).
        int pitchWheelValue = static_cast<int>(8192 + slider->getValue() * 8191.0);
        auto message = juce::MidiMessage::pitchWheel(1, pitchWheelValue);
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getMidiMessageCollector().addMessageToQueue(message);
    } else if (slider == &modDepthSlider) {
        // Mod depth is 0.0 to 1.0. MIDI CC is 0-127.
        int controllerValue = static_cast<int>(slider->getValue() * 127);
        auto message = juce::MidiMessage::controllerEvent(1, 1, controllerValue);  // CC #1
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getMidiMessageCollector().addMessageToQueue(message);
    }
}

void ModulationComponent::resized() {
    auto bounds = getLocalBounds().reduced(5).withTrimmedTop(24);
    auto targets = bounds.removeFromRight(76);
    lfoTargetLabel.setBounds(targets.removeFromTop(22));
    for (auto* button : lfoTargetButtons)
        button->setBounds(targets.removeFromTop(28));

    bounds.removeFromRight(6);
    auto bend = bounds.removeFromLeft(bounds.getWidth() / 2);
    pitchBendLabel.setBounds(bend.removeFromBottom(24));
    modDepthLabel.setBounds(bounds.removeFromBottom(24));
    pitchBendSlider.setBounds(bend.reduced(3, 0));
    modDepthSlider.setBounds(bounds.reduced(3, 0));
}

void ModulationComponent::parameterValueChanged(int parameterIndex, float newValue) {
    if (parameterIndex == lfoTargetParam->getParameterIndex()) {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(lfoTargetParam)) {
            lfoTargetButtons[choiceParam->getIndex()]->setToggleState(true,
                                                                      juce::dontSendNotification);
        }
    }
}

void ModulationComponent::parameterGestureChanged(int parameterIndex, bool gestureIsStarting) {
    // Not needed for this component
}
