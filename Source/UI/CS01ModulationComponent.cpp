#include "CS01ModulationComponent.h"
#include "../CS01AudioProcessor.h"
#include "../Parameters.h"

CS01ModulationComponent::CS01ModulationComponent(CS01AudioProcessor& p) : processor(p)
{
    // Pitch Bend Slider
    pitchBendSlider.setSliderStyle(juce::Slider::LinearVertical);
    pitchBendSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    pitchBendSlider.setRange(0.0, 1.0, 0.001);
    pitchBendSlider.setValue(0.0);
    pitchBendSlider.setDoubleClickReturnValue(true, 0.0);
    pitchBendSlider.addListener(this);
    addAndMakeVisible(pitchBendSlider);
    pitchBendLabel.setText("BEND", juce::dontSendNotification);
    addAndMakeVisible(pitchBendLabel);

    // Mod Depth Slider
    modDepthSlider.setSliderStyle(juce::Slider::LinearVertical);
    modDepthSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    modDepthSlider.setRange(0.0, 1.0, 0.001);
    modDepthSlider.setValue(0.0);
    modDepthSlider.addListener(this);
    addAndMakeVisible(modDepthSlider);
    modDepthLabel.setText("MOD", juce::dontSendNotification);
    addAndMakeVisible(modDepthLabel);

    // LFO Target Buttons
    lfoTargetParam = processor.getValueTreeState().getParameter(ParameterIds::lfoTarget);
    jassert(lfoTargetParam != nullptr);
    addAndMakeVisible(lfoTargetLabel);
    lfoTargetLabel.setText("TARGET", juce::dontSendNotification);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(lfoTargetParam))
    {
        auto choices = choiceParam->choices;
        for (int i = 0; i < choices.size(); ++i)
        {
            auto* button = lfoTargetButtons.add(new juce::ToggleButton(choices[i]));
            addAndMakeVisible(button);
            button->setRadioGroupId(3); // Group ID for LFO Target
            button->setClickingTogglesState(true);
            button->onClick = [this, choiceParam, i] { *choiceParam = i; };
        }
    }
    lfoTargetParam->addListener(this);
    
    // Initial update
    parameterValueChanged(lfoTargetParam->getParameterIndex(), lfoTargetParam->getValue());
}

CS01ModulationComponent::~CS01ModulationComponent()
{
    pitchBendSlider.removeListener(this);
    modDepthSlider.removeListener(this);
    if (lfoTargetParam) lfoTargetParam->removeListener(this);
}

void CS01ModulationComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("Modulation", getLocalBounds(), juce::Justification::centredTop, 1);
}

void CS01ModulationComponent::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &pitchBendSlider)
    {
        // Pitch bend value is from 0.0 to 1.0. MIDI pitch wheel is 14-bit (0-16383).
        // We map our range to the upper half of the MIDI pitch wheel range (8192-16383).
        int pitchWheelValue = static_cast<int>(8192 + slider->getValue() * 8191.0);
        auto message = juce::MidiMessage::pitchWheel(1, pitchWheelValue);
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getMidiMessageCollector().addMessageToQueue(message);
    }
    else if (slider == &modDepthSlider)
    {
        // Mod depth is 0.0 to 1.0. MIDI CC is 0-127.
        int controllerValue = static_cast<int>(slider->getValue() * 127);
        auto message = juce::MidiMessage::controllerEvent(1, 1, controllerValue); // CC #1
        message.setTimeStamp(juce::Time::getMillisecondCounterHiRes() * 0.001);
        processor.getMidiMessageCollector().addMessageToQueue(message);
    }
}

void CS01ModulationComponent::resized()
{
    auto bounds = getLocalBounds().reduced(5).withTrimmedTop(20);
    
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    flex.alignItems = juce::FlexBox::AlignItems::stretch;

    auto sliderWidth = bounds.getWidth() / 4;
    auto buttonGroupWidth = bounds.getWidth() / 2;

    // Calculate slider height (to match other components)
    float totalHeight = bounds.getHeight();
    float labelHeight = 15.0f;
    float sliderHeight = (totalHeight - labelHeight) * 5.0f / 6.0f;

    juce::FlexBox bendBox;
    bendBox.flexDirection = juce::FlexBox::Direction::column;
    bendBox.items.add(juce::FlexItem(pitchBendSlider).withHeight(sliderHeight));
    bendBox.items.add(juce::FlexItem(pitchBendLabel).withHeight(labelHeight));
    flex.items.add(juce::FlexItem(bendBox).withWidth(sliderWidth));

    juce::FlexBox modBox;
    modBox.flexDirection = juce::FlexBox::Direction::column;
    modBox.items.add(juce::FlexItem(modDepthSlider).withHeight(sliderHeight));
    modBox.items.add(juce::FlexItem(modDepthLabel).withHeight(labelHeight));
    flex.items.add(juce::FlexItem(modBox).withWidth(sliderWidth));

    juce::FlexBox targetBox;
    targetBox.flexDirection = juce::FlexBox::Direction::column;
    targetBox.items.add(juce::FlexItem(lfoTargetLabel).withHeight(15.0f));
    for (auto* button : lfoTargetButtons)
        targetBox.items.add(juce::FlexItem(*button).withFlex(1.0f));
    flex.items.add(juce::FlexItem(targetBox).withWidth(buttonGroupWidth));

    flex.performLayout(bounds);
}

void CS01ModulationComponent::parameterValueChanged(int parameterIndex, float newValue)
{
    if (parameterIndex == lfoTargetParam->getParameterIndex())
    {
        if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(lfoTargetParam))
        {
            lfoTargetButtons[choiceParam->getIndex()]->setToggleState(true, juce::dontSendNotification);
        }
    }
}

void CS01ModulationComponent::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    // Not needed for this component
}
