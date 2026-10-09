#include "UI/BreathControlComponent.h"

#include "Parameters.h"
#include "UI/CS01LookAndFeel.h"

BreathControlComponent::BreathControlComponent(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts) {
    breathVcfSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    breathVcfSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(breathVcfSlider);
    breathVcfLabel.setText("VCF", juce::dontSendNotification);
    addAndMakeVisible(breathVcfLabel);
    breathVcfAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::breathVcf), breathVcfSlider);

    breathVcaSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    breathVcaSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(breathVcaSlider);
    breathVcaLabel.setText("VCA", juce::dontSendNotification);
    addAndMakeVisible(breathVcaLabel);
    breathVcaAttachment = std::make_unique<CS01SliderParameterAttachment>(
        *valueTreeState.getParameter(ParameterIds::breathVca), breathVcaSlider);
    for (auto* slider : {&breathVcfSlider, &breathVcaSlider}) {
        slider->setPopupDisplayEnabled(true, true, this);
        slider->setTooltip("Breath control depth; percentage represents the control amount.");
    }
}

BreathControlComponent::~BreathControlComponent() {}

void BreathControlComponent::paint(juce::Graphics& g) {
    CS01LookAndFeel::drawSectionBackground(g, getLocalBounds(), "BREATH");
}

void BreathControlComponent::resized() {
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
    for (auto* slider : {&breathVcfSlider, &breathVcaSlider}) {
        auto area = slider->getBounds();
        const int size = juce::jmin(60, area.getWidth(), area.getHeight());
        slider->setBounds(area.withSizeKeepingCentre(size, size));
    }
}
