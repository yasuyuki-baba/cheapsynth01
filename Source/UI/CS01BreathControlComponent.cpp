#include "CS01BreathControlComponent.h"
#include "../Parameters.h"

CS01BreathControlComponent::CS01BreathControlComponent(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
{
    breathVcfSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    breathVcfSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(breathVcfSlider);
    breathVcfLabel.setText("VCF", juce::dontSendNotification);
    addAndMakeVisible(breathVcfLabel);
    breathVcfAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::breathVcf), breathVcfSlider);

    breathVcaSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    breathVcaSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(breathVcaSlider);
    breathVcaLabel.setText("VCA", juce::dontSendNotification);
    addAndMakeVisible(breathVcaLabel);
    breathVcaAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::breathVca), breathVcaSlider);
}

CS01BreathControlComponent::~CS01BreathControlComponent() {}

void CS01BreathControlComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("Breath Control", getLocalBounds(), juce::Justification::centredTop, 1);
}

void CS01BreathControlComponent::resized()
{
    auto bounds = getLocalBounds().withTrimmedTop(20);
    
    juce::FlexBox flexbox;
    flexbox.flexDirection = juce::FlexBox::Direction::column;
    flexbox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    flexbox.alignItems = juce::FlexBox::AlignItems::stretch;

    flexbox.items.add(juce::FlexItem(breathVcfSlider).withFlex(1.0f));
    flexbox.items.add(juce::FlexItem(breathVcfLabel).withHeight(15.0f));
    flexbox.items.add(juce::FlexItem(breathVcaSlider).withFlex(1.0f));
    flexbox.items.add(juce::FlexItem(breathVcaLabel).withHeight(15.0f));

    flexbox.performLayout(bounds);
}
